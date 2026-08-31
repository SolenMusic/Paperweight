#include <paperweight/material.hpp>

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

} // namespace

std::optional<std::string> validateMaterial(const Material& material)
{
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
            [&prefix](const auto& operation) -> std::optional<std::string> {
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
                } else if constexpr (std::is_same_v<Operation, BrickGridOperation>) {
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

} // namespace paperweight
