#include <paperweight/regional_detail.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/structural.hpp>

#include "noise_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace paperweight {
namespace {

constexpr std::uint64_t detailDomain = 0x7265676465746169ULL;
constexpr std::uint64_t macroDomain = 0x6d6163726f62616eULL;
constexpr std::uint64_t mesoDomain = 0x6d65736f62616e64ULL;
constexpr std::uint64_t microDomain = 0x6d6963726f62616eULL;
constexpr std::uint64_t wearDomain = 0x7765617262616e64ULL;

double smoothStep(double edge0, double edge1, double value)
{
    if (edge0 == edge1) return value >= edge1 ? 1.0 : 0.0;
    const double t = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double channel(std::uint8_t value)
{
    return static_cast<double>(value) / 255.0;
}

std::uint32_t physicalPeriod(double extentMetres, double scaleMetres)
{
    const auto rounded = std::llround(
        std::max(extentMetres, scaleMetres) / scaleMetres);
    return static_cast<std::uint32_t>(std::clamp<std::int64_t>(rounded, 1, 4096));
}

std::uint64_t variationKey(
    const RegionalDetailOperation& operation,
    const RegionSample& region,
    std::uint64_t groupScopeKey)
{
    switch (operation.variationScope) {
    case RegionalVariationScope::material:
        return detailDomain;
    case RegionalVariationScope::group:
        return groupScopeKey != 0
            ? mixBits(detailDomain ^ groupScopeKey)
            : mixBits(detailDomain ^ mixBits(operation.seedOffset));
    case RegionalVariationScope::parentRegion:
        return region.parentValid ? region.parentKey : region.key;
    case RegionalVariationScope::region:
        return region.valid ? region.key : detailDomain;
    }
    return detailDomain;
}

// A linear ramp cannot be periodic across both texture axes. This toroidal
// projection retains an authored directional bias while matching exactly at
// every repeat boundary.
double periodicDirectionalGradient(double x, double y, double cosine, double sine)
{
    const double xWave = std::sin(x * 2.0 * std::numbers::pi);
    const double yWave = std::sin(y * 2.0 * std::numbers::pi);
    return std::clamp(0.5 + 0.25 * (xWave * cosine + yWave * sine), 0.0, 1.0);
}

// Integer winding counts are the only globally rotated stripe directions that
// close exactly on a rectangular torus. The requested angle chooses the
// closest winding vector at the authored physical frequency.
double periodicDirectionalWave(
    double x,
    double y,
    double cosine,
    double sine,
    std::uint32_t periodX,
    std::uint32_t periodY,
    double phase)
{
    auto windingX = static_cast<std::int64_t>(std::llround(
        -sine * static_cast<double>(periodX)));
    auto windingY = static_cast<std::int64_t>(std::llround(
        cosine * static_cast<double>(periodY)));
    if (windingX == 0 && windingY == 0) windingY = 1;
    return 0.5 + 0.5 * std::sin(
        (x * static_cast<double>(windingX) +
         y * static_cast<double>(windingY) + phase) *
        2.0 * std::numbers::pi);
}

double periodicBand(
    double x,
    double y,
    double extentX,
    double extentY,
    double scale,
    std::uint64_t seed)
{
    const auto periodX = physicalPeriod(extentX, scale);
    const auto periodY = physicalPeriod(extentY, scale);
    return detail::periodicValueNoise2DUnchecked(
        wrapUnit(x) * static_cast<double>(periodX),
        wrapUnit(y) * static_cast<double>(periodY),
        periodX,
        periodY,
        seed);
}

double band(double distance, double start, double width, double softness)
{
    if (width <= 0.0) return 0.0;
    return smoothStep(start - softness, start + softness, distance) *
        (1.0 - smoothStep(
            start + width - softness,
            start + width + softness,
            distance));
}

double selectedField(
    const RegionalDetailSample& sample,
    RegionalDetailField field)
{
    switch (field) {
    case RegionalDetailField::material: return sample.combined;
    case RegionalDetailField::macro: return sample.macro;
    case RegionalDetailField::meso: return sample.meso;
    case RegionalDetailField::micro: return sample.micro;
    case RegionalDetailField::centreGradient: return sample.centreGradient;
    case RegionalDetailField::directionalGradient: return sample.directionalGradient;
    case RegionalDetailField::planarGradient: return sample.planarGradient;
    case RegionalDetailField::mottling: return sample.mottling;
    case RegionalDetailField::grain: return sample.grain;
    case RegionalDetailField::directionalStrokes: return sample.directionalStrokes;
    case RegionalDetailField::outerShadow: return sample.outerShadow;
    case RegionalDetailField::bevel: return sample.bevel;
    case RegionalDetailField::body: return sample.body;
    case RegionalDetailField::innerHighlight: return sample.innerHighlight;
    case RegionalDetailField::wear: return sample.wear;
    case RegionalDetailField::combined: return sample.combined;
    case RegionalDetailField::palette: return sample.palette;
    }
    return 0.0;
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

} // namespace

RegionalDetailSample evaluateRegionalDetailFields(
    const RegionalDetailOperation& operation,
    const Material& material,
    const RegionSample& region,
    double inputCoverage,
    double u,
    double v,
    std::uint64_t groupScopeKey)
{
    const double coverage = std::clamp(inputCoverage, 0.0, 1.0);
    const bool local = operation.orientation == RegionalDetailOrientation::region &&
        region.valid;
    const double x = local ? region.localU : wrapUnit(u);
    const double y = local ? region.localV : wrapUnit(v);
    const double extentX = std::max(
        LayerLimits::minimumPhysicalDetailScale,
        material.physicalSize.widthMetres * (local ? region.extentU : 1.0));
    const double extentY = std::max(
        LayerLimits::minimumPhysicalDetailScale,
        material.physicalSize.heightMetres * (local ? region.extentV : 1.0));
    const auto key = variationKey(operation, region, groupScopeKey);
    const auto seeded = [&](std::uint64_t domain) {
        return mixBits(material.seed ^ mixBits(operation.seedOffset) ^ mixBits(key) ^ domain);
    };

    const double macro = periodicBand(
        x, y, extentX, extentY, operation.macroScaleMetres, seeded(macroDomain));
    const double meso = periodicBand(
        x, y, extentX, extentY, operation.mesoScaleMetres, seeded(mesoDomain));
    const double micro = periodicBand(
        x, y, extentX, extentY, operation.microScaleMetres, seeded(microDomain));

    const double authoredAngle = operation.gradientAngleDegrees / 360.0;
    const double regionAngle = local ? region.orientationTurns : 0.0;
    const double radians = (authoredAngle + regionAngle) * 2.0 * std::numbers::pi;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    const double centredX = x - 0.5;
    const double centredY = y - 0.5;
    const double directional = local
        ? std::clamp(
            0.5 + (centredX * cosine + centredY * sine) / std::sqrt(2.0),
            0.0,
            1.0)
        : periodicDirectionalGradient(x, y, cosine, sine);
    const double randomAngle = regionRandom(
        material.seed, key, operation.seedOffset, 40) * 2.0 * std::numbers::pi;
    const double randomCosine = std::cos(randomAngle);
    const double randomSine = std::sin(randomAngle);
    const double planar = local
        ? std::clamp(
            0.5 + (centredX * randomCosine + centredY * randomSine) /
                std::sqrt(2.0),
            0.0,
            1.0)
        : periodicDirectionalGradient(x, y, randomCosine, randomSine);
    const double centre = region.valid
        ? std::clamp(1.0 - region.centreDistance, 0.0, 1.0)
        : std::clamp(1.0 - std::hypot(centredX, centredY) * std::sqrt(2.0), 0.0, 1.0);
    const double mottling = std::clamp(macro * 0.62 + meso * 0.38, 0.0, 1.0);

    const auto grainPeriodX = physicalPeriod(extentX, operation.microScaleMetres);
    const auto grainPeriodY = physicalPeriod(extentY, operation.microScaleMetres);
    const double along = centredX * cosine + centredY * sine;
    const double across = -centredX * sine + centredY * cosine;
    const double randomPhase = regionRandom(
        material.seed, key, operation.seedOffset, 41);
    const double grainPhase = randomPhase + (meso - 0.5) * 0.8;
    const double grainWave = local
        ? 0.5 + 0.5 * std::sin(
            (across * static_cast<double>(grainPeriodY) + grainPhase) *
            2.0 * std::numbers::pi)
        : periodicDirectionalWave(
            x,
            y,
            cosine,
            sine,
            grainPeriodX,
            grainPeriodY,
            grainPhase);
    const double grain = std::clamp(
        grainWave * 0.72 + micro * 0.28,
        0.0,
        1.0);
    const auto strokePeriodX = std::max(1U, grainPeriodX / 2U);
    const auto strokePeriodY = std::max(1U, grainPeriodY / 2U);
    const double strokePhase = (macro - 0.5) * 0.35;
    const double strokeWave = local
        ? 0.5 + 0.5 * std::sin(
            (across * static_cast<double>(strokePeriodY) + strokePhase) *
            2.0 * std::numbers::pi)
        : periodicDirectionalWave(
            x,
            y,
            cosine,
            sine,
            strokePeriodX,
            strokePeriodY,
            strokePhase);
    const double strokeBreak = periodicBand(
        local ? along + 0.5 : x,
        local ? across + 0.5 : y,
        extentX,
        extentY,
        operation.mesoScaleMetres,
        seeded(wearDomain));
    const double strokes = smoothStep(0.46, 0.72, strokeWave) *
        smoothStep(0.28, 0.58, strokeBreak);

    const double minimumExtent = std::max(
        LayerLimits::minimumPhysicalDetailScale,
        std::min(extentX, extentY));
    const double boundaryMetres = region.valid
        ? std::clamp(region.boundaryDistance, 0.0, 1.0) * minimumExtent * 0.5
        : minimumExtent;
    const double totalEdgeWidth = operation.outerBandMetres +
        operation.bevelBandMetres + operation.innerBandMetres;
    const double taperedWidth = totalEdgeWidth *
        (1.0 - operation.edgeTaper *
            regionRandom(material.seed, key, operation.seedOffset, 42) * 0.65);
    const double edgeScale = totalEdgeWidth > 0.0
        ? taperedWidth / totalEdgeWidth
        : 1.0;
    const double irregularOffset = (meso * 2.0 - 1.0) *
        operation.edgeIrregularity * totalEdgeWidth * 0.45;
    const double edgeDistance = std::max(0.0, boundaryMetres + irregularOffset);
    const double softness = std::max(
        LayerLimits::minimumPhysicalDetailScale,
        totalEdgeWidth * 0.08);
    const double outerWidth = operation.outerBandMetres * edgeScale;
    const double bevelWidth = operation.bevelBandMetres * edgeScale;
    const double innerWidth = operation.innerBandMetres * edgeScale;
    const double continuity = operation.edgeBreakup == 0.0
        ? 1.0
        : smoothStep(
            operation.edgeBreakup * 0.55,
            std::min(1.0, operation.edgeBreakup * 0.55 + 0.24),
            micro);
    const double outer = coverage * continuity *
        (1.0 - smoothStep(outerWidth - softness, outerWidth + softness, edgeDistance));
    const double bevel = coverage * continuity *
        band(edgeDistance, outerWidth, bevelWidth, softness);
    const double inner = coverage * continuity *
        band(edgeDistance, outerWidth + bevelWidth, innerWidth, softness);
    const double body = coverage * smoothStep(
        outerWidth + bevelWidth + innerWidth - softness,
        outerWidth + bevelWidth + innerWidth + softness,
        edgeDistance);

    const double patches = std::clamp(mottling * 0.65 + grain * 0.35, 0.0, 1.0);
    double wearBasis = std::max(outer, bevel);
    switch (operation.wearBias) {
    case RegionalWearBias::exposedEdges:
        wearBasis = std::max(bevel, inner);
        break;
    case RegionalWearBias::cavities:
        wearBasis = outer;
        break;
    case RegionalWearBias::upwardFaces:
        wearBasis = coverage * planar;
        break;
    case RegionalWearBias::localPatches:
        wearBasis = coverage * patches;
        break;
    case RegionalWearBias::mixed:
        wearBasis = std::clamp(
            std::max(bevel, inner) * 0.48 + outer * 0.22 +
                coverage * planar * 0.12 + coverage * patches * 0.18,
            0.0,
            1.0);
        break;
    }
    const double wearModulation = periodicBand(
        x, y, extentX, extentY, operation.wearScaleMetres, seeded(wearDomain ^ key));
    const double wear = std::clamp(
        wearBasis * operation.wearAmount * (0.55 + wearModulation * 0.45),
        0.0,
        1.0);

    const double variation =
        (macro - 0.5) * operation.macroStrength +
        (meso - 0.5) * operation.mesoStrength +
        (micro - 0.5) * operation.microStrength +
        (planar - 0.5) * operation.gradientStrength +
        (mottling - 0.5) * operation.mottlingStrength +
        (grain - 0.5) * operation.grainStrength +
        (strokes - 0.5) * operation.strokeStrength;
    const double scopedOffset = (regionRandom(
        material.seed, key, operation.seedOffset, 43) - 0.5) * 0.22;
    const double combined = std::clamp(0.5 + variation + scopedOffset, 0.0, 1.0);
    double palette = combined;
    if (operation.paletteSteps >= 2U) {
        const double intervals = static_cast<double>(operation.paletteSteps - 1U);
        palette = std::round(palette * intervals) / intervals;
    }

    return {
        macro, meso, micro, centre, directional, planar, mottling, grain, strokes,
        outer, bevel, body, inner, wear, combined, palette,
    };
}

EvaluatedSample evaluateRegionalDetail(
    const RegionalDetailOperation& operation,
    const EvaluationContext& context,
    const EvaluatedSample& input,
    std::uint64_t groupScopeKey)
{
    const auto fields = evaluateRegionalDetailFields(
        operation,
        context.material,
        input.region,
        input.scalar,
        context.u,
        context.v,
        groupScopeKey);
    if (operation.field != RegionalDetailField::material) {
        const double value = selectedField(fields, operation.field);
        auto result = input;
        if (affectsScalar(operation.target)) result.scalar = value;
        if (affectsColour(operation.target)) {
            result.red = value;
            result.green = value;
            result.blue = value;
        }
        return result;
    }

    auto result = input;
    if (affectsColour(operation.target)) {
        const auto interpolate = [amount = fields.palette](std::uint8_t low, std::uint8_t high) {
            return channel(low) + (channel(high) - channel(low)) * amount;
        };
        const double edgeLighting = std::clamp(
            1.0 - fields.outerShadow * 0.34 - fields.bevel * 0.1 +
                fields.innerHighlight * 0.26 + fields.wear * 0.12,
            0.0,
            1.35);
        const auto apply = [amount = operation.colourAmount, edgeLighting](
            double source,
            double authored) {
            return std::clamp(
                (source + (authored - source) * amount) * edgeLighting,
                0.0,
                1.0);
        };
        result.red = apply(input.red, interpolate(
            operation.paletteLow.red, operation.paletteHigh.red));
        result.green = apply(input.green, interpolate(
            operation.paletteLow.green, operation.paletteHigh.green));
        result.blue = apply(input.blue, interpolate(
            operation.paletteLow.blue, operation.paletteHigh.blue));
    }
    if (affectsScalar(operation.target)) {
        const double centred = (fields.combined - 0.5) * 2.0;
        switch (context.output) {
        case MaterialOutput::height:
        case MaterialOutput::normal:
            result.scalar = std::clamp(
                input.scalar + centred * operation.heightAmount -
                    fields.wear * operation.heightAmount * 0.65,
                0.0,
                1.0);
            break;
        case MaterialOutput::roughness:
        case MaterialOutput::clearCoatRoughness:
            result.scalar = std::clamp(
                input.scalar + std::abs(centred) * operation.roughnessAmount +
                    fields.wear * operation.roughnessAmount,
                0.0,
                1.0);
            break;
        case MaterialOutput::coating:
        case MaterialOutput::clearCoat:
            result.scalar = std::clamp(
                input.scalar * (1.0 - fields.wear * operation.coatingWear),
                0.0,
                1.0);
            break;
        case MaterialOutput::occlusion:
            result.scalar = std::clamp(
                input.scalar * (1.0 -
                    (fields.outerShadow + fields.wear * 0.35) *
                        operation.occlusionAmount),
                0.0,
                1.0);
            break;
        case MaterialOutput::colour:
        case MaterialOutput::metalness:
        case MaterialOutput::emissive:
            break;
        }
    }
    return result;
}

} // namespace paperweight
