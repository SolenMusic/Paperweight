#include <paperweight/generator.hpp>

#include <paperweight/evaluation.hpp>
#include <paperweight/noise.hpp>

#include "graph_evaluator.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <vector>

namespace paperweight {
namespace {

constexpr std::uint32_t maximumDimension = 4096;

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

} // namespace

GenerationResult generate(const GenerationRequest& request)
{
    return generate(request, {});
}

GenerationResult generate(
    const GenerationRequest& request,
    const GenerationCancellationCheck& isCancelled)
{
    const auto cancelled = [&isCancelled]() {
        return isCancelled && isCancelled();
    };
    const auto cancellationError = []() {
        return GenerationError{
            GenerationErrorCode::cancelled,
            "generation was cancelled",
        };
    };

    if (cancelled()) {
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

    try {
        detail::GraphEvaluator evaluator(request.material, *graph);
        Image image(request.width, request.height);
        if (request.output == MaterialOutput::normal) {
            std::vector<double> heights(
                static_cast<std::size_t>(request.width) * request.height);
            for (std::uint32_t y = 0; y < request.height; ++y) {
                if (cancelled()) {
                    return cancellationError();
                }
                const double v = (static_cast<double>(y) + 0.5) / request.height;
                for (std::uint32_t x = 0; x < request.width; ++x) {
                    const double u = (static_cast<double>(x) + 0.5) / request.width;
                    heights[static_cast<std::size_t>(y) * request.width + x] =
                        evaluator.evaluate(MaterialOutput::normal, u, v).scalar;
                }
            }

            const auto heightAt = [&](std::uint32_t x, std::uint32_t y) {
                return heights[static_cast<std::size_t>(y) * request.width + x];
            };
            for (std::uint32_t y = 0; y < request.height; ++y) {
                if (cancelled()) {
                    return cancellationError();
                }
                auto row = image.row(y);
                const auto previousY = y == 0 ? request.height - 1 : y - 1;
                const auto nextY = y + 1 == request.height ? 0 : y + 1;
                for (std::uint32_t x = 0; x < request.width; ++x) {
                    const auto previousX = x == 0 ? request.width - 1 : x - 1;
                    const auto nextX = x + 1 == request.width ? 0 : x + 1;
                    const double derivativeU =
                        (heightAt(nextX, y) - heightAt(previousX, y)) *
                        static_cast<double>(request.width) * 0.5;
                    const double derivativeV =
                        (heightAt(x, nextY) - heightAt(x, previousY)) *
                        static_cast<double>(request.height) * 0.5;
                    row[x] = encodeNormal(
                        derivativeU,
                        derivativeV,
                        request.material.normalStrength);
                }
            }
            return image;
        }

        for (std::uint32_t y = 0; y < request.height; ++y) {
            if (cancelled()) {
                return cancellationError();
            }
            auto row = image.row(y);
            const double v = (static_cast<double>(y) + 0.5) / request.height;
            for (std::uint32_t x = 0; x < request.width; ++x) {
                const double u = (static_cast<double>(x) + 0.5) / request.width;
                const auto sample = evaluator.evaluate(request.output, u, v);
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
                default:
                    return GenerationError{
                        GenerationErrorCode::invalidOutput,
                        "the requested material output is not supported"};
                }
            }
        }
        return image;
    } catch (const std::exception& exception) {
        return GenerationError{GenerationErrorCode::allocationFailure, exception.what()};
    }
}

} // namespace paperweight
