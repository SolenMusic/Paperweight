#include <paperweight/material.hpp>

#include <algorithm>
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

} // namespace

std::optional<std::string> validateMaterial(const Material& material)
{
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
            break;
        default:
            return prefix + "composite mode is not supported";
        }
        if (layer.operation.valueless_by_exception()) {
            return prefix + "operation has no value";
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
                if constexpr (std::is_same_v<Operation, LevelsOperation>) {
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
