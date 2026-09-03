#include <paperweight/evaluation.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/organic.hpp>
#include <paperweight/sculpt.hpp>
#include <paperweight/scatter.hpp>
#include <paperweight/shape.hpp>
#include <paperweight/structural.hpp>
#include <paperweight/surface.hpp>

#include "graph_evaluator.hpp"
#include "noise_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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

std::uint8_t unitToChannel(double value)
{
    return static_cast<std::uint8_t>(
        std::round(std::clamp(value, 0.0, 1.0) * 255.0));
}

bool affectsColour(ProcessingTarget target)
{
    return target == ProcessingTarget::colour ||
        target == ProcessingTarget::colourAndScalar;
}

bool affectsScalar(ProcessingTarget target)
{
    return target == ProcessingTarget::scalar ||
        target == ProcessingTarget::colourAndScalar;
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
        {},
    };
}

EvaluatedSample sampleFromScalar(const Material& material, double scalar)
{
    const auto low = sampleFromColour(material.lowColour);
    const auto high = sampleFromColour(material.highColour);
    const auto interpolate = [scalar](double from, double to) {
        return from + (to - from) * scalar;
    };
    return {
        scalar,
        interpolate(low.red, high.red),
        interpolate(low.green, high.green),
        interpolate(low.blue, high.blue),
        interpolate(low.alpha, high.alpha),
        {},
    };
}

EvaluatedSample sampleFromStructural(
    const Material& material,
    const StructuralSample& structural)
{
    auto result = sampleFromScalar(material, structural.value);
    result.region = structural.region;
    return result;
}

EvaluatedSample noiseSample(const EvaluationContext& context, std::uint64_t seedOffset)
{
    const auto seed = seedOffset == 0
        ? context.material.seed
        : mixBits(context.material.seed ^ seedOffset);
    const double scalar = detail::periodicFbm2DUnchecked(
        context.u,
        context.v,
        context.material,
        seed);
    return sampleFromScalar(context.material, scalar);
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
        input.region,
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
        result.region = input.region;
        return result;
    }
    auto result = sampleFromColour(context.material.lowColour);
    result.scalar = 0.0;
    result.region = input.region;
    return result;
}

double posteriseValue(double value, std::uint32_t bands)
{
    const double intervals = static_cast<double>(bands - 1U);
    return std::round(std::clamp(value, 0.0, 1.0) * intervals) / intervals;
}

EvaluatedSample posteriseSample(
    const EvaluatedSample& input,
    const PosteriseOperation& operation)
{
    auto result = input;
    if (affectsScalar(operation.target)) {
        result.scalar = posteriseValue(input.scalar, operation.bands);
    }
    if (affectsColour(operation.target)) {
        result.red = posteriseValue(input.red, operation.bands);
        result.green = posteriseValue(input.green, operation.bands);
        result.blue = posteriseValue(input.blue, operation.bands);
    }
    return result;
}

EvaluatedSample colourRampSample(
    const EvaluatedSample& input,
    const ColourRampOperation& operation)
{
    const auto scalar = std::clamp(input.scalar, 0.0, 1.0);
    const ColourRampStop* lower = &operation.stops.front();
    const ColourRampStop* upper = lower;
    for (const auto& stop : operation.stops) {
        if (stop.position <= scalar) {
            lower = &stop;
        }
        if (stop.position >= scalar) {
            upper = &stop;
            break;
        }
        upper = &stop;
    }

    double amount = 0.0;
    if (operation.mode == ColourRampMode::linear && upper != lower) {
        amount = (scalar - lower->position) / (upper->position - lower->position);
        amount = std::clamp(amount, 0.0, 1.0);
    }
    const auto channel = [amount](std::uint8_t from, std::uint8_t to) {
        return channelToUnit(from) +
            (channelToUnit(to) - channelToUnit(from)) * amount;
    };
    return {
        input.scalar,
        channel(lower->colour.red, upper->colour.red),
        channel(lower->colour.green, upper->colour.green),
        channel(lower->colour.blue, upper->colour.blue),
        channel(lower->colour.alpha, upper->colour.alpha),
        input.region,
    };
}

