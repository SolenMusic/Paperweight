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

std::uint32_t resolvedWorkerCount(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t requestedCount)
{
    std::uint32_t count = requestedCount;
#if PAPERWEIGHT_ENABLE_THREADS
    const auto pixels = static_cast<std::uint64_t>(width) * height;
    if (count == 0) {
        count = pixels < automaticParallelPixelThreshold
            ? 1U
            : std::max(1U, std::thread::hardware_concurrency());
    }
    return std::min({count, maximumWorkerCount, height});
#else
    static_cast<void>(width);
    static_cast<void>(height);
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
        if (brick != nullptr && brick->physicalDimensions) {
            Material probe = material;
            probe.layers.clear();
            auto layer = makeBrickGridLayer();
            layer.operation = *brick;
            probe.layers.push_back(layer);
            if (const auto error = validateMaterial(probe)) {
                return "graph node " + std::to_string(generator->id) + ": " + *error;
            }
        }
        const auto* course = std::get_if<CourseLayoutOperation>(&generator->operation);
        if (course != nullptr && course->physicalDimensions) {
            Material probe = material;
            probe.layers.clear();
            auto layer = makeCourseLayoutLayer();
            layer.operation = *course;
            probe.layers.push_back(layer);
            if (const auto error = validateMaterial(probe)) {
                return "graph node " + std::to_string(generator->id) + ": " + *error;
            }
        }
    }
    return std::nullopt;
}

std::optional<Rgba8> constantOutputPixel(
    MaterialOutput output,
    const Material& material)
{
    const auto constantScalar = [](double low, double high) -> std::optional<Rgba8> {
        return low == high ? std::optional<Rgba8>{encodeScalar(low)} : std::nullopt;
    };
    switch (output) {
    case MaterialOutput::roughness:
        return constantScalar(material.roughnessLow, material.roughnessHigh);
    case MaterialOutput::metalness:
        return constantScalar(material.metalnessLow, material.metalnessHigh);
    case MaterialOutput::coating:
        return constantScalar(material.coatingLow, material.coatingHigh);
    case MaterialOutput::occlusion:
        return constantScalar(material.occlusionLow, material.occlusionHigh);
    case MaterialOutput::clearCoat:
        return constantScalar(material.clearCoatLow, material.clearCoatHigh);
    case MaterialOutput::clearCoatRoughness:
        return constantScalar(
            material.clearCoatRoughnessLow,
            material.clearCoatRoughnessHigh);
    case MaterialOutput::colour:
    case MaterialOutput::height:
    case MaterialOutput::normal:
    case MaterialOutput::emissive:
        return std::nullopt;
    }
    return std::nullopt;
}

bool normalEvaluationMatchesHeight(const MaterialGraph& graph)
{
    GraphNodeId heightInput = invalidGraphNodeId;
    GraphNodeId normalInput = invalidGraphNodeId;
    for (const auto& node : graph.nodes) {
        if (const auto* output = std::get_if<OutputNode>(&node)) {
            if (output->output == MaterialOutput::height) {
                heightInput = output->input;
            } else if (output->output == MaterialOutput::normal) {
                normalInput = output->input;
            }
            continue;
        }
        const auto* processing = std::get_if<ProcessingNode>(&node);
        if (processing == nullptr) {
            continue;
        }
        if (const auto* surface =
                std::get_if<RegionSurfaceProcessing>(&processing->operation);
            surface != nullptr && surface->parameters.facetedNormals) {
            return false;
        }
    }
    return heightInput != invalidGraphNodeId && heightInput == normalInput;
}

