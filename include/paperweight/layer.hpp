#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include <paperweight/image.hpp>

namespace paperweight {

enum class CompositeMode {
    blend,
    add,
    multiply,
};

enum class QuarterTurn : std::uint8_t {
    none = 0,
    clockwise90 = 1,
    clockwise180 = 2,
    clockwise270 = 3,
};

struct CoordinateTransform {
    std::uint32_t scaleX{1};
    std::uint32_t scaleY{1};
    double offsetX{};
    double offsetY{};
    QuarterTurn rotation{QuarterTurn::none};
    bool warpEnabled{};
    double warpStrength{};
    std::uint32_t warpFrequency{1};
    std::uint64_t warpSeedOffset{};

    friend constexpr bool operator==(
        const CoordinateTransform&,
        const CoordinateTransform&) = default;
};

struct LayerMask {
    bool enabled{};
    bool inverted{};
    std::uint64_t seedOffset{};
    double inputLow{};
    double inputHigh{1.0};

    friend constexpr bool operator==(const LayerMask&, const LayerMask&) = default;
};

struct NoiseOperation {
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(const NoiseOperation&, const NoiseOperation&) = default;
};

struct SolidColourOperation {
    Rgba8 colour{128, 128, 128, 255};

    friend constexpr bool operator==(
        const SolidColourOperation&,
        const SolidColourOperation&) = default;
};

struct LevelsOperation {
    double inputLow{0.0};
    double inputHigh{1.0};
    double gamma{1.0};

    friend constexpr bool operator==(const LevelsOperation&, const LevelsOperation&) = default;
};

struct ThresholdOperation {
    double threshold{0.5};

    friend constexpr bool operator==(
        const ThresholdOperation&,
        const ThresholdOperation&) = default;
};

enum class BrickMortarSpace : std::uint8_t {
    cell = 0,
    texture = 1,
};

struct BrickGridOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{8};
    double mortar{0.08};
    double stagger{0.5};
    double softness{0.02};
    BrickMortarSpace mortarSpace{BrickMortarSpace::cell};
    struct PhysicalDimensions {
        double widthMetres{0.24};
        double heightMetres{0.075};
        double mortarMetres{0.01};

        friend constexpr bool operator==(
            const PhysicalDimensions&,
            const PhysicalDimensions&) = default;
    };
    std::optional<PhysicalDimensions> physicalDimensions;

    friend constexpr bool operator==(
        const BrickGridOperation&,
        const BrickGridOperation&) = default;
};

struct TileGridOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{6};
    double grout{0.08};
    double softness{0.02};

    friend constexpr bool operator==(
        const TileGridOperation&,
        const TileGridOperation&) = default;
};

struct WorleyCellsOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{6};
    double jitter{0.75};
    double edgeWidth{0.18};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const WorleyCellsOperation&,
        const WorleyCellsOperation&) = default;
};

struct RandomCellsOperation {
    std::uint32_t columns{8};
    std::uint32_t rows{8};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const RandomCellsOperation&,
        const RandomCellsOperation&) = default;
};

enum class LineDirection : std::uint8_t {
    vertical = 0,
    horizontal = 1,
};

struct LinesOperation {
    LineDirection direction{LineDirection::vertical};
    std::uint32_t count{8};
    double width{0.12};
    double softness{0.02};

    friend constexpr bool operator==(
        const LinesOperation&,
        const LinesOperation&) = default;
};

struct RectanglesOperation {
    std::uint32_t columns{4};
    std::uint32_t rows{4};
    double width{0.7};
    double height{0.7};
    double softness{0.02};

    friend constexpr bool operator==(
        const RectanglesOperation&,
        const RectanglesOperation&) = default;
};

struct CirclesOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{6};
    double radius{0.35};
    double softness{0.02};

    friend constexpr bool operator==(
        const CirclesOperation&,
        const CirclesOperation&) = default;
};

using LayerOperation = std::variant<
    NoiseOperation,
    SolidColourOperation,
    LevelsOperation,
    ThresholdOperation,
    BrickGridOperation,
    TileGridOperation,
    WorleyCellsOperation,
    RandomCellsOperation,
    LinesOperation,
    RectanglesOperation,
    CirclesOperation>;