EvaluatedSample paletteSample(
    const EvaluatedSample& input,
    const PaletteOperation& operation)
{
    const std::array<std::uint8_t, 3> source{
        unitToChannel(input.red),
        unitToChannel(input.green),
        unitToChannel(input.blue),
    };
    const Rgba8* nearest = &operation.colours.front();
    std::uint32_t nearestDistance = std::numeric_limits<std::uint32_t>::max();
    for (const auto& colour : operation.colours) {
        const std::array<std::uint8_t, 3> candidate{
            colour.red,
            colour.green,
            colour.blue,
        };
        std::uint32_t distance = 0;
        for (std::size_t channelIndex = 0; channelIndex < source.size(); ++channelIndex) {
            const int difference = static_cast<int>(source[channelIndex]) -
                static_cast<int>(candidate[channelIndex]);
            distance += static_cast<std::uint32_t>(difference * difference);
        }
        if (distance < nearestDistance) {
            nearest = &colour;
            nearestDistance = distance;
        }
    }
    const auto mapped = sampleFromColour(*nearest);
    return {
        input.scalar,
        mapped.red,
        mapped.green,
        mapped.blue,
        mapped.alpha,
        input.region,
    };
}

EvaluatedSample regionFieldSample(
    const EvaluatedSample& input,
    const EvaluationContext& context,
    const RegionFieldOperation& operation)
{
    double value = 0.0;
    if (input.region.valid) {
        switch (operation.field) {
        case RegionFieldKind::random:
            value = regionRandom(
                context.material.seed,
                input.region.key,
                operation.seedOffset,
                operation.channel);
            break;
        case RegionFieldKind::localU:
            value = input.region.localU;
            break;
        case RegionFieldKind::localV:
            value = input.region.localV;
            break;
        case RegionFieldKind::centreDistance:
            value = input.region.centreDistance;
            break;
        case RegionFieldKind::boundaryDistance:
            value = input.region.boundaryDistance;
            break;
        case RegionFieldKind::courseRandom:
            if (input.region.parentValid) {
                value = regionRandom(
                    context.material.seed,
                    input.region.parentKey,
                    operation.seedOffset,
                    operation.channel);
            }
            break;
        }
    }
    value = std::clamp(value, 0.0, 1.0);
    if (operation.inverted) {
        value = 1.0 - value;
    }
    value = operation.outputLow +
        (operation.outputHigh - operation.outputLow) * value;

    auto result = input;
    if (affectsScalar(operation.target)) {
        result.scalar = value;
    }
    if (affectsColour(operation.target)) {
        result.red = value;
        result.green = value;
        result.blue = value;
    }
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
    case CompositeMode::minimum:
        return background + (std::min(background, source) - background) * opacity;
    case CompositeMode::maximum:
        return background + (std::max(background, source) - background) * opacity;
    case CompositeMode::detail:
        return std::clamp(
            background + (source - 0.5) * 2.0 * opacity,
            0.0,
            1.0);
    }
    throw std::invalid_argument("unknown layer composite mode");
}

std::uint64_t domainSeed(
    std::uint64_t materialSeed,
    std::uint64_t seedOffset,
    std::uint64_t domain)
{
    return mixBits(materialSeed ^ mixBits(seedOffset) ^ domain);
}

EvaluatedCoordinates rotateCoordinates(double u, double v, QuarterTurn rotation)
{
    switch (rotation) {
    case QuarterTurn::none:
        return {u, v};
    case QuarterTurn::clockwise90:
        return {-v, u};
    case QuarterTurn::clockwise180:
        return {-u, -v};
    case QuarterTurn::clockwise270:
        return {v, -u};
    }
    throw std::invalid_argument("unknown coordinate rotation");
}

} // namespace

EvaluatedCoordinates transformCoordinates(
    const CoordinateTransform& transform,
    const EvaluationContext& context)
{
    auto coordinates = rotateCoordinates(context.u, context.v, transform.rotation);
    coordinates.u = coordinates.u * static_cast<double>(transform.scaleX) + transform.offsetX;
    coordinates.v = coordinates.v * static_cast<double>(transform.scaleY) + transform.offsetY;

    if (!transform.warpEnabled || transform.warpStrength == 0.0) {
        return coordinates;
    }

    const double warpU = coordinates.u * static_cast<double>(transform.warpFrequency);
    const double warpV = coordinates.v * static_cast<double>(transform.warpFrequency);
    constexpr std::uint64_t xDomain = 0x5a17c9e3d4b2816fULL;
    constexpr std::uint64_t yDomain = 0xc3e5a7912b4d680fULL;
    const double displacementX = detail::periodicFbm2DUnchecked(
        warpU,
        warpV,
        context.material,
        domainSeed(
            context.material.seed,
            transform.warpSeedOffset,
            xDomain));
    const double displacementY = detail::periodicFbm2DUnchecked(
        warpU,
        warpV,
        context.material,
        domainSeed(
            context.material.seed,
            transform.warpSeedOffset,
            yDomain));
    coordinates.u += (displacementX * 2.0 - 1.0) * transform.warpStrength;
    coordinates.v += (displacementY * 2.0 - 1.0) * transform.warpStrength;
    return coordinates;
}