Rgba8 encodeOutput(
    MaterialOutput output,
    const Material& material,
    const EvaluatedSample& sample)
{
    switch (output) {
    case MaterialOutput::colour:
        return encodeColour(sample);
    case MaterialOutput::height:
        return encodeScalar(sample.scalar);
    case MaterialOutput::roughness:
        return encodeScalar(
            material.roughnessLow +
            (material.roughnessHigh - material.roughnessLow) * sample.scalar);
    case MaterialOutput::metalness:
        return encodeScalar(
            material.metalnessLow +
            (material.metalnessHigh - material.metalnessLow) * sample.scalar);
    case MaterialOutput::coating:
        return encodeScalar(
            material.coatingLow +
            (material.coatingHigh - material.coatingLow) * sample.scalar);
    case MaterialOutput::occlusion:
        return encodeScalar(
            material.occlusionLow +
            (material.occlusionHigh - material.occlusionLow) * sample.scalar);
    case MaterialOutput::clearCoat:
        return encodeScalar(
            material.clearCoatLow +
            (material.clearCoatHigh - material.clearCoatLow) * sample.scalar);
    case MaterialOutput::clearCoatRoughness:
        return encodeScalar(
            material.clearCoatRoughnessLow +
            (material.clearCoatRoughnessHigh - material.clearCoatRoughnessLow) *
                sample.scalar);
    case MaterialOutput::emissive: {
        auto colour = encodeColour(sample);
        colour.red = toUnorm8(sample.red * material.emissiveIntensity);
        colour.green = toUnorm8(sample.green * material.emissiveIntensity);
        colour.blue = toUnorm8(sample.blue * material.emissiveIntensity);
        return colour;
    }
    case MaterialOutput::normal:
        break;
    }
    return {};
}

} // namespace

