#pragma once

#include <cstddef>
#include <cstdint>
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

using LayerOperation = std::variant<
    NoiseOperation,
    SolidColourOperation,
    LevelsOperation,
    ThresholdOperation>;

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
    default:
        return "unknown";
    }
}

} // namespace paperweight