double evaluateLayerMask(const LayerMask& mask, const EvaluationContext& context)
{
    if (!mask.enabled) {
        return 1.0;
    }
    constexpr std::uint64_t maskDomain = 0x8f31b6d2c5a479e0ULL;
    const double source = detail::periodicFbm2DUnchecked(
        context.u,
        context.v,
        context.material,
        domainSeed(context.material.seed, mask.seedOffset, maskDomain));
    double value = std::clamp(
        (source - mask.inputLow) / (mask.inputHigh - mask.inputLow),
        0.0,
        1.0);
    if (mask.inverted) {
        value = 1.0 - value;
    }
    return value;
}

EvaluatedSample evaluateScatterOperation(
    const ScatterOperation& operation,
    const ScatterLayout& layout,
    const EvaluationContext& context)
{
    const auto scatter = evaluateScatter(operation, layout, context.u, context.v);
    if (!scatter.region.valid) {
        return sampleFromScalar(context.material, 0.0);
    }

    double value = scatter.coverage;
    switch (operation.field) {
    case ScatterField::material: {
        const auto background = sampleFromColour(context.material.lowColour);
        const double amount = scatter.coverage;
        const auto blend = [amount](double from, std::uint8_t to) {
            const double target = static_cast<double>(to) / 255.0;
            return from + (target - from) * amount;
        };
        const double attribute = context.output == MaterialOutput::roughness
            ? scatter.roughness
            : scatter.height;
        return EvaluatedSample{
            attribute * amount,
            blend(background.red, scatter.colour.red),
            blend(background.green, scatter.colour.green),
            blend(background.blue, scatter.colour.blue),
            blend(background.alpha, scatter.colour.alpha),
            scatter.region,
        };
    }
    case ScatterField::fill:
        value = scatter.coverage;
        break;
    case ScatterField::instanceRandom:
        value = scatter.random * scatter.coverage;
        break;
    case ScatterField::localU:
        value = scatter.localU * scatter.coverage;
        break;
    case ScatterField::localV:
        value = scatter.localV * scatter.coverage;
        break;
    case ScatterField::boundaryDistance:
        value = scatter.boundaryDistance * scatter.coverage;
        break;
    }
    auto result = sampleFromScalar(context.material, value);
    result.region = scatter.region;
    return result;
}

EvaluatedSample evaluateOrganicCrackOperation(
    const OrganicCrackOperation& operation,
    const OrganicCrackLayout& layout,
    const EvaluationContext& context)
{
    const auto crack = evaluateOrganicCracks(
        operation,
        layout,
        context.u,
        context.v);
    auto result = sampleFromScalar(context.material, crack.coverage);
    result.region = crack.region;
    return result;
}