namespace {

MaterialSetResult generateMaterialSetImpl(
    const Material& material,
    std::uint32_t width,
    std::uint32_t height,
    const MaterialOutputSelection& selectedOutputs,
    const std::optional<MaterialGraph>& requestedGraph,
    const std::optional<PhysicalSize>& requestedCoverage,
    std::uint32_t requestedWorkerCount,
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
    if (width == 0 || height == 0 || width > maximumDimension ||
        height > maximumDimension) {
        return GenerationError{
            GenerationErrorCode::invalidDimensions,
            "output dimensions must be between 1 and 4096 pixels",
        };
    }
    if (std::none_of(selectedOutputs.begin(), selectedOutputs.end(), [](bool value) {
            return value;
        })) {
        return GenerationError{
            GenerationErrorCode::invalidOutput,
            "at least one material output must be selected",
        };
    }

    MaterialGraph compiledGraph;
    const MaterialGraph* graph = nullptr;
    if (requestedGraph) {
        if (const auto error = validateMaterialSettings(material)) {
            return GenerationError{GenerationErrorCode::invalidMaterial, *error};
        }
        if (const auto error = validateMaterialGraph(*requestedGraph)) {
            return GenerationError{GenerationErrorCode::invalidGraph, error->message};
        }
        if (const auto error = validateGraphPhysicalScale(material, *requestedGraph)) {
            return GenerationError{GenerationErrorCode::invalidGraph, *error};
        }
        graph = &*requestedGraph;
    } else {
        if (const auto error = validateMaterial(material)) {
            return GenerationError{GenerationErrorCode::invalidMaterial, *error};
        }
        auto compilation = compileMaterialGraph(material);
        if (const auto* error = std::get_if<GraphError>(&compilation)) {
            return GenerationError{GenerationErrorCode::invalidGraph, error->message};
        }
        compiledGraph = std::get<MaterialGraph>(std::move(compilation));
        graph = &compiledGraph;
    }

    const PhysicalSize coverage = requestedCoverage.value_or(material.physicalSize);
    const auto horizontalRepeats = exactRepeatCount(
        coverage.widthMetres,
        material.physicalSize.widthMetres);
    const auto verticalRepeats = exactRepeatCount(
        coverage.heightMetres,
        material.physicalSize.heightMetres);
    if (!horizontalRepeats || !verticalRepeats) {
        return GenerationError{
            GenerationErrorCode::invalidPhysicalCoverage,
            "physical coverage must be finite, positive, and contain 1 to 4096 whole material repeats on each axis",
        };
    }

    try {
        const auto workerCount = resolvedWorkerCount(width, height, requestedWorkerCount);
        MaterialImageSet result;
        std::array<bool, materialOutputs.size()> needsEvaluation{};
        bool anyEvaluation = false;
        for (std::size_t index = 0; index < materialOutputs.size(); ++index) {
            if (!selectedOutputs[index]) {
                continue;
            }
            const auto output = materialOutputs[index];
            if (const auto constant = constantOutputPixel(output, material)) {
                result.images[index].emplace(width, height, *constant);
                continue;
            }
            result.images[index].emplace(width, height);
            needsEvaluation[index] = true;
            anyEvaluation = true;
        }
        if (!anyEvaluation) {
            return result;
        }

        std::vector<detail::GraphEvaluator> evaluators;
        evaluators.reserve(workerCount);
        evaluators.emplace_back(material, *graph);
        for (std::uint32_t worker = 1; worker < workerCount; ++worker) {
            evaluators.push_back(evaluators.front().cloneWorker());
        }
        std::vector<double> uCoordinates(width);
        for (std::uint32_t x = 0; x < width; ++x) {
            uCoordinates[x] =
                (static_cast<double>(x) + 0.5) / width * *horizontalRepeats;
        }

        const auto normalIndex = materialOutputIndex(MaterialOutput::normal);
        const auto heightIndex = materialOutputIndex(MaterialOutput::height);
        const bool normalRequested = selectedOutputs[normalIndex];
        const bool reuseNormalFieldForHeight = normalRequested && selectedOutputs[heightIndex] &&
            normalEvaluationMatchesHeight(*graph);
        if (reuseNormalFieldForHeight) {
            needsEvaluation[heightIndex] = false;
        }
        std::vector<double> normalField;
        if (normalRequested) {
            normalField.resize(static_cast<std::size_t>(width) * height);
        }

        const bool generationComplete = forEachRow(
            height,
            workerCount,
            cancellation,
            [&](std::uint32_t worker, std::uint32_t y) {
            const double v =
                (static_cast<double>(y) + 0.5) / height * *verticalRepeats;
            for (std::size_t outputIndex = 0;
                 outputIndex < materialOutputs.size();
                 ++outputIndex) {
                if (!needsEvaluation[outputIndex]) {
                    continue;
                }
                if (cancellation.cancelled()) {
                    return;
                }
                const auto output = materialOutputs[outputIndex];
                if (output == MaterialOutput::normal) {
                    for (std::uint32_t x = 0; x < width; ++x) {
                        normalField[static_cast<std::size_t>(y) * width + x] =
                            evaluators[worker].evaluate(
                                MaterialOutput::normal,
                                uCoordinates[x],
                                v).scalar;
                    }
                    continue;
                }
                auto row = result.images[outputIndex]->row(y);
                for (std::uint32_t x = 0; x < width; ++x) {
                    const auto sample = evaluators[worker].evaluate(
                        output,
                        uCoordinates[x],
                        v);
                    row[x] = encodeOutput(output, material, sample);
                }
            }
        });
        if (!generationComplete) {
            return cancellationError();
        }

        if (normalRequested) {
            const auto heightAt = [&](std::uint32_t x, std::uint32_t y) {
                return normalField[static_cast<std::size_t>(y) * width + x];
            };
            const bool normalsComplete = forEachRow(
                height,
                workerCount,
                cancellation,
                [&](std::uint32_t, std::uint32_t y) {
                auto normalRow = result.images[normalIndex]->row(y);
                std::span<Rgba8> heightRow;
                if (reuseNormalFieldForHeight) {
                    heightRow = result.images[heightIndex]->row(y);
                }
                const auto previousY = y == 0 ? height - 1 : y - 1;
                const auto nextY = y + 1 == height ? 0 : y + 1;
                for (std::uint32_t x = 0; x < width; ++x) {
                    const auto previousX = x == 0 ? width - 1 : x - 1;
                    const auto nextX = x + 1 == width ? 0 : x + 1;
                    const double derivativeU =
                        (heightAt(nextX, y) - heightAt(previousX, y)) *
                        static_cast<double>(width) /
                        coverage.widthMetres * 0.5 *
                        material.reliefDepthMetres.value_or(1.0);
                    const double derivativeV =
                        (heightAt(x, nextY) - heightAt(x, previousY)) *
                        static_cast<double>(height) /
                        coverage.heightMetres * 0.5 *
                        material.reliefDepthMetres.value_or(1.0);
                    normalRow[x] = encodeNormal(
                        derivativeU,
                        derivativeV,
                        material.normalStrength);
                    if (reuseNormalFieldForHeight) {
                        heightRow[x] = encodeScalar(heightAt(x, y));
                    }
                }
            });
            if (!normalsComplete) {
                return cancellationError();
            }
        }
        return result;
    } catch (const std::exception& exception) {
        return GenerationError{GenerationErrorCode::allocationFailure, exception.what()};
    }
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
    MaterialOutputSelection outputs{};
    const auto outputIndex = materialOutputIndex(request.output);
    if (outputIndex >= outputs.size()) {
        return GenerationError{
            GenerationErrorCode::invalidOutput,
            "the requested material output is not supported",
        };
    }
    outputs[outputIndex] = true;
    auto result = generateMaterialSetImpl(
        request.material,
        request.width,
        request.height,
        outputs,
        request.graph,
        request.physicalCoverage,
        request.workerCount,
        isCancelled);
    if (auto* error = std::get_if<GenerationError>(&result)) {
        return std::move(*error);
    }
    auto& images = std::get<MaterialImageSet>(result).images;
    return std::move(*images[outputIndex]);
}

MaterialSetResult generateMaterialSet(const MaterialSetRequest& request)
{
    return generateMaterialSet(request, {});
}

MaterialSetResult generateMaterialSet(
    const MaterialSetRequest& request,
    const GenerationCancellationCheck& isCancelled)
{
    return generateMaterialSetImpl(
        request.material,
        request.width,
        request.height,
        request.outputs,
        request.graph,
        request.physicalCoverage,
        request.workerCount,
        isCancelled);
}

MaterialOutputSelection affectedMaterialOutputs(
    const Material& before,
    const Material& after) noexcept
{
    MaterialOutputSelection affected{};
    const auto include = [&affected](MaterialOutput output) {
        affected[materialOutputIndex(output)] = true;
        if (output == MaterialOutput::height) {
            affected[materialOutputIndex(MaterialOutput::normal)] = true;
        }
    };
    const auto includeRouting = [&include](const LayerOutputRouting& routing) {
        for (const auto output : materialOutputs) {
            if (routing.includes(output)) {
                include(output);
            }
        }
    };

    if (before.seed != after.seed || before.frequency != after.frequency ||
        before.octaves != after.octaves || before.lacunarity != after.lacunarity ||
        before.gain != after.gain || before.physicalSize != after.physicalSize ||
        before.layers.size() != after.layers.size()) {
        return allMaterialOutputsSelected;
    }

    if (before.lowColour != after.lowColour || before.highColour != after.highColour) {
        include(MaterialOutput::colour);
        include(MaterialOutput::emissive);
    }
    if (before.normalStrength != after.normalStrength ||
        before.reliefDepthMetres != after.reliefDepthMetres) {
        include(MaterialOutput::normal);
    }
    if (before.roughnessLow != after.roughnessLow ||
        before.roughnessHigh != after.roughnessHigh) {
        include(MaterialOutput::roughness);
    }
    if (before.metalnessLow != after.metalnessLow ||
        before.metalnessHigh != after.metalnessHigh) {
        include(MaterialOutput::metalness);
    }
    if (before.coatingLow != after.coatingLow ||
        before.coatingHigh != after.coatingHigh) {
        include(MaterialOutput::coating);
    }
    if (before.occlusionLow != after.occlusionLow ||
        before.occlusionHigh != after.occlusionHigh) {
        include(MaterialOutput::occlusion);
    }
    if (before.clearCoatLow != after.clearCoatLow ||
        before.clearCoatHigh != after.clearCoatHigh) {
        include(MaterialOutput::clearCoat);
    }
    if (before.clearCoatRoughnessLow != after.clearCoatRoughnessLow ||
        before.clearCoatRoughnessHigh != after.clearCoatRoughnessHigh) {
        include(MaterialOutput::clearCoatRoughness);
    }
    if (before.emissiveIntensity != after.emissiveIntensity) {
        include(MaterialOutput::emissive);
    }

    for (std::size_t index = 0; index < before.layers.size(); ++index) {
        if (before.layers[index] != after.layers[index]) {
            includeRouting(before.layers[index].outputs);
            includeRouting(after.layers[index].outputs);
        }
    }
    return affected;
}

} // namespace paperweight
