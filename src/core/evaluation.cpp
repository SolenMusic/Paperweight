#include <paperweight/evaluation.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/noise.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace paperweight {
namespace {

template<class... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

double channelToUnit(std::uint8_t channel)
{
    return static_cast<double>(channel) / 255.0;
}

EvaluatedSample sampleFromColour(const Rgba8& colour)
{
    const double red = channelToUnit(colour.red);
    const double green = channelToUnit(colour.green);
    const double blue = channelToUnit(colour.blue);
    return {
        0.2126 * red + 0.7152 * green + 0.0722 * blue,
        red,
        green,
        blue,
        channelToUnit(colour.alpha),
    };
}

EvaluatedSample noiseSample(const EvaluationContext& context, std::uint64_t seedOffset)
{
    const auto seed = seedOffset == 0
        ? context.material.seed
        : mixBits(context.material.seed ^ seedOffset);
    const double scalar = periodicFbm2D(
        context.u,
        context.v,
        context.material,
        seed);
    const auto low = sampleFromColour(context.material.lowColour);
    const auto high = sampleFromColour(context.material.highColour);
    const auto interpolate = [scalar](double from, double to) {
        return from + (to - from) * scalar;
    };
    return {
        scalar,
        interpolate(low.red, high.red),
        interpolate(low.green, high.green),
        interpolate(low.blue, high.blue),
        interpolate(low.alpha, high.alpha),
    };
}

double applyLevels(double value, const LevelsOperation& levels)
{
    const double normalised = std::clamp(
        (value - levels.inputLow) / (levels.inputHigh - levels.inputLow),
        0.0,
        1.0);
    return std::pow(normalised, 1.0 / levels.gamma);
}

EvaluatedSample levelsSample(const EvaluatedSample& input, const LevelsOperation& levels)
{
    return {
        applyLevels(input.scalar, levels),
        applyLevels(input.red, levels),
        applyLevels(input.green, levels),
        applyLevels(input.blue, levels),
        input.alpha,
    };
}

EvaluatedSample thresholdSample(
    const EvaluatedSample& input,
    const EvaluationContext& context,
    const ThresholdOperation& threshold)
{
    if (input.scalar >= threshold.threshold) {
        auto result = sampleFromColour(context.material.highColour);
        result.scalar = 1.0;
        return result;
    }
    auto result = sampleFromColour(context.material.lowColour);
    result.scalar = 0.0;
    return result;
}

double compositeChannel(double background, double source, CompositeMode mode, double opacity)
{
    switch (mode) {
    case CompositeMode::blend:
        return background + (source - background) * opacity;
    case CompositeMode::add:
        return std::clamp(background + source * opacity, 0.0, 1.0);
    case CompositeMode::multiply:
        return background * (1.0 + (source - 1.0) * opacity);
    }
    throw std::invalid_argument("unknown layer composite mode");
}

} // namespace

EvaluatedSample evaluateOperation(
    const LayerOperation& operation,
    const EvaluationContext& context,
    const EvaluatedSample& input)
{
    return std::visit(
        Overloaded{
            [&context](const NoiseOperation& noise) {
                return noiseSample(context, noise.seedOffset);
            },
            [](const SolidColourOperation& solid) {
                return sampleFromColour(solid.colour);
            },
            [&input](const LevelsOperation& levels) {
                return levelsSample(input, levels);
            },
            [&input, &context](const ThresholdOperation& threshold) {
                return thresholdSample(input, context, threshold);
            },
        },
        operation);
}

EvaluatedSample compositeSamples(
    const EvaluatedSample& background,
    const EvaluatedSample& source,
    CompositeMode mode,
    double opacity)
{
    const double amount = std::clamp(opacity, 0.0, 1.0);
    return {
        compositeChannel(background.scalar, source.scalar, mode, amount),
        compositeChannel(background.red, source.red, mode, amount),
        compositeChannel(background.green, source.green, mode, amount),
        compositeChannel(background.blue, source.blue, mode, amount),
        compositeChannel(background.alpha, source.alpha, mode, amount),
    };
}

EvaluatedSample evaluateMaterialSample(const Material& material, double u, double v)
{
    if (const auto error = validateMaterial(material)) {
        throw std::invalid_argument(*error);
    }
    if (!std::isfinite(u) || !std::isfinite(v)) {
        throw std::invalid_argument("evaluation coordinates must be finite");
    }

    const EvaluationContext context{material, u, v};
    if (material.layers.empty()) {
        return noiseSample(context, 0);
    }

    EvaluatedSample accumulated;
    for (const auto& layer : material.layers) {
        if (!layer.enabled) {
            continue;
        }
        const auto source = evaluateOperation(layer.operation, context, accumulated);
        accumulated = compositeSamples(
            accumulated,
            source,
            layer.compositeMode,
            layer.opacity);
    }
    return accumulated;
}

} // namespace paperweight