EvaluatedSample evaluateLeafClusterOperation(
    const LeafClusterOperation& operation,
    const LeafClusterLayout& layout,
    const EvaluationContext& context)
{
    const auto leaf = evaluateLeafCluster(operation, layout, context.u, context.v);
    if (!leaf.region.valid) {
        return sampleFromScalar(context.material, 0.0);
    }
    if (operation.field == LeafField::material) {
        const auto background = sampleFromColour(context.material.lowColour);
        const double attribute = context.output == MaterialOutput::roughness
            ? leaf.roughness
            : leaf.height;
        const auto blend = [amount = leaf.coverage](double from, std::uint8_t to) {
            return from + (static_cast<double>(to) / 255.0 - from) * amount;
        };
        return {
            attribute * leaf.coverage,
            blend(background.red, leaf.colour.red),
            blend(background.green, leaf.colour.green),
            blend(background.blue, leaf.colour.blue),
            blend(background.alpha, leaf.colour.alpha),
            leaf.region,
        };
    }
    double value = leaf.coverage;
    switch (operation.field) {
    case LeafField::material:
    case LeafField::fill:
        value = leaf.coverage;
        break;
    case LeafField::edge:
        value = leaf.edge;
        break;
    case LeafField::midrib:
        value = leaf.midrib;
        break;
    case LeafField::veins:
        value = leaf.veins;
        break;
    case LeafField::instanceRandom:
        value = leaf.random * leaf.coverage;
        break;
    }
    auto result = sampleFromScalar(context.material, value);
    result.region = leaf.region;
    return result;
}

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
            [&context](const SurfaceValueOperation& value) {
                return sampleFromScalar(context.material, value.value);
            },
            [&input](const LevelsOperation& levels) {
                return levelsSample(input, levels);
            },
            [&input, &context](const ThresholdOperation& threshold) {
                return thresholdSample(input, context, threshold);
            },
            [&input](const PosteriseOperation& posterise) {
                return posteriseSample(input, posterise);
            },
            [&input](const ColourRampOperation& ramp) {
                return colourRampSample(input, ramp);
            },
            [&input](const PaletteOperation& palette) {
                return paletteSample(input, palette);
            },
            [&context](const BrickGridOperation& brick) {
                return sampleFromStructural(
                    context.material,
                    evaluateBrickGridSample(
                        brick,
                        context.material.physicalSize,
                        context.u,
                        context.v));
            },
            [&context](const TileGridOperation& tile) {
                return sampleFromStructural(
                    context.material,
                    evaluateTileGridSample(tile, context.u, context.v));
            },
            [&context](const CourseLayoutOperation& course) {
                return sampleFromStructural(
                    context.material,
                    evaluateCourseLayoutSample(
                        course,
                        context.material.physicalSize,
                        context.u,
                        context.v,
                        context.material.seed));
            },
            [&context](const WorleyCellsOperation& worley) {
                return sampleFromStructural(
                    context.material,
                    evaluateWorleyCellsSample(
                        worley,
                        context.u,
                        context.v,
                        context.material.seed));
            },
            [&context](const RandomCellsOperation& cells) {
                return sampleFromStructural(
                    context.material,
                    evaluateRandomCellsSample(
                        cells,
                        context.u,
                        context.v,
                        context.material.seed));
            },
            [&context](const LinesOperation& lines) {
                return sampleFromStructural(
                    context.material,
                    evaluateLinesSample(lines, context.u, context.v));
            },
            [&context](const RectanglesOperation& rectangles) {
                return sampleFromStructural(
                    context.material,
                    evaluateRectanglesSample(rectangles, context.u, context.v));
            },
            [&context](const CirclesOperation& circles) {
                return sampleFromStructural(
                    context.material,
                    evaluateCirclesSample(circles, context.u, context.v));
            },
            [&context](const ShapePrimitiveOperation& shape) {
                const auto sample = evaluateShapePrimitive(shape, context.u, context.v);
                return sampleFromStructural(
                    context.material,
                    StructuralSample{sample.value, sample.region});
            },
            [&context](const LatticeOperation& lattice) {
                const auto sample = evaluateLattice(lattice, context.u, context.v);
                return sampleFromStructural(
                    context.material,
                    StructuralSample{sample.value, sample.region});
            },
            [&context](const ScatterOperation& scatter) {
                const auto layout = buildScatterLayout(scatter, context.material.seed);
                return evaluateScatterOperation(scatter, layout, context);
            },
            [&input, &context](const ShapeBooleanOperation& boolean) {
                const auto shape = evaluateShapePrimitive(
                    boolean.shape,
                    context.u,
                    context.v);
                auto result = input;
                if (affectsScalar(boolean.target)) {
                    result.scalar = combineShapeMasks(
                        input.scalar,
                        shape.value,
                        boolean.mode);
                }
                if (affectsColour(boolean.target)) {
                    result.red = combineShapeMasks(input.red, shape.value, boolean.mode);
                    result.green = combineShapeMasks(input.green, shape.value, boolean.mode);
                    result.blue = combineShapeMasks(input.blue, shape.value, boolean.mode);
                }
                if (boolean.mode == ShapeBooleanMode::unionMask &&
                    shape.value > input.scalar) {
                    result.region = shape.region;
                }
                return result;
            },
            [&context](const SurfacePatternOperation& surface) {
                return sampleFromScalar(
                    context.material,
                    evaluateSurfacePattern(
                        surface,
                        context.material,
                        context.u,
                        context.v));
            },
            [&input](const SurfaceFilterOperation& filter) {
                const SurfaceNeighbourhood scalar{
                    input.scalar, input.scalar, input.scalar,
                    input.scalar, input.scalar, input.scalar,
                    input.scalar, input.scalar, input.scalar,
                };
                const auto apply = [&filter](double value) {
                    const SurfaceNeighbourhood neighbourhood{
                        value, value, value, value, value,
                        value, value, value, value,
                    };
                    return evaluateSurfaceFilter(filter, neighbourhood);
                };
                return EvaluatedSample{
                    affectsScalar(filter.target)
                        ? evaluateSurfaceFilter(filter, scalar)
                        : input.scalar,
                    affectsColour(filter.target) ? apply(input.red) : input.red,
                    affectsColour(filter.target) ? apply(input.green) : input.green,
                    affectsColour(filter.target) ? apply(input.blue) : input.blue,
                    input.alpha,
                    input.region,
                };
            },
            [&input](const InkContourOperation&) {
                return input;
            },
            [&input, &context](const RegionFieldOperation& regionField) {
                return regionFieldSample(input, context, regionField);
            },
            [&input, &context](const RegionSurfaceOperation& surface) {
                const double value = evaluateRegionSurface(
                    surface,
                    context.material,
                    input.region,
                    input.scalar,
                    context.u,
                    context.v,
                    context.output);
                auto result = input;
                if (affectsScalar(surface.target)) {
                    result.scalar = value;
                }
                if (affectsColour(surface.target)) {
                    result.red = value;
                    result.green = value;
                    result.blue = value;
                }
                return result;
            },
            [&context](const OrganicCellOperation& organic) {
                const auto sample = evaluateOrganicCells(
                    organic,
                    context.u,
                    context.v,
                    context.material.seed);
                auto result = sampleFromScalar(context.material, sample.value);
                result.region = sample.region;
                return result;
            },
            [&context](const OrganicCrackOperation& organic) {
                return evaluateOrganicCrackOperation(
                    organic,
                    buildOrganicCrackLayout(organic, context.material.seed),
                    context);
            },
            [&context](const LeafClusterOperation& leaves) {
                return evaluateLeafClusterOperation(
                    leaves,
                    buildLeafClusterLayout(leaves, context.material.seed),
                    context);
            },
            [&input, &context](const OrganicAccumulationOperation& organic) {
                const auto growth = evaluateOrganicAccumulation(
                    organic,
                    input.scalar,
                    input.region,
                    context.u,
                    context.v,
                    context.material.seed);
                auto result = input;
                if (organic.kind == OrganicAccumulationKind::colourVariation) {
                    if (affectsColour(organic.target)) {
                        const double factor = 1.0 +
                            (growth.variation * 2.0 - 1.0) * organic.variation * growth.amount;
                        result.red = std::clamp(result.red * factor, 0.0, 1.0);
                        result.green = std::clamp(result.green * factor, 0.0, 1.0);
                        result.blue = std::clamp(result.blue * factor, 0.0, 1.0);
                    }
                } else if (affectsColour(organic.target)) {
                    const auto channel = [amount = growth.variation](
                        std::uint8_t from,
                        std::uint8_t to) {
                        return static_cast<std::uint8_t>(std::round(
                            static_cast<double>(from) +
                            (static_cast<double>(to) - static_cast<double>(from)) * amount));
                    };
                    const Rgba8 colour{
                        channel(organic.lowColour.red, organic.highColour.red),
                        channel(organic.lowColour.green, organic.highColour.green),
                        channel(organic.lowColour.blue, organic.highColour.blue),
                        channel(organic.lowColour.alpha, organic.highColour.alpha),
                    };
                    const auto blend = [amount = growth.amount](double from, std::uint8_t to) {
                        return from + (static_cast<double>(to) / 255.0 - from) * amount;
                    };
                    result.red = blend(input.red, colour.red);
                    result.green = blend(input.green, colour.green);
                    result.blue = blend(input.blue, colour.blue);
                }
                if (affectsScalar(organic.target)) {
                    result.scalar = growth.amount;
                }
                return result;
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
    auto result = EvaluatedSample{
        compositeChannel(background.scalar, source.scalar, mode, amount),
        compositeChannel(background.red, source.red, mode, amount),
        compositeChannel(background.green, source.green, mode, amount),
        compositeChannel(background.blue, source.blue, mode, amount),
        compositeChannel(background.alpha, source.alpha, mode, amount),
        {},
    };
    result.region = amount > 0.0 && source.region.valid
        ? source.region
        : background.region;
    return result;
}

EvaluatedSample evaluateMaterialSample(const Material& material, double u, double v)
{
    const auto compiled = compileMaterialGraph(material);
    if (const auto* error = std::get_if<GraphError>(&compiled)) {
        throw std::invalid_argument(error->message);
    }
    return evaluateMaterialGraphSample(
        material,
        std::get<MaterialGraph>(compiled),
        MaterialOutput::colour,
        u,
        v);
}

EvaluatedSample evaluateMaterialGraphSample(
    const Material& material,
    const MaterialGraph& graph,
    MaterialOutput output,
    double u,
    double v)
{
    if (const auto error = validateMaterialSettings(material)) {
        throw std::invalid_argument(*error);
    }
    detail::GraphEvaluator evaluator(material, graph);
    return evaluator.evaluate(output, u, v);
}

} // namespace paperweight
