#include <paperweight/generator.hpp>

#include <paperweight/evaluation.hpp>
#include <paperweight/noise.hpp>

#include "graph_evaluator.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <limits>
#if PAPERWEIGHT_ENABLE_THREADS
#include <mutex>
#endif
#include <optional>
#if PAPERWEIGHT_ENABLE_THREADS
#include <thread>
#endif
#include <type_traits>
#include <vector>

namespace paperweight {
namespace {

constexpr std::uint32_t maximumDimension = 4096;
#if PAPERWEIGHT_ENABLE_THREADS
constexpr std::uint32_t maximumWorkerCount = 32;
constexpr std::uint64_t automaticParallelPixelThreshold = 32U * 1024U;
#endif

class CancellationState {
public:
    explicit CancellationState(const GenerationCancellationCheck& check)
        : check_(check)
    {
    }

    bool cancelled()
    {
        if (stopped_.load(std::memory_order_relaxed)) {
            return true;
        }
        if (!check_) {
            return false;
        }
#if PAPERWEIGHT_ENABLE_THREADS
        const std::lock_guard lock(checkMutex_);
#endif
        if (stopped_.load(std::memory_order_relaxed)) {
            return true;
        }
        if (check_()) {
            stopped_.store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    void requestStop() noexcept
    {
        stopped_.store(true, std::memory_order_relaxed);
    }

    bool stopped() const noexcept
    {
        return stopped_.load(std::memory_order_relaxed);
    }

private:
    const GenerationCancellationCheck& check_;
    std::atomic_bool stopped_{false};
#if PAPERWEIGHT_ENABLE_THREADS
    std::mutex checkMutex_;
#endif
};

std::uint32_t resolvedWorkerCount(const GenerationRequest& request)
{
    std::uint32_t count = request.workerCount;
#if PAPERWEIGHT_ENABLE_THREADS
    const auto pixels = static_cast<std::uint64_t>(request.width) * request.height;
    if (count == 0) {
        count = pixels < automaticParallelPixelThreshold
            ? 1U
            : std::max(1U, std::thread::hardware_concurrency());
    }
    return std::min({count, maximumWorkerCount, request.height});
#else
    static_cast<void>(count);
    return 1;
#endif
}

template<typename Function>
bool forEachRow(
    std::uint32_t height,
    std::uint32_t workerCount,
    CancellationState& cancellation,
    Function&& function)
{
    if (workerCount <= 1) {
        for (std::uint32_t y = 0; y < height; ++y) {
            if (cancellation.cancelled()) {
                return false;
            }
            function(0, y);
        }
        return true;
    }

#if PAPERWEIGHT_ENABLE_THREADS
    std::atomic<std::uint32_t> nextRow{0};
    std::exception_ptr failure;
    std::mutex failureMutex;
    {
        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
            workers.emplace_back([&, workerIndex]() {
                try {
                    while (!cancellation.cancelled()) {
                        const auto y = nextRow.fetch_add(1, std::memory_order_relaxed);
                        if (y >= height) {
                            return;
                        }
                        function(workerIndex, y);
                    }
                } catch (...) {
                    const std::lock_guard lock(failureMutex);
                    if (failure == nullptr) {
                        failure = std::current_exception();
                    }
                    cancellation.requestStop();
                }
            });
        }
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
    return !cancellation.stopped();
#else
    static_cast<void>(function);
    return false;
#endif
}

std::uint8_t toUnorm8(double value)
{
    return static_cast<std::uint8_t>(std::round(std::clamp(value, 0.0, 1.0) * 255.0));
}

std::uint8_t signedToUnorm8(double value)
{
    return toUnorm8(std::clamp(value, -1.0, 1.0) * 0.5 + 0.5);
}

Rgba8 encodeColour(const EvaluatedSample& sample)
{
    return {
        toUnorm8(sample.red),
        toUnorm8(sample.green),
        toUnorm8(sample.blue),
        toUnorm8(sample.alpha),
    };
}

Rgba8 encodeScalar(double value)
{
    const auto byte = toUnorm8(value);
    return {byte, byte, byte, 255};
}

Rgba8 encodeNormal(double derivativeU, double derivativeV, double strength)
{
    double x = -derivativeU * strength;
    double y = -derivativeV * strength;
    double z = 1.0;
    const double inverseLength = 1.0 / std::sqrt(x * x + y * y + z * z);
    x *= inverseLength;
    y *= inverseLength;
    z *= inverseLength;
    return {signedToUnorm8(x), signedToUnorm8(y), signedToUnorm8(z), 255};
}

std::optional<std::uint32_t> exactRepeatCount(double coverage, double materialExtent)
{
    if (!std::isfinite(coverage) || !std::isfinite(materialExtent) || coverage <= 0.0 ||
        materialExtent <= 0.0) {
        return std::nullopt;
    }
    const double repeats = coverage / materialExtent;
    const double rounded = std::round(repeats);
    const double tolerance = 1.0e-9 * std::max(1.0, std::abs(repeats));
    if (std::abs(repeats - rounded) > tolerance || rounded < 1.0 ||
        rounded > PhysicalLimits::maximumRepeats) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(rounded);
}

std::optional<std::string> validateGraphPhysicalScale(
    const Material& material,
    const MaterialGraph& graph)
{
    for (const auto& node : graph.nodes) {
        const auto* generator = std::get_if<GeneratorNode>(&node);
        if (generator == nullptr) {
            continue;
        }
        const auto* brick = std::get_if<BrickGridOperation>(&generator->operation);
        if (brick == nullptr || !brick->physicalDimensions) {
            continue;
        }
        Material probe = material;
        probe.layers.clear();
        auto layer = makeBrickGridLayer();
        layer.operation = *brick;
        probe.layers.push_back(layer);
        if (const auto error = validateMaterial(probe)) {
            return "graph node " + std::to_string(generator->id) + ": " + *error;
        }
    }
    return std::nullopt;
}

} // namespace

GenerationResult generate(const GenerationRequest& request)
{
    return generate(request, {});
}

GenerationResult generate(
    const GenerationRequest& request,
    const GenerationCancellationCheck& isCancelled)
{
    CancellationState cancellation(isCancelled);
    const auto cancellationError = []() {
        return GenerationError{
            GenerationErrorCode::cancelled,
            "generation was cancelled",
        };
    };

    if (cancellation.cancelled()) {
        return cancellationError();
    }
    if (request.width == 0 || request.height == 0 || request.width > maximumDimension ||
        request.height > maximumDimension) {
        return GenerationError{
            GenerationErrorCode::invalidDimensions,
            "output dimensions must be between 1 and 4096 pixels",
        };
    }

    switch (request.output) {
    case MaterialOutput::colour:
    case MaterialOutput::height:
    case MaterialOutput::normal:
    case MaterialOutput::roughness:
        break;
    default:
        return GenerationError{
            GenerationErrorCode::invalidOutput,
            "the requested material output is not supported",
        };
    }

    MaterialGraph compiledGraph;
    const MaterialGraph* graph = nullptr;
    if (request.graph) {
        if (const auto error = validateMaterialSettings(request.material)) {
            return GenerationError{GenerationErrorCode::invalidMaterial, *error};
        }
        if (const auto error = validateMaterialGraph(*request.graph)) {
            return GenerationError{GenerationErrorCode::invalidGraph, error->message};
        }
        if (const auto error = validateGraphPhysicalScale(request.material, *request.graph)) {
            return GenerationError{GenerationErrorCode::invalidGraph, *error};
        }
        graph = &*request.graph;
    } else {
        if (const auto error = validateMaterial(request.material)) {
            return GenerationError{GenerationErrorCode::invalidMaterial, *error};
        }
        auto compilation = compileMaterialGraph(request.material);
        if (const auto* error = std::get_if<GraphError>(&compilation)) {
            return GenerationError{GenerationErrorCode::invalidGraph, error->message};
        }
        compiledGraph = std::get<MaterialGraph>(std::move(compilation));
        graph = &compiledGraph;
    }

    const PhysicalSize coverage = request.physicalCoverage.value_or(
        request.material.physicalSize);
    const auto horizontalRepeats = exactRepeatCount(
        coverage.widthMetres,
        request.material.physicalSize.widthMetres);
    const auto verticalRepeats = exactRepeatCount(
        coverage.heightMetres,
        request.material.physicalSize.heightMetres);
    if (!horizontalRepeats || !verticalRepeats) {
        return GenerationError{
            GenerationErrorCode::invalidPhysicalCoverage,
            "physical coverage must be finite, positive, and contain 1 to 4096 whole material repeats on each axis",
        };
    }

    try {
        const auto workerCount = resolvedWorkerCount(request);
        std::vector<detail::GraphEvaluator> evaluators;
        evaluators.reserve(workerCount);
        for (std::uint32_t worker = 0; worker < workerCount; ++worker) {
            evaluators.emplace_back(request.material, *graph);
        }
        Image image(request.width, request.height);
        if (request.output == MaterialOutput::normal) {
            std::vector<double> heights(
                static_cast<std::size_t>(request.width) * request.height);
            const bool heightsComplete = forEachRow(
                request.height,
                workerCount,
                cancellation,
                [&](std::uint32_t worker, std::uint32_t y) {
                const double v =
                    (static_cast<double>(y) + 0.5) / request.height * *verticalRepeats;
                for (std::uint32_t x = 0; x < request.width; ++x) {
                    const double u =
                        (static_cast<double>(x) + 0.5) / request.width * *horizontalRepeats;
                    heights[static_cast<std::size_t>(y) * request.width + x] =
                        evaluators[worker].evaluate(MaterialOutput::normal, u, v).scalar;
                }
            });
            if (!heightsComplete) {
                return cancellationError();
            }

            const auto heightAt = [&](std::uint32_t x, std::uint32_t y) {
                return heights[static_cast<std::size_t>(y) * request.width + x];
            };
            const bool normalsComplete = forEachRow(
                request.height,
                workerCount,
                cancellation,
                [&](std::uint32_t, std::uint32_t y) {
                auto row = image.row(y);
                const auto previousY = y == 0 ? request.height - 1 : y - 1;
                const auto nextY = y + 1 == request.height ? 0 : y + 1;
                for (std::uint32_t x = 0; x < request.width; ++x) {
                    const auto previousX = x == 0 ? request.width - 1 : x - 1;
                    const auto nextX = x + 1 == request.width ? 0 : x + 1;
                    const double derivativeU =
                        (heightAt(nextX, y) - heightAt(previousX, y)) *
                        static_cast<double>(request.width) /
                        coverage.widthMetres * 0.5;
                    const double derivativeV =
                        (heightAt(x, nextY) - heightAt(x, previousY)) *
                        static_cast<double>(request.height) /
                        coverage.heightMetres * 0.5;
                    row[x] = encodeNormal(
                        derivativeU,
                        derivativeV,
                        request.material.normalStrength);
                }
            });
            if (!normalsComplete) {
                return cancellationError();
            }
            return image;
        }

        const bool generationComplete = forEachRow(
            request.height,
            workerCount,
            cancellation,
            [&](std::uint32_t worker, std::uint32_t y) {
            auto row = image.row(y);
            const double v =
                (static_cast<double>(y) + 0.5) / request.height * *verticalRepeats;
            for (std::uint32_t x = 0; x < request.width; ++x) {
                const double u =
                    (static_cast<double>(x) + 0.5) / request.width * *horizontalRepeats;
                const auto sample = evaluators[worker].evaluate(request.output, u, v);
                switch (request.output) {
                case MaterialOutput::colour:
                    row[x] = encodeColour(sample);
                    break;
                case MaterialOutput::height:
                    row[x] = encodeScalar(sample.scalar);
                    break;
                case MaterialOutput::roughness:
                    row[x] = encodeScalar(
                        request.material.roughnessLow +
                        (request.material.roughnessHigh - request.material.roughnessLow) *
                            sample.scalar);
                    break;
                case MaterialOutput::normal:
                    break;
                }
            }
        });
        if (!generationComplete) {
            return cancellationError();
        }
        return image;
    } catch (const std::exception& exception) {
        return GenerationError{GenerationErrorCode::allocationFailure, exception.what()};
    }
}

} // namespace paperweight