struct MaterialLayer {
    bool enabled{true};
    double opacity{1.0};
    CompositeMode compositeMode{CompositeMode::blend};
    LayerOperation operation{NoiseOperation{}};
    CoordinateTransform transform;
    LayerMask mask;

    friend constexpr bool operator==(const MaterialLayer&, const MaterialLayer&) = default;
};

struct LayerLimits {
    static constexpr std::size_t maximumLayers = 32;
    static constexpr double minimumOpacity = 0.0;
    static constexpr double maximumOpacity = 1.0;
    static constexpr double minimumLevel = 0.0;
    static constexpr double maximumLevel = 1.0;
    static constexpr double minimumGamma = 0.1;
    static constexpr double maximumGamma = 4.0;
    static constexpr double minimumThreshold = 0.0;
    static constexpr double maximumThreshold = 1.0;
    static constexpr std::uint32_t minimumScale = 1;
    static constexpr std::uint32_t maximumScale = 16;
    static constexpr double maximumOffsetMagnitude = 1024.0;
    static constexpr double minimumWarpStrength = 0.0;
    static constexpr double maximumWarpStrength = 1.0;
    static constexpr std::uint32_t minimumWarpFrequency = 1;
    static constexpr std::uint32_t maximumWarpFrequency = 16;
    static constexpr std::uint32_t minimumPatternCount = 1;
    static constexpr std::uint32_t maximumPatternCount = 64;
    static constexpr double minimumGap = 0.0;
    static constexpr double maximumGap = 0.95;
    static constexpr double minimumStagger = 0.0;
    static constexpr double maximumStagger = 1.0;
    static constexpr double minimumSoftness = 0.0;
    static constexpr double maximumSoftness = 0.25;
    static constexpr double minimumJitter = 0.0;
    static constexpr double maximumJitter = 1.0;
    static constexpr double minimumCellEdgeWidth = 0.01;
    static constexpr double maximumCellEdgeWidth = 2.0;
    static constexpr double minimumShapeSize = 0.0;
    static constexpr double maximumShapeSize = 1.0;
    static constexpr double minimumCircleRadius = 0.0;
    static constexpr double maximumCircleRadius = 0.5;
};

[[nodiscard]] constexpr MaterialLayer makeNoiseLayer(std::uint64_t seedOffset = 0)
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        NoiseOperation{seedOffset},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeSolidColourLayer(
    Rgba8 colour = {128, 128, 128, 255})
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        SolidColourOperation{colour},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeLevelsLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        LevelsOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeThresholdLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        ThresholdOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeBrickGridLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        BrickGridOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeTileGridLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        TileGridOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeWorleyCellsLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        WorleyCellsOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRandomCellsLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RandomCellsOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeLinesLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        LinesOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRectanglesLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RectanglesOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeCirclesLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        CirclesOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr std::uint32_t rotationDegrees(QuarterTurn rotation)
{
    return static_cast<std::uint32_t>(rotation) * 90U;
}

[[nodiscard]] constexpr std::string_view compositeModeName(CompositeMode mode)
{
    switch (mode) {
    case CompositeMode::blend:
        return "blend";
    case CompositeMode::add:
        return "add";
    case CompositeMode::multiply:
        return "multiply";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view operationName(const LayerOperation& operation)
{
    switch (operation.index()) {
    case 0:
        return "noise";
    case 1:
        return "solid_colour";
    case 2:
        return "levels";
    case 3:
        return "threshold";
    case 4:
        return "brick_grid";
    case 5:
        return "tile_grid";
    case 6:
        return "worley_cells";
    case 7:
        return "random_cells";
    case 8:
        return "lines";
    case 9:
        return "rectangles";
    case 10:
        return "circles";
    default:
        return "unknown";
    }
}

[[nodiscard]] constexpr std::string_view lineDirectionName(LineDirection direction)
{
    switch (direction) {
    case LineDirection::vertical:
        return "vertical";
    case LineDirection::horizontal:
        return "horizontal";
    }
    return "unknown";
}

} // namespace paperweight
