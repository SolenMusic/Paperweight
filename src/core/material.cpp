#include <paperweight/material.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <type_traits>

namespace paperweight {
namespace {

bool validPatternCount(std::uint32_t value)
{
    return value >= LayerLimits::minimumPatternCount &&
        value <= LayerLimits::maximumPatternCount;
}

bool validRange(double value, double minimum, double maximum)
{
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

bool validSoftness(double value)
{
    return validRange(
        value,
        LayerLimits::minimumSoftness,
        LayerLimits::maximumSoftness);
}

bool validProcessingTarget(ProcessingTarget target)
{
    switch (target) {
    case ProcessingTarget::colour:
    case ProcessingTarget::scalar:
    case ProcessingTarget::colourAndScalar:
        return true;
    }
    return false;
}

bool validPhysicalMetres(double value)
{
    return validRange(
        value,
        PhysicalLimits::minimumMetres,
        PhysicalLimits::maximumMetres);
}

std::optional<std::uint32_t> exactRepeatCount(double extent, double unit)
{
    if (!validPhysicalMetres(extent) || !validPhysicalMetres(unit)) {
        return std::nullopt;
    }
    const double repeats = extent / unit;
    const double rounded = std::round(repeats);
    const double tolerance = 1.0e-9 * std::max(1.0, std::abs(repeats));
    if (std::abs(repeats - rounded) > tolerance ||
        rounded < LayerLimits::minimumPatternCount ||
        rounded > LayerLimits::maximumPatternCount) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(rounded);
}

std::optional<std::string> validateShapePrimitive(
    const ShapePrimitiveOperation& operation,
    std::string_view prefix)
{
    switch (operation.kind) {
    case ShapePrimitiveKind::roundedRectangle:
    case ShapePrimitiveKind::ellipse:
    case ShapePrimitiveKind::capsule:
    case ShapePrimitiveKind::diamond:
    case ShapePrimitiveKind::convexPolygon:
    case ShapePrimitiveKind::annulus:
    case ShapePrimitiveKind::arc:
    case ShapePrimitiveKind::sector:
    case ShapePrimitiveKind::crescent:
        break;
    default:
        return std::string(prefix) + "shape primitive kind is not supported";
    }
    switch (operation.field) {
    case ShapeFieldKind::fill:
    case ShapeFieldKind::inset:
    case ShapeFieldKind::outline:
    case ShapeFieldKind::border:
        break;
    default:
        return std::string(prefix) + "shape field kind is not supported";
    }
    if (!validPatternCount(operation.columns) ||
        !validPatternCount(operation.rows)) {
        return std::string(prefix) + "shape columns and rows must be between 1 and 64";
    }
    if (!validRange(
            operation.width,
            LayerLimits::minimumShapeDimension,
            LayerLimits::maximumShapeDimension) ||
        !validRange(
            operation.height,
            LayerLimits::minimumShapeDimension,
            LayerLimits::maximumShapeDimension)) {
        return std::string(prefix) +
            "shape width and height must be finite and between 0.001 and 1";
    }
    if (!validRange(operation.cornerRadius, 0.0, 0.5) ||
        !validRange(operation.inset, 0.0, 0.5) ||
        !validRange(operation.borderWidth, 0.0, 0.5)) {
        return std::string(prefix) +
            "shape corner radius, inset, and border width must be finite and between 0 and 0.5";
    }
    if (!validSoftness(operation.softness)) {
        return std::string(prefix) + "shape softness must be finite and between 0 and 0.25";
    }
    if (!validRange(
            operation.offsetX,
            -LayerLimits::maximumShapeOffset,
            LayerLimits::maximumShapeOffset) ||
        !validRange(
            operation.offsetY,
            -LayerLimits::maximumShapeOffset,
            LayerLimits::maximumShapeOffset) ||
        !validRange(operation.stagger, 0.0, 1.0)) {
        return std::string(prefix) +
            "shape offsets must be within -0.5 to 0.5 and stagger within 0 to 1";
    }
    if (!validRange(
            operation.rotationDegrees,
            -LayerLimits::maximumShapeRotation,
            LayerLimits::maximumShapeRotation)) {
        return std::string(prefix) + "local shape rotation must be finite and within -360 to 360 degrees";
    }
    switch (operation.radialOrientation) {
    case RadialOrientation::fixed:
    case RadialOrientation::outward:
    case RadialOrientation::tangent:
        break;
    default:
        return std::string(prefix) + "radial motif orientation is not supported";
    }
    if (!validRange(operation.innerRadius, 0.0, LayerLimits::maximumShapeInnerRadius) ||
        !validRange(operation.arcStartDegrees,
                    -LayerLimits::maximumShapeRotation,
                    LayerLimits::maximumShapeRotation) ||
        !validRange(operation.arcSweepDegrees,
                    LayerLimits::minimumArcSweep,
                    LayerLimits::maximumShapeRotation) ||
        !validRange(operation.crescentOffset,
                    -LayerLimits::maximumShapeOffset,
                    LayerLimits::maximumShapeOffset) ||
        operation.radialCopies < 1 ||
        operation.radialCopies > LayerLimits::maximumRadialCopies ||
        !validRange(operation.radialRadius, 0.0, LayerLimits::maximumShapeOffset) ||
        !validRange(operation.radialPhaseDegrees,
                    -LayerLimits::maximumShapeRotation,
                    LayerLimits::maximumShapeRotation)) {
        return std::string(prefix) +
            "radial shape parameters are outside their supported ranges";
    }
    if ((operation.kind == ShapePrimitiveKind::annulus ||
         operation.kind == ShapePrimitiveKind::arc ||
         operation.kind == ShapePrimitiveKind::sector ||
         operation.kind == ShapePrimitiveKind::crescent) &&
        operation.innerRadius >= std::min(operation.width, operation.height) * 0.5) {
        return std::string(prefix) +
            "radial shape inner radius must be smaller than its outer radius";
    }
    if (operation.kind != ShapePrimitiveKind::convexPolygon) {
        return std::nullopt;
    }
    if (operation.vertices.size() < LayerLimits::minimumPolygonVertices ||
        operation.vertices.size() > LayerLimits::maximumPolygonVertices) {
        return std::string(prefix) + "convex polygons must contain between 3 and 12 vertices";
    }
    double orientation = 0.0;
    for (std::size_t index = 0; index < operation.vertices.size(); ++index) {
        const auto& a = operation.vertices[index];
        const auto& b = operation.vertices[(index + 1) % operation.vertices.size()];
        const auto& c = operation.vertices[(index + 2) % operation.vertices.size()];
        if (!validRange(a.x, -0.5, 0.5) || !validRange(a.y, -0.5, 0.5)) {
            return std::string(prefix) +
                "convex polygon vertices must be finite and within -0.5 to 0.5";
        }
        const double cross =
            (b.x - a.x) * (c.y - b.y) -
            (b.y - a.y) * (c.x - b.x);
        if (std::abs(cross) <= 1.0e-12) {
            return std::string(prefix) +
                "convex polygon vertices must form non-degenerate corners";
        }
        if (orientation == 0.0) {
            orientation = cross;
        } else if ((orientation < 0.0) != (cross < 0.0)) {
            return std::string(prefix) +
                "convex polygon vertices must be ordered around a convex boundary";
        }
    }
    return std::nullopt;
}

std::optional<std::string> validateScatterMask(
    const ScatterMask& mask,
    std::string_view prefix,
    std::string_view name)
{
    if (!validPatternCount(mask.frequency)) {
        return std::string(prefix) + std::string(name) +
            " frequency must be between 1 and 64";
    }
    if (!validRange(mask.inputLow, 0.0, 1.0) ||
        !validRange(mask.inputHigh, 0.0, 1.0) ||
        mask.inputLow >= mask.inputHigh) {
        return std::string(prefix) + std::string(name) +
            " input range must be finite, within 0 to 1, and increasing";
    }
    return std::nullopt;
}

std::optional<std::string> validateScatter(
    const ScatterOperation& operation,
    std::string_view prefix)
{
    switch (operation.field) {
    case ScatterField::material:
    case ScatterField::fill:
    case ScatterField::instanceRandom:
    case ScatterField::localU:
    case ScatterField::localV:
    case ScatterField::boundaryDistance:
        break;
    default:
        return std::string(prefix) + "scatter field is not supported";
    }
    switch (operation.overlapMode) {
    case ScatterOverlapMode::forbidden:
    case ScatterOverlapMode::controlled:
    case ScatterOverlapMode::unrestricted:
        break;
    default:
        return std::string(prefix) + "scatter overlap mode is not supported";
    }
    if (!validPatternCount(operation.columns) ||
        !validPatternCount(operation.rows)) {
        return std::string(prefix) +
            "scatter candidate columns and rows must be between 1 and 64";
    }
    if (!validRange(operation.density, 0.0, 1.0) ||
        !validRange(operation.jitter, 0.0, 1.0) ||
        !validRange(
            operation.minimumDistance,
            0.0,
            LayerLimits::maximumScatterDistance) ||
        !validRange(operation.maximumOverlap, 0.0, 1.0)) {
        return std::string(prefix) +
            "scatter density, jitter, minimum distance, or overlap is outside its supported range";
    }
    if (const auto error = validateShapePrimitive(operation.stamp, prefix)) {
        return error;
    }
    if (operation.stamp.columns != 1 || operation.stamp.rows != 1 ||
        operation.stamp.offsetX != 0.0 || operation.stamp.offsetY != 0.0 ||
        operation.stamp.stagger != 0.0 || operation.stamp.seedOffset != 0) {
        return std::string(prefix) +
            "scatter stamps must use one local shape without repeat offsets or a seed offset";
    }
    if (operation.populations.empty() ||
        operation.populations.size() > LayerLimits::maximumScatterPopulations) {
        return std::string(prefix) +
            "scatter must contain between 1 and 4 size populations";
    }
    double largestScale = 0.0;
    double largestAspect = 0.0;
    for (const auto& population : operation.populations) {
        if (!validRange(population.weight, 0.001, 100.0)) {
            return std::string(prefix) +
                "scatter population weight must be finite and between 0.001 and 100";
        }
        if (!validRange(
                population.minimumScale,
                LayerLimits::minimumScatterScale,
                LayerLimits::maximumScatterScale) ||
            !validRange(
                population.maximumScale,
                LayerLimits::minimumScatterScale,
                LayerLimits::maximumScatterScale) ||
            population.minimumScale > population.maximumScale) {
            return std::string(prefix) +
                "scatter population scale range must be increasing and within 0.1 to 4";
        }
        if (!validRange(
                population.minimumAspect,
                LayerLimits::minimumScatterAspect,
                LayerLimits::maximumScatterAspect) ||
            !validRange(
                population.maximumAspect,
                LayerLimits::minimumScatterAspect,
                LayerLimits::maximumScatterAspect) ||
            population.minimumAspect > population.maximumAspect) {
            return std::string(prefix) +
                "scatter population aspect range must be increasing and within 0.25 to 4";
        }
        if (!validRange(
                population.minimumRotation,
                -LayerLimits::maximumShapeRotation,
                LayerLimits::maximumShapeRotation) ||
            !validRange(
                population.maximumRotation,
                -LayerLimits::maximumShapeRotation,
                LayerLimits::maximumShapeRotation) ||
            population.minimumRotation > population.maximumRotation) {
            return std::string(prefix) +
                "scatter population rotation range must be increasing and within -360 to 360";
        }
        if (!validRange(population.minimumHeight, 0.0, 1.0) ||
            !validRange(population.maximumHeight, 0.0, 1.0) ||
            population.minimumHeight > population.maximumHeight ||
            !validRange(population.minimumRoughness, 0.0, 1.0) ||
            !validRange(population.maximumRoughness, 0.0, 1.0) ||
            population.minimumRoughness > population.maximumRoughness) {
            return std::string(prefix) +
                "scatter population height and roughness ranges must be increasing and within 0 to 1";
        }
        largestScale = std::max(largestScale, population.maximumScale);
        largestAspect = std::max(
            largestAspect,
            std::max(population.maximumAspect, 1.0 / population.minimumAspect));
    }
    const double largestAspectRoot = std::sqrt(largestAspect);
    if (operation.stamp.width * largestScale * largestAspectRoot > 1.0 ||
        operation.stamp.height * largestScale * largestAspectRoot > 1.0) {
        return std::string(prefix) +
            "scatter population range must keep every wrapped stamp within one tile extent";
    }
    if (const auto error = validateScatterMask(
            operation.densityMask,
            prefix,
            "scatter density mask")) {
        return error;
    }
    return validateScatterMask(
        operation.exclusionMask,
        prefix,
        "scatter exclusion mask");
}

std::optional<std::string> validateOrganicCells(
    const OrganicCellOperation& operation,
    std::string_view prefix)
{
    switch (operation.field) {
    case OrganicCellField::plates:
    case OrganicCellField::boundaries:
    case OrganicCellField::cellRandom:
        break;
    default:
        return std::string(prefix) + "organic cell field is not supported";
    }
    switch (operation.direction) {
    case OrganicDirection::vertical:
    case OrganicDirection::horizontal:
        break;
    default:
        return std::string(prefix) + "organic direction is not supported";
    }
    if (!validPatternCount(operation.columns) || !validPatternCount(operation.rows)) {
        return std::string(prefix) + "organic cell columns and rows must be between 1 and 64";
    }
    if (!validRange(
            operation.anisotropy,
            LayerLimits::minimumOrganicAnisotropy,
            LayerLimits::maximumOrganicAnisotropy) ||
        !validRange(operation.jitter, 0.0, 1.0) ||
        !validRange(operation.irregularity, 0.0, 1.0) ||
        !validRange(operation.gap, 0.0, 1.0) ||
        !validSoftness(operation.softness)) {
        return std::string(prefix) +
            "organic cell anisotropy, jitter, irregularity, gap, or softness is outside its supported range";
    }
    return std::nullopt;
}

std::optional<std::string> validateOrganicCracks(
    const OrganicCrackOperation& operation,
    std::string_view prefix)
{
    switch (operation.field) {
    case OrganicCrackField::cracks:
    case OrganicCrackField::trunks:
    case OrganicCrackField::branches:
    case OrganicCrackField::hierarchy:
    case OrganicCrackField::distance:
        break;
    default:
        return std::string(prefix) + "organic crack field is not supported";
    }
    switch (operation.direction) {
    case OrganicDirection::vertical:
    case OrganicDirection::horizontal:
        break;
    default:
        return std::string(prefix) + "organic crack direction is not supported";
    }
    if (operation.roots == 0 || operation.roots > LayerLimits::maximumCrackRoots ||
        operation.segments < 2 || operation.segments > LayerLimits::maximumCrackSegments ||
        operation.branchLevels > LayerLimits::maximumCrackBranchLevels) {
        return std::string(prefix) +
            "organic cracks require 1 to 16 roots, 2 to 16 segments, and at most 5 branch levels";
    }
    if (!validRange(operation.branchProbability, 0.0, 1.0) ||
        !validRange(operation.bend, 0.0, 1.0) ||
        !validRange(operation.width, 0.001, 0.25) ||
        !validRange(operation.taper, 0.0, 1.0) ||
        !validSoftness(operation.softness)) {
        return std::string(prefix) +
            "organic crack probability, bend, width, taper, or softness is outside its supported range";
    }
    return std::nullopt;
}

std::optional<std::string> validateLeafCluster(
    const LeafClusterOperation& operation,
    std::string_view prefix)
{
    switch (operation.field) {
    case LeafField::material:
    case LeafField::fill:
    case LeafField::edge:
    case LeafField::midrib:
    case LeafField::veins:
    case LeafField::instanceRandom:
    case LeafField::outline:
    case LeafField::innerHighlight:
    case LeafField::clusterRandom:
    case LeafField::population:
        break;
    default:
        return std::string(prefix) + "leaf field is not supported";
    }
    switch (operation.profile) {
    case LeafProfile::ovate:
    case LeafProfile::lanceolate:
    case LeafProfile::cordate:
    case LeafProfile::lobed:
    case LeafProfile::blob:
    case LeafProfile::rosette:
    case LeafProfile::lichen:
        break;
    default:
        return std::string(prefix) + "leaf profile is not supported";
    }
    switch (operation.pattern) {
    case LeafClusterPattern::radial:
    case LeafClusterPattern::fan:
    case LeafClusterPattern::vine:
    case LeafClusterPattern::canopy:
    case LeafClusterPattern::groundScatter:
        break;
    default:
        return std::string(prefix) + "leaf cluster pattern is not supported";
    }
    if (!validPatternCount(operation.columns) || !validPatternCount(operation.rows) ||
        operation.leavesPerCluster == 0 ||
        operation.leavesPerCluster > LayerLimits::maximumLeavesPerCluster) {
        return std::string(prefix) +
            "leaf clusters require 1 to 64 columns and rows and 1 to 24 leaves per cluster";
    }
    if (!validRange(operation.density, 0.0, 1.0) ||
        !validRange(operation.clusterSpread, 0.0, LayerLimits::maximumLeafExtent) ||
        !validRange(operation.leafLength, 0.001, LayerLimits::maximumLeafExtent) ||
        !validRange(operation.leafWidth, 0.001, LayerLimits::maximumLeafExtent) ||
        !validRange(operation.scaleVariation, 0.0, 0.9) ||
        !validRange(operation.rotationVariation, 0.0, 360.0) ||
        !validRange(operation.directionDegrees, -360.0, 360.0)) {
        return std::string(prefix) +
            "leaf density, spread, dimensions, variation, or direction is outside its supported range";
    }
    if (!validRange(operation.taper, 0.2, 2.0) ||
        !validRange(operation.baseNotch, 0.0, 1.0) ||
        !validRange(operation.curvature, -1.0, 1.0) ||
        !validRange(operation.serration, 0.0, 0.8) ||
        operation.serrationCount == 0 ||
        operation.serrationCount > LayerLimits::maximumLeafDetails ||
        !validRange(operation.lobing, 0.0, 0.8) ||
        operation.lobeCount == 0 || operation.lobeCount > LayerLimits::maximumLeafDetails ||
        !validRange(operation.midribWidth, 0.0, 0.5) ||
        operation.veinPairs > LayerLimits::maximumLeafDetails ||
        !validRange(operation.veinWidth, 0.0, 0.5) ||
        !validRange(operation.edgeWidth, 0.0, 0.5) ||
        !validRange(operation.innerHighlightWidth, 0.0, 0.5) ||
        !validRange(operation.innerHighlightInset, 0.0, 0.5) ||
        !validRange(operation.clusterColourVariation, 0.0, 1.0) ||
        !validRange(operation.instanceColourVariation, 0.0, 1.0) ||
        !validSoftness(operation.softness)) {
        return std::string(prefix) +
            "leaf silhouette, serration, lobe, rib, vein, edge, or softness parameter is outside its supported range";
    }
    const auto validPopulationProfile = [](LeafProfile profile) {
        switch (profile) {
        case LeafProfile::ovate:
        case LeafProfile::lanceolate:
        case LeafProfile::cordate:
        case LeafProfile::lobed:
        case LeafProfile::blob:
        case LeafProfile::rosette:
        case LeafProfile::lichen:
            return true;
        }
        return false;
    };
    if (!validPopulationProfile(operation.secondaryProfile) ||
        !validPopulationProfile(operation.tertiaryProfile) ||
        !validRange(operation.secondaryWeight, 0.0, 1.0) ||
        !validRange(operation.tertiaryWeight, 0.0, 1.0) ||
        operation.secondaryWeight + operation.tertiaryWeight > 1.0 ||
        !validRange(operation.secondaryScale, LayerLimits::minimumScatterScale,
                    LayerLimits::maximumScatterScale) ||
        !validRange(operation.tertiaryScale, LayerLimits::minimumScatterScale,
                    LayerLimits::maximumScatterScale)) {
        return std::string(prefix) +
            "organic cluster population profiles, weights, and scales are outside their supported range";
    }
    if (!validRange(operation.minimumHeight, 0.0, 1.0) ||
        !validRange(operation.maximumHeight, 0.0, 1.0) ||
        operation.minimumHeight > operation.maximumHeight ||
        !validRange(operation.minimumRoughness, 0.0, 1.0) ||
        !validRange(operation.maximumRoughness, 0.0, 1.0) ||
        operation.minimumRoughness > operation.maximumRoughness) {
        return std::string(prefix) +
            "leaf height and roughness ranges must be increasing and within 0 to 1";
    }
    if (operation.clusterSpread +
            std::hypot(operation.leafLength, operation.leafWidth) * 0.95 > 0.75) {
        return std::string(prefix) +
            "leaf cluster extent must stay bounded below one wrapped tile";
    }
    return std::nullopt;
}

std::optional<std::string> validateOrganicAccumulation(
    const OrganicAccumulationOperation& operation,
    std::string_view prefix)
{
    switch (operation.kind) {
    case OrganicAccumulationKind::moss:
    case OrganicAccumulationKind::lichen:
    case OrganicAccumulationKind::colourVariation:
        break;
    default:
        return std::string(prefix) + "organic accumulation kind is not supported";
    }
    switch (operation.source) {
    case OrganicAccumulationSource::cavity:
    case OrganicAccumulationSource::boundary:
    case OrganicAccumulationSource::lowHeight:
    case OrganicAccumulationSource::authoredMask:
        break;
    default:
        return std::string(prefix) + "organic accumulation source is not supported";
    }
    switch (operation.profile) {
    case OrganicAccumulationProfile::noise:
    case OrganicAccumulationProfile::colonies:
    case OrganicAccumulationProfile::speckles:
        break;
    default:
        return std::string(prefix) + "organic accumulation profile is not supported";
    }
    switch (operation.field) {
    case OrganicAccumulationField::material:
    case OrganicAccumulationField::fill:
    case OrganicAccumulationField::outline:
    case OrganicAccumulationField::innerHighlight:
    case OrganicAccumulationField::detail:
        break;
    default:
        return std::string(prefix) + "organic accumulation field is not supported";
    }
    if (!validPatternCount(operation.scale) ||
        !validRange(operation.coverage, 0.0, 1.0) ||
        !validRange(operation.softness, 0.0, 0.5) ||
        !validRange(operation.moistureBias, 0.0, 1.0) ||
        !validRange(operation.breakup, 0.0, 1.0) ||
        !validRange(operation.variation, 0.0, 1.0) ||
        !validRange(operation.outlineWidth, 0.0, 0.5) ||
        !validRange(operation.innerHighlightWidth, 0.0, 0.5) ||
        !validRange(operation.innerHighlightInset, 0.0, 0.5) ||
        !validProcessingTarget(operation.target)) {
        return std::string(prefix) +
            "organic accumulation scale, coverage, softness, moisture, breakup, variation, or target is outside its supported range";
    }
    return std::nullopt;
}

std::optional<std::string> validateTextile(
    const TextileOperation& operation,
    std::string_view prefix)
{
    switch (operation.pattern) {
    case TextilePattern::plainWeave:
    case TextilePattern::basketWeave:
    case TextilePattern::twillWeave:
    case TextilePattern::loopPile:
    case TextilePattern::cutPile:
        break;
    default:
        return std::string(prefix) + "textile pattern is not supported";
    }
    switch (operation.field) {
    case TextileField::material:
    case TextileField::height:
    case TextileField::warp:
    case TextileField::weft:
    case TextileField::overUnder:
    case TextileField::fibres:
    case TextileField::pile:
    case TextileField::damage:
    case TextileField::colourVariation:
    case TextileField::direction:
        break;
    default:
        return std::string(prefix) + "textile field is not supported";
    }
    switch (operation.yarnProfile) {
    case YarnProfile::round:
    case YarnProfile::flat:
    case YarnProfile::twisted:
        break;
    default:
        return std::string(prefix) + "textile yarn profile is not supported";
    }
    switch (operation.tileOrientation) {
    case TextileTileOrientation::uniform:
    case TextileTileOrientation::alternatingRows:
    case TextileTileOrientation::alternatingColumns:
    case TextileTileOrientation::checkerboard:
        break;
    default:
        return std::string(prefix) + "textile tile orientation is not supported";
    }
    if (operation.columns == 0 || operation.rows == 0 ||
        operation.columns > LayerLimits::maximumTextileThreads ||
        operation.rows > LayerLimits::maximumTextileThreads ||
        operation.tileColumns == 0 || operation.tileRows == 0 ||
        operation.tileColumns > LayerLimits::maximumTextileTiles ||
        operation.tileRows > LayerLimits::maximumTextileTiles) {
        return std::string(prefix) +
            "textiles require 1 to 128 threads and 1 to 16 orientation tiles per axis";
    }
    if (operation.weaveSpan == 0 ||
        operation.weaveSpan > LayerLimits::maximumWeaveSpan ||
        operation.twillStep == 0 ||
        operation.twillStep > LayerLimits::maximumWeaveSpan ||
        operation.fibreFrequency == 0 ||
        operation.fibreFrequency > LayerLimits::maximumFibreFrequency) {
        return std::string(prefix) +
            "textile weave span, twill step, and fibre frequency are outside their supported ranges";
    }
    if (!validRange(operation.yarnWidth, 0.05, 1.5) ||
        !validRange(operation.yarnRoundness, 0.0, 1.0) ||
        !validRange(operation.crossingHeight, 0.0, 1.0) ||
        !validRange(operation.jitter, 0.0, 1.0) ||
        !validRange(operation.fibreStrength, 0.0, 1.0) ||
        !validRange(operation.twist, 0.0, 1.0) ||
        !validRange(operation.pileRadius, 0.05, 0.75) ||
        !validRange(operation.pileHeight, 0.0, 1.0) ||
        !validRange(operation.missingAmount, 0.0, 1.0) ||
        !validRange(operation.damageAmount, 0.0, 1.0) ||
        !validRange(operation.differentColourAmount, 0.0, 1.0) ||
        !validRange(operation.colourVariation, 0.0, 1.0) ||
        !validRange(operation.softness, 0.0, 0.25)) {
        return std::string(prefix) +
            "textile yarn, pile, damage, colour, or softness parameter is outside its supported range";
    }
    if ((operation.tileOrientation == TextileTileOrientation::alternatingRows &&
         operation.tileRows % 2U != 0U) ||
        (operation.tileOrientation == TextileTileOrientation::alternatingColumns &&
         operation.tileColumns % 2U != 0U) ||
        (operation.tileOrientation == TextileTileOrientation::checkerboard &&
         (operation.tileColumns % 2U != 0U || operation.tileRows % 2U != 0U))) {
        return std::string(prefix) +
            "alternating textile orientation requires an even tile count on every alternating axis";
    }
    return std::nullopt;
}

std::optional<std::string> validateRegionAttachment(
    const RegionAttachmentOperation& operation,
    std::string_view prefix)
{
    switch (operation.kind) {
    case RegionAttachmentKind::fastener:
    case RegionAttachmentKind::inlay:
    case RegionAttachmentKind::glyph:
    case RegionAttachmentKind::chip:
    case RegionAttachmentKind::crack:
    case RegionAttachmentKind::damage:
        break;
    default:
        return std::string(prefix) + "region attachment kind is not supported";
    }
    switch (operation.field) {
    case RegionAttachmentField::material:
    case RegionAttachmentField::mask:
    case RegionAttachmentField::distance:
        break;
    default:
        return std::string(prefix) + "region attachment field is not supported";
    }
    const auto validAnchor = [](RegionAnchor anchor) {
        return anchor == RegionAnchor::centre || anchor == RegionAnchor::edge ||
            anchor == RegionAnchor::corner || anchor == RegionAnchor::cavity;
    };
    if (!validAnchor(operation.startAnchor) || !validAnchor(operation.endAnchor)) {
        return std::string(prefix) + "region attachment anchor is not supported";
    }
    switch (operation.glyph) {
    case RegionGlyph::cross:
    case RegionGlyph::chevron:
    case RegionGlyph::triangle:
    case RegionGlyph::rune:
        break;
    default:
        return std::string(prefix) + "region attachment glyph is not supported";
    }
    if (operation.count == 0 ||
        operation.count > LayerLimits::maximumRegionAttachments) {
        return std::string(prefix) + "region attachment count must be between 1 and 8";
    }
    if (!validRange(operation.size, 0.01, 1.0) ||
        !validRange(operation.aspect, 0.2, 5.0) ||
        !validRange(operation.inset, 0.0, 0.45) ||
        !validRange(operation.rotationDegrees, -360.0, 360.0) ||
        !validRange(operation.jitter, 0.0, 1.0) ||
        !validRange(operation.selection, 0.0, 1.0) ||
        !validRange(operation.lineWidth, 0.001, 0.5) ||
        !validRange(operation.length, 0.01, 1.5) ||
        !validRange(operation.branching, 0.0, 1.0) ||
        !validRange(operation.softness, 0.0, 0.25)) {
        return std::string(prefix) +
            "region attachment geometry is outside its supported range";
    }
    if (!validRange(operation.height, 0.0, 1.0) ||
        !validRange(operation.roughness, 0.0, 1.0) ||
        !validRange(operation.metalness, 0.0, 1.0) ||
        !validRange(operation.occlusion, 0.0, 1.0) ||
        !validRange(operation.emissive, 0.0, 1.0)) {
        return std::string(prefix) +
            "region attachment material values must be finite and between 0 and 1";
    }
    return std::nullopt;
}

} // namespace

bool isCanonicalMaterialUid(std::string_view uid)
{
    if (uid.size() != 36) {
        return false;
    }
    for (std::size_t index = 0; index < uid.size(); ++index) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (uid[index] != '-') {
                return false;
            }
            continue;
        }
        const auto character = static_cast<unsigned char>(uid[index]);
        if (!std::isdigit(character) && !(character >= 'a' && character <= 'f')) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> validateMaterialMetadata(const MaterialMetadata& metadata)
{
    const auto validText = [](std::string_view value, std::size_t maximumLength) {
        if (value.size() > maximumLength ||
            (!value.empty() &&
             (std::isspace(static_cast<unsigned char>(value.front())) != 0 ||
              std::isspace(static_cast<unsigned char>(value.back())) != 0))) {
            return false;
        }
        return std::none_of(value.begin(), value.end(), [](char valueCharacter) {
            const auto character = static_cast<unsigned char>(valueCharacter);
            return character < 0x20 || character == 0x7f || valueCharacter == '#' ||
                valueCharacter == '=';
        });
    };

    if (metadata.uid.empty() && metadata.name.empty() && metadata.description.empty() &&
        metadata.category.empty() && metadata.tags.empty()) {
        return "material metadata must contain at least one value";
    }
    if (!metadata.uid.empty() && !isCanonicalMaterialUid(metadata.uid)) {
        return "material UID must be a lowercase canonical UUID";
    }
    if (!validText(metadata.name, MaterialLimits::maximumNameLength)) {
        return "material name must contain at most 128 trimmed, single-line characters";
    }
    if (!validText(metadata.description, MaterialLimits::maximumDescriptionLength)) {
        return "material description must contain at most 512 trimmed, single-line characters";
    }
    if (!validText(metadata.category, MaterialLimits::maximumCategoryLength)) {
        return "material category must contain at most 64 trimmed, single-line characters";
    }
    if (metadata.tags.size() > MaterialLimits::maximumTags) {
        return "material metadata may contain at most 32 tags";
    }
    for (const auto& tag : metadata.tags) {
        if (tag.empty() || !validText(tag, MaterialLimits::maximumTagLength) ||
            tag.find(',') != std::string::npos) {
            return "material tags must be non-empty, comma-free, trimmed, and at most 48 characters";
        }
    }
    return std::nullopt;
}

std::optional<std::string> validateMaterial(const Material& material)
{
    if (material.metadata) {
        if (const auto error = validateMaterialMetadata(*material.metadata)) {
            return error;
        }
    }
    if (!validPhysicalMetres(material.physicalSize.widthMetres) ||
        !validPhysicalMetres(material.physicalSize.heightMetres)) {
        return "material physical width and height must be finite and between 0.000001m and 1000000m";
    }
    if (material.frequency < MaterialLimits::minimumFrequency ||
        material.frequency > MaterialLimits::maximumFrequency) {
        return "frequency must be between 1 and 64";
    }
    if (material.octaves < MaterialLimits::minimumOctaves ||
        material.octaves > MaterialLimits::maximumOctaves) {
        return "octaves must be between 1 and 8";
    }
    if (material.lacunarity < MaterialLimits::minimumLacunarity ||
        material.lacunarity > MaterialLimits::maximumLacunarity) {
        return "lacunarity must be an integer between 1 and 4";
    }
    if (!std::isfinite(material.gain) || material.gain < MaterialLimits::minimumGain ||
        material.gain > MaterialLimits::maximumGain) {
        return "gain must be finite and between 0.1 and 0.9";
    }
    if (!std::isfinite(material.normalStrength) ||
        material.normalStrength < MaterialLimits::minimumNormalStrength ||
        material.normalStrength > MaterialLimits::maximumNormalStrength) {
        return "normal strength must be finite and between 0 and 16";
    }
    if (material.reliefDepthMetres &&
        (!std::isfinite(*material.reliefDepthMetres) ||
         *material.reliefDepthMetres < MaterialLimits::minimumReliefDepthMetres ||
         *material.reliefDepthMetres > MaterialLimits::maximumReliefDepthMetres)) {
        return "surface relief depth must be finite and between 0m and 1000000m";
    }
    if (!std::isfinite(material.roughnessLow) ||
        material.roughnessLow < MaterialLimits::minimumRoughness ||
        material.roughnessLow > MaterialLimits::maximumRoughness) {
        return "low roughness must be finite and between 0 and 1";
    }
    if (!std::isfinite(material.roughnessHigh) ||
        material.roughnessHigh < MaterialLimits::minimumRoughness ||
        material.roughnessHigh > MaterialLimits::maximumRoughness) {
        return "high roughness must be finite and between 0 and 1";
    }
    if (!std::isfinite(material.metalnessLow) ||
        material.metalnessLow < MaterialLimits::minimumMetalness ||
        material.metalnessLow > MaterialLimits::maximumMetalness) {
        return "low metalness must be finite and between 0 and 1";
    }
    if (!std::isfinite(material.metalnessHigh) ||
        material.metalnessHigh < MaterialLimits::minimumMetalness ||
        material.metalnessHigh > MaterialLimits::maximumMetalness) {
        return "high metalness must be finite and between 0 and 1";
    }
    if (!std::isfinite(material.dielectricIor) ||
        material.dielectricIor < MaterialLimits::minimumDielectricIor ||
        material.dielectricIor > MaterialLimits::maximumDielectricIor) {
        return "dielectric IOR must be finite and between 1 and 4";
    }
    const auto validUnit = [](double value) {
        return std::isfinite(value) && value >= MaterialLimits::minimumUnitChannel &&
            value <= MaterialLimits::maximumUnitChannel;
    };
    if (!validUnit(material.coatingLow) || !validUnit(material.coatingHigh)) {
        return "coating range must be finite and between 0 and 1";
    }
    if (!validUnit(material.occlusionLow) || !validUnit(material.occlusionHigh)) {
        return "occlusion range must be finite and between 0 and 1";
    }
    if (!validUnit(material.clearCoatLow) || !validUnit(material.clearCoatHigh)) {
        return "clear coat range must be finite and between 0 and 1";
    }
    if (!validUnit(material.clearCoatRoughnessLow) ||
        !validUnit(material.clearCoatRoughnessHigh)) {
        return "clear coat roughness range must be finite and between 0 and 1";
    }
    if (!validUnit(material.emissiveIntensity)) {
        return "emissive intensity must be finite and between 0 and 1";
    }
    if (!validUnit(material.anisotropyStrength)) {
        return "anisotropy strength must be finite and between 0 and 1";
    }
    if (!std::isfinite(material.anisotropyRotationDegrees) ||
        material.anisotropyRotationDegrees <
            MaterialLimits::minimumAnisotropyRotationDegrees ||
        material.anisotropyRotationDegrees >
            MaterialLimits::maximumAnisotropyRotationDegrees) {
        return "anisotropy rotation must be finite and between 0 and 360 degrees";
    }
    if (material.layers.size() > LayerLimits::maximumLayers) {
        return "a material may contain at most 32 layers";
    }
    for (std::size_t index = 0; index < material.layers.size(); ++index) {
        const auto& layer = material.layers[index];
        const auto prefix = "layer " + std::to_string(index) + ": ";
        if (!std::isfinite(layer.opacity) || layer.opacity < LayerLimits::minimumOpacity ||
            layer.opacity > LayerLimits::maximumOpacity) {
            return prefix + "opacity must be finite and between 0 and 1";
        }
        switch (layer.compositeMode) {
        case CompositeMode::blend:
        case CompositeMode::add:
        case CompositeMode::multiply:
        case CompositeMode::minimum:
        case CompositeMode::maximum:
        case CompositeMode::detail:
            break;
        default:
            return prefix + "composite mode is not supported";
        }
        if (layer.operation.valueless_by_exception()) {
            return prefix + "operation has no value";
        }
        if (!layer.outputs.colour && !layer.outputs.height &&
            !layer.outputs.roughness && !layer.outputs.metalness &&
            !layer.outputs.coating && !layer.outputs.occlusion &&
            !layer.outputs.clearCoat && !layer.outputs.clearCoatRoughness &&
            !layer.outputs.emissive) {
            return prefix + "must target at least one output channel";
        }
        if (layer.transform.scaleX < LayerLimits::minimumScale ||
            layer.transform.scaleX > LayerLimits::maximumScale ||
            layer.transform.scaleY < LayerLimits::minimumScale ||
            layer.transform.scaleY > LayerLimits::maximumScale) {
            return prefix + "coordinate scale must be an integer between 1 and 16";
        }
        switch (layer.transform.rotation) {
        case QuarterTurn::none:
        case QuarterTurn::clockwise90:
        case QuarterTurn::clockwise180:
        case QuarterTurn::clockwise270:
            break;
        default:
            return prefix + "coordinate rotation must be 0, 90, 180, or 270 degrees";
        }
        if (!std::isfinite(layer.transform.offsetX) ||
            !std::isfinite(layer.transform.offsetY) ||
            std::abs(layer.transform.offsetX) > LayerLimits::maximumOffsetMagnitude ||
            std::abs(layer.transform.offsetY) > LayerLimits::maximumOffsetMagnitude) {
            return prefix + "coordinate offsets must be finite and between -1024 and 1024";
        }
        if (!std::isfinite(layer.transform.warpStrength) ||
            layer.transform.warpStrength < LayerLimits::minimumWarpStrength ||
            layer.transform.warpStrength > LayerLimits::maximumWarpStrength) {
            return prefix + "warp strength must be finite and between 0 and 1";
        }
        if (layer.transform.warpFrequency < LayerLimits::minimumWarpFrequency ||
            layer.transform.warpFrequency > LayerLimits::maximumWarpFrequency) {
            return prefix + "warp frequency must be an integer between 1 and 16";
        }
        if (!std::isfinite(layer.mask.inputLow) ||
            !std::isfinite(layer.mask.inputHigh) ||
            layer.mask.inputLow < LayerLimits::minimumLevel ||
            layer.mask.inputHigh > LayerLimits::maximumLevel ||
            layer.mask.inputLow >= layer.mask.inputHigh) {
            return prefix +
                "mask input range must be finite, within 0 to 1, and increasing";
        }
        const auto operationError = std::visit(
            [&prefix, &material](const auto& operation) -> std::optional<std::string> {
                using Operation = std::decay_t<decltype(operation)>;
                if constexpr (std::is_same_v<Operation, SurfaceValueOperation>) {
                    if (!std::isfinite(operation.value) ||
                        operation.value < LayerLimits::minimumLevel ||
                        operation.value > LayerLimits::maximumLevel) {
                        return prefix + "surface value must be finite and between 0 and 1";
                    }
                } else if constexpr (std::is_same_v<Operation, LevelsOperation>) {
                    if (!std::isfinite(operation.inputLow) ||
                        !std::isfinite(operation.inputHigh) ||
                        operation.inputLow < LayerLimits::minimumLevel ||
                        operation.inputHigh > LayerLimits::maximumLevel ||
                        operation.inputLow >= operation.inputHigh) {
                        return prefix +
                            "levels input range must be finite, within 0 to 1, and increasing";
                    }
                    if (!std::isfinite(operation.gamma) ||
                        operation.gamma < LayerLimits::minimumGamma ||
                        operation.gamma > LayerLimits::maximumGamma) {
                        return prefix + "levels gamma must be finite and between 0.1 and 4";
                    }
                } else if constexpr (std::is_same_v<Operation, ThresholdOperation>) {
                    if (!std::isfinite(operation.threshold) ||
                        operation.threshold < LayerLimits::minimumThreshold ||
                        operation.threshold > LayerLimits::maximumThreshold) {
                        return prefix + "threshold must be finite and between 0 and 1";
                    }
                } else if constexpr (std::is_same_v<Operation, PosteriseOperation>) {
                    if (operation.bands < LayerLimits::minimumPosteriseBands ||
                        operation.bands > LayerLimits::maximumPosteriseBands) {
                        return prefix + "posterise bands must be between 2 and 16";
                    }
                    if (!validProcessingTarget(operation.target)) {
                        return prefix + "posterise target is not supported";
                    }
                } else if constexpr (std::is_same_v<Operation, ColourRampOperation>) {
                    if (operation.stops.size() < LayerLimits::minimumColourStops ||
                        operation.stops.size() > LayerLimits::maximumColourStops) {
                        return prefix + "colour ramp must contain between 2 and 8 stops";
                    }
                    switch (operation.mode) {
                    case ColourRampMode::linear:
                    case ColourRampMode::stepped:
                        break;
                    default:
                        return prefix + "colour ramp mode is not supported";
                    }
                    double previous = -1.0;
                    for (const auto& stop : operation.stops) {
                        if (!std::isfinite(stop.position) || stop.position < 0.0 ||
                            stop.position > 1.0 || stop.position <= previous) {
                            return prefix +
                                "colour ramp stop positions must be finite, within 0 to 1, and strictly increasing";
                        }
                        previous = stop.position;
                    }
                } else if constexpr (std::is_same_v<Operation, PaletteOperation>) {
                    if (operation.colours.size() < LayerLimits::minimumColourStops ||
                        operation.colours.size() > LayerLimits::maximumColourStops) {
                        return prefix + "palette must contain between 2 and 8 colours";
                    }
                } else if constexpr (std::is_same_v<Operation, BrickGridOperation>) {
                    if (operation.physicalDimensions) {
                        const auto& physical = *operation.physicalDimensions;
                        if (!validPhysicalMetres(physical.widthMetres) ||
                            !validPhysicalMetres(physical.heightMetres) ||
                            !std::isfinite(physical.mortarMetres) ||
                            physical.mortarMetres < 0.0 ||
                            physical.mortarMetres >=
                                std::min(physical.widthMetres, physical.heightMetres)) {
                            return prefix +
                                "physical brick width and height must be positive metre values, and mortar must be smaller than both";
                        }
                        if (!exactRepeatCount(
                                material.physicalSize.widthMetres,
                                physical.widthMetres) ||
                            !exactRepeatCount(
                                material.physicalSize.heightMetres,
                                physical.heightMetres)) {
                            return prefix +
                                "physical brick width and height must divide the material repeat into 1 to 64 whole bricks";
                        }
                    } else {
                        if (!validPatternCount(operation.columns) ||
                            !validPatternCount(operation.rows)) {
                            return prefix + "brick columns and rows must be between 1 and 64";
                        }
                        if (!validRange(
                                operation.mortar,
                                LayerLimits::minimumGap,
                                LayerLimits::maximumGap)) {
                            return prefix + "brick mortar must be finite and between 0 and 0.95";
                        }
                        switch (operation.mortarSpace) {
                        case BrickMortarSpace::cell:
                        case BrickMortarSpace::texture:
                            break;
                        default:
                            return prefix + "brick mortar space is not supported";
                        }
                    }
                    if (!validRange(
                            operation.stagger,
                            LayerLimits::minimumStagger,
                            LayerLimits::maximumStagger)) {
                        return prefix + "brick stagger must be finite and between 0 and 1";
                    }
                    if (!validSoftness(operation.softness)) {
                        return prefix + "brick softness must be finite and between 0 and 0.25";
                    }
                } else if constexpr (std::is_same_v<Operation, TileGridOperation>) {
                    if (!validPatternCount(operation.columns) ||
                        !validPatternCount(operation.rows)) {
                        return prefix + "tile columns and rows must be between 1 and 64";
                    }
                    if (!validRange(
                            operation.grout,
                            LayerLimits::minimumGap,
                            LayerLimits::maximumGap)) {
                        return prefix + "tile grout must be finite and between 0 and 0.95";
                    }
                    if (!validSoftness(operation.softness)) {
                        return prefix + "tile softness must be finite and between 0 and 0.25";
                    }
                } else if constexpr (std::is_same_v<Operation, CourseLayoutOperation>) {
                    switch (operation.profile) {
                    case CourseLayoutProfile::masonry:
                    case CourseLayoutProfile::slabs:
                    case CourseLayoutProfile::slates:
                        break;
                    default:
                        return prefix + "course layout profile is not supported";
                    }
                    switch (operation.field) {
                    case CourseLayoutField::blocks:
                    case CourseLayoutField::mortar:
                    case CourseLayoutField::course:
                    case CourseLayoutField::overlap:
                        break;
                    default:
                        return prefix + "course layout field is not supported";
                    }
                    if (!validPatternCount(operation.blocks) ||
                        !validPatternCount(operation.courses)) {
                        return prefix +
                            "course layout blocks and courses must be between 1 and 64";
                    }
                    if (operation.physicalDimensions) {
                        const auto& physical = *operation.physicalDimensions;
                        if (!validPhysicalMetres(physical.blockWidthMetres) ||
                            !validPhysicalMetres(physical.courseHeightMetres) ||
                            !std::isfinite(physical.gapMetres) ||
                            physical.gapMetres < 0.0 ||
                            physical.gapMetres >= std::min(
                                physical.blockWidthMetres,
                                physical.courseHeightMetres) ||
                            !std::isfinite(physical.overlapMetres) ||
                            physical.overlapMetres < 0.0 ||
                            physical.overlapMetres >= physical.courseHeightMetres) {
                            return prefix +
                                "physical course block width and height must be positive metre values; gap and overlap must fit within them";
                        }
                        if (!exactRepeatCount(
                                material.physicalSize.widthMetres,
                                physical.blockWidthMetres) ||
                            !exactRepeatCount(
                                material.physicalSize.heightMetres,
                                physical.courseHeightMetres)) {
                            return prefix +
                                "physical course block width and height must divide the material repeat into 1 to 64 whole units";
                        }
                    }
                    if (!validRange(
                            operation.blockVariation,
                            LayerLimits::minimumLayoutVariation,
                            LayerLimits::maximumLayoutVariation) ||
                        !validRange(
                            operation.courseVariation,
                            LayerLimits::minimumLayoutVariation,
                            LayerLimits::maximumLayoutVariation)) {
                        return prefix +
                            "course layout variation must be finite and between 0 and 1";
                    }
                    if (!validRange(
                            operation.stagger,
                            LayerLimits::minimumStagger,
                            LayerLimits::maximumStagger) ||
                        !validRange(
                            operation.crookedness,
                            LayerLimits::minimumLayoutVariation,
                            LayerLimits::maximumLayoutVariation) ||
                        !validRange(
                            operation.gap,
                            LayerLimits::minimumGap,
                            LayerLimits::maximumGap) ||
                        !validSoftness(operation.softness) ||
                        !validRange(
                            operation.overlap,
                            LayerLimits::minimumLayoutOverlap,
                            LayerLimits::maximumLayoutOverlap)) {
                        return prefix +
                            "course layout stagger, crookedness, gap, softness, or overlap is outside its supported range";
                    }
                } else if constexpr (std::is_same_v<Operation, WorleyCellsOperation>) {
                    if (!validPatternCount(operation.columns) ||
                        !validPatternCount(operation.rows)) {
                        return prefix + "Worley columns and rows must be between 1 and 64";
                    }
                    if (!validRange(
                            operation.jitter,
                            LayerLimits::minimumJitter,
                            LayerLimits::maximumJitter)) {
                        return prefix + "Worley jitter must be finite and between 0 and 1";
                    }
                    if (!validRange(
                            operation.edgeWidth,
                            LayerLimits::minimumCellEdgeWidth,
                            LayerLimits::maximumCellEdgeWidth)) {
                        return prefix + "Worley edge width must be finite and between 0.01 and 2";
                    }
                } else if constexpr (std::is_same_v<Operation, RandomCellsOperation>) {
                    if (!validPatternCount(operation.columns) ||
                        !validPatternCount(operation.rows)) {
                        return prefix + "random-cell columns and rows must be between 1 and 64";
                    }
                } else if constexpr (std::is_same_v<Operation, LinesOperation>) {
                    if (!validPatternCount(operation.count)) {
                        return prefix + "line count must be between 1 and 64";
                    }
                    switch (operation.direction) {
                    case LineDirection::vertical:
                    case LineDirection::horizontal:
                        break;
                    default:
                        return prefix + "line direction is not supported";
                    }
                    if (!validRange(
                            operation.width,
                            LayerLimits::minimumShapeSize,
                            LayerLimits::maximumShapeSize)) {
                        return prefix + "line width must be finite and between 0 and 1";
                    }
                    if (!validSoftness(operation.softness)) {
                        return prefix + "line softness must be finite and between 0 and 0.25";
                    }
                } else if constexpr (std::is_same_v<Operation, RectanglesOperation>) {
                    if (!validPatternCount(operation.columns) ||
                        !validPatternCount(operation.rows)) {
                        return prefix + "rectangle columns and rows must be between 1 and 64";
                    }
                    if (!validRange(
                            operation.width,
                            LayerLimits::minimumShapeSize,
                            LayerLimits::maximumShapeSize) ||
                        !validRange(
                            operation.height,
                            LayerLimits::minimumShapeSize,
                            LayerLimits::maximumShapeSize)) {
                        return prefix + "rectangle width and height must be finite and between 0 and 1";
                    }
                    if (!validSoftness(operation.softness)) {
                        return prefix + "rectangle softness must be finite and between 0 and 0.25";
                    }
                } else if constexpr (std::is_same_v<Operation, CirclesOperation>) {
                    if (!validPatternCount(operation.columns) ||
                        !validPatternCount(operation.rows)) {
                        return prefix + "circle columns and rows must be between 1 and 64";
                    }
                    if (!validRange(
                            operation.radius,
                            LayerLimits::minimumCircleRadius,
                            LayerLimits::maximumCircleRadius)) {
                        return prefix + "circle radius must be finite and between 0 and 0.5";
                    }
                    if (!validSoftness(operation.softness)) {
                        return prefix + "circle softness must be finite and between 0 and 0.25";
                    }
                } else if constexpr (
                    std::is_same_v<Operation, ShapePrimitiveOperation>) {
                    if (const auto error = validateShapePrimitive(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (
                    std::is_same_v<Operation, ShapeBooleanOperation>) {
                    switch (operation.mode) {
                    case ShapeBooleanMode::unionMask:
                    case ShapeBooleanMode::intersection:
                    case ShapeBooleanMode::subtraction:
                        break;
                    default:
                        return prefix + "shape Boolean mode is not supported";
                    }
                    if (!validProcessingTarget(operation.target)) {
                        return prefix + "shape Boolean target is not supported";
                    }
                    if (const auto error = validateShapePrimitive(operation.shape, prefix)) {
                        return error;
                    }
                } else if constexpr (std::is_same_v<Operation, LatticeOperation>) {
                    switch (operation.kind) {
                    case LatticeKind::lines:
                    case LatticeKind::diamonds:
                        break;
                    default:
                        return prefix + "lattice kind is not supported";
                    }
                    if (operation.windingX < -LayerLimits::maximumLatticeWinding ||
                        operation.windingX > LayerLimits::maximumLatticeWinding ||
                        operation.windingY < -LayerLimits::maximumLatticeWinding ||
                        operation.windingY > LayerLimits::maximumLatticeWinding ||
                        (operation.windingX == 0 && operation.windingY == 0)) {
                        return prefix +
                            "lattice integer windings must be between -64 and 64 and not both zero";
                    }
                    if (operation.kind == LatticeKind::diamonds &&
                        (operation.windingX == 0 || operation.windingY == 0)) {
                        return prefix +
                            "diamond lattices require non-zero horizontal and vertical windings";
                    }
                    if (!validRange(operation.width, 0.001, 1.0) ||
                        !validSoftness(operation.softness)) {
                        return prefix +
                            "lattice width must be within 0.001 to 1 and softness within 0 to 0.25";
                    }
                    if (!validRange(operation.phase, 0.0, 1.0)) {
                        return prefix + "lattice phase must be finite and between 0 and 1";
                    }
                } else if constexpr (std::is_same_v<Operation, ScatterOperation>) {
                    if (const auto error = validateScatter(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (std::is_same_v<Operation, OrganicCellOperation>) {
                    if (const auto error = validateOrganicCells(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (std::is_same_v<Operation, OrganicCrackOperation>) {
                    if (const auto error = validateOrganicCracks(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (std::is_same_v<Operation, LeafClusterOperation>) {
                    if (const auto error = validateLeafCluster(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (
                    std::is_same_v<Operation, OrganicAccumulationOperation>) {
                    if (const auto error = validateOrganicAccumulation(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (std::is_same_v<Operation, TextileOperation>) {
                    if (const auto error = validateTextile(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (
                    std::is_same_v<Operation, RegionAttachmentOperation>) {
                    if (const auto error = validateRegionAttachment(operation, prefix)) {
                        return error;
                    }
                } else if constexpr (
                    std::is_same_v<Operation, SurfacePatternOperation>) {
                    switch (operation.kind) {
                    case SurfacePatternKind::ridgedNoise:
                    case SurfacePatternKind::bands:
                    case SurfacePatternKind::rings:
                    case SurfacePatternKind::scatter:
                    case SurfacePatternKind::streaks:
                        break;
                    default:
                        return prefix + "surface pattern kind is not supported";
                    }
                    if (!validPatternCount(operation.scale)) {
                        return prefix + "surface pattern scale must be between 1 and 64";
                    }
                    if (!validRange(
                            operation.width,
                            LayerLimits::minimumSurfaceWidth,
                            LayerLimits::maximumSurfaceWidth)) {
                        return prefix +
                            "surface pattern width must be finite and between 0.001 and 1";
                    }
                    if (!validRange(
                            operation.detail,
                            LayerLimits::minimumSurfaceControl,
                            LayerLimits::maximumSurfaceControl) ||
                        !validRange(
                            operation.distortion,
                            LayerLimits::minimumSurfaceControl,
                            LayerLimits::maximumSurfaceControl) ||
                        !validRange(
                            operation.variation,
                            LayerLimits::minimumSurfaceControl,
                            LayerLimits::maximumSurfaceControl)) {
                        return prefix +
                            "surface pattern detail, distortion, and variation must be finite and between 0 and 1";
                    }
                } else if constexpr (
                    std::is_same_v<Operation, SurfaceFilterOperation>) {
                    switch (operation.kind) {
                    case SurfaceFilterKind::invert:
                    case SurfaceFilterKind::soften:
                    case SurfaceFilterKind::expand:
                    case SurfaceFilterKind::contract:
                    case SurfaceFilterKind::edge:
                    case SurfaceFilterKind::slope:
                    case SurfaceFilterKind::cavity:
                    case SurfaceFilterKind::peaks:
                    case SurfaceFilterKind::edgeAwareSoften:
                        break;
                    default:
                        return prefix + "surface filter kind is not supported";
                    }
                    if (!validRange(
                            operation.radius,
                            LayerLimits::minimumFilterRadius,
                            LayerLimits::maximumFilterRadius)) {
                        return prefix +
                            "surface filter radius must be finite and between 0 and 0.25";
                    }
                    if (!validRange(
                            operation.strength,
                            LayerLimits::minimumSurfaceControl,
                            LayerLimits::maximumSurfaceControl)) {
                        return prefix +
                            "surface filter strength must be finite and between 0 and 1";
                    }
                    if (!validRange(
                            operation.sensitivity,
                            LayerLimits::minimumFilterSensitivity,
                            LayerLimits::maximumFilterSensitivity)) {
                        return prefix +
                            "surface filter sensitivity must be finite and between 0 and 1";
                    }
                    if (!validProcessingTarget(operation.target)) {
                        return prefix + "surface filter target is not supported";
                    }
                } else if constexpr (std::is_same_v<Operation, InkContourOperation>) {
                    if (!validRange(
                            operation.radius,
                            LayerLimits::minimumFilterRadius,
                            LayerLimits::maximumFilterRadius)) {
                        return prefix +
                            "ink contour radius must be finite and between 0 and 0.25";
                    }
                    if (!validRange(
                            operation.threshold,
                            LayerLimits::minimumThreshold,
                            LayerLimits::maximumThreshold)) {
                        return prefix +
                            "ink contour threshold must be finite and between 0 and 1";
                    }
                    if (!validRange(
                            operation.softness,
                            LayerLimits::minimumContourSoftness,
                            LayerLimits::maximumContourSoftness)) {
                        return prefix +
                            "ink contour softness must be finite and between 0 and 0.5";
                    }
                    if (!validRange(
                            operation.strength,
                            LayerLimits::minimumSurfaceControl,
                            LayerLimits::maximumSurfaceControl)) {
                        return prefix +
                            "ink contour strength must be finite and between 0 and 1";
                    }
                } else if constexpr (std::is_same_v<Operation, RegionFieldOperation>) {
                    switch (operation.field) {
                    case RegionFieldKind::random:
                    case RegionFieldKind::localU:
                    case RegionFieldKind::localV:
                    case RegionFieldKind::centreDistance:
                    case RegionFieldKind::boundaryDistance:
                    case RegionFieldKind::courseRandom:
                        break;
                    default:
                        return prefix + "region field kind is not supported";
                    }
                    if (operation.channel > LayerLimits::maximumRegionChannel) {
                        return prefix + "region random channel must be between 0 and 255";
                    }
                    if (!validRange(
                            operation.outputLow,
                            LayerLimits::minimumLevel,
                            LayerLimits::maximumLevel) ||
                        !validRange(
                            operation.outputHigh,
                            LayerLimits::minimumLevel,
                            LayerLimits::maximumLevel)) {
                        return prefix +
                            "region field output range must be finite and between 0 and 1";
                    }
                    if (!validProcessingTarget(operation.target)) {
                        return prefix + "region field target is not supported";
                    }
                } else if constexpr (std::is_same_v<Operation, RegionSurfaceOperation>) {
                    switch (operation.field) {
                    case RegionSurfaceField::height:
                    case RegionSurfaceField::cavity:
                    case RegionSurfaceField::outerEdge:
                    case RegionSurfaceField::exposedFace:
                    case RegionSurfaceField::facet:
                    case RegionSurfaceField::wear:
                        break;
                    default:
                        return prefix + "region surface field is not supported";
                    }
                    switch (operation.profile) {
                    case BevelProfile::rounded:
                    case BevelProfile::chamfered:
                    case BevelProfile::handCut:
                        break;
                    default:
                        return prefix + "bevel profile is not supported";
                    }
                    if (!validRange(
                            operation.bevelWidth,
                            LayerLimits::minimumBevelWidth,
                            LayerLimits::maximumBevelWidth)) {
                        return prefix +
                            "bevel width must be finite and between 0.001 and 1";
                    }
                    if (!validRange(operation.bevelHeight, 0.0, 1.0) ||
                        !validRange(operation.facetStrength, 0.0, 1.0) ||
                        !validRange(operation.centrePeak, 0.0, 1.0) ||
                        !validRange(operation.slopeStrength, 0.0, 1.0) ||
                        !validRange(operation.chipAmount, 0.0, 1.0) ||
                        !validRange(operation.wearAmount, 0.0, 1.0) ||
                        !validRange(operation.erosionAmount, 0.0, 1.0)) {
                        return prefix +
                            "surface height, facets, peak, slope, chips, wear, and erosion must be finite and between 0 and 1";
                    }
                    if (operation.facetCount < LayerLimits::minimumFacetCount ||
                        operation.facetCount > LayerLimits::maximumFacetCount) {
                        return prefix + "facet count must be between 3 and 16";
                    }
                    if (operation.chipScale < LayerLimits::minimumChipScale ||
                        operation.chipScale > LayerLimits::maximumChipScale) {
                        return prefix + "chip scale must be between 1 and 64";
                    }
                    if (!validProcessingTarget(operation.target)) {
                        return prefix + "region surface target is not supported";
                    }
                }
                return std::nullopt;
            },
            layer.operation);
        if (operationError) {
            return operationError;
        }
    }

    std::uint64_t period = material.frequency;
    for (std::uint32_t octave = 1; octave < material.octaves; ++octave) {
        if (period > MaterialLimits::maximumLatticePeriod / material.lacunarity) {
            return "frequency, octaves, and lacunarity exceed the maximum lattice period";
        }
        period *= material.lacunarity;
    }
    return std::nullopt;
}

std::optional<std::string> validateMaterialSettings(const Material& material)
{
    auto settingsOnly = material;
    settingsOnly.layers.clear();
    return validateMaterial(settingsOnly);
}

std::optional<std::string> validateMaterialLayer(
    const MaterialLayer& layer,
    std::string_view prefix)
{
    Material probe;
    if (const auto* brick = std::get_if<BrickGridOperation>(&layer.operation);
        brick != nullptr && brick->physicalDimensions) {
        probe.physicalSize = {
            brick->physicalDimensions->widthMetres,
            brick->physicalDimensions->heightMetres,
        };
    } else if (const auto* course =
                   std::get_if<CourseLayoutOperation>(&layer.operation);
               course != nullptr && course->physicalDimensions) {
        probe.physicalSize = {
            course->physicalDimensions->blockWidthMetres,
            course->physicalDimensions->courseHeightMetres,
        };
    }
    probe.layers.push_back(layer);
    auto error = validateMaterial(probe);
    if (!error) {
        return std::nullopt;
    }

    constexpr std::string_view generatedPrefix = "layer 0: ";
    if (error->starts_with(generatedPrefix)) {
        return std::string(prefix) + error->substr(generatedPrefix.size());
    }
    return error;
}

} // namespace paperweight
