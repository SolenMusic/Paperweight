#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <paperweight/image.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/physical.hpp>

namespace paperweight {

struct Material {
    std::uint64_t seed{18431};
    std::uint32_t frequency{4};
    std::uint32_t octaves{5};
    std::uint32_t lacunarity{2};
    double gain{0.5};
    Rgba8 lowColour{0, 0, 0, 255};
    Rgba8 highColour{255, 255, 255, 255};
    double normalStrength{1.0};
    double roughnessLow{0.25};
    double roughnessHigh{0.85};
    std::vector<MaterialLayer> layers;
    PhysicalSize physicalSize;

    friend bool operator==(const Material&, const Material&) = default;
};

struct MaterialLimits {
    static constexpr std::uint32_t minimumFrequency = 1;
    static constexpr std::uint32_t maximumFrequency = 64;
    static constexpr std::uint32_t minimumOctaves = 1;
    static constexpr std::uint32_t maximumOctaves = 8;
    static constexpr std::uint32_t minimumLacunarity = 1;
    static constexpr std::uint32_t maximumLacunarity = 4;
    static constexpr double minimumGain = 0.1;
    static constexpr double maximumGain = 0.9;
    static constexpr double minimumNormalStrength = 0.0;
    static constexpr double maximumNormalStrength = 16.0;
    static constexpr double minimumRoughness = 0.0;
    static constexpr double maximumRoughness = 1.0;
    static constexpr std::uint32_t maximumLatticePeriod = 4096;
};

enum class MaterialParameter {
    frequency,
    octaves,
    lacunarity,
    gain,
    normalStrength,
    roughnessLow,
    roughnessHigh,
};

enum class MaterialColour {
    low,
    high,
};

struct ParameterMetadata {
    MaterialParameter parameter;
    std::string_view key;
    std::string_view displayName;
    double defaultValue;
    double minimumValue;
    double maximumValue;
    double step;
    bool integral;
};

inline constexpr std::array<ParameterMetadata, 7> parameterMetadata{{
    {MaterialParameter::frequency, "noise.frequency", "Frequency", 4.0, 1.0, 64.0, 1.0, true},
    {MaterialParameter::octaves, "noise.octaves", "Octaves", 5.0, 1.0, 8.0, 1.0, true},
    {MaterialParameter::lacunarity, "noise.lacunarity", "Lacunarity", 2.0, 1.0, 4.0, 1.0, true},
    {MaterialParameter::gain, "noise.gain", "Gain", 0.5, 0.1, 0.9, 0.01, false},
    {MaterialParameter::normalStrength, "normal.strength", "Normal strength", 1.0, 0.0, 16.0, 0.1, false},
    {MaterialParameter::roughnessLow, "roughness.low", "Roughness low", 0.25, 0.0, 1.0, 0.01, false},
    {MaterialParameter::roughnessHigh, "roughness.high", "Roughness high", 0.85, 0.0, 1.0, 0.01, false},
}};

struct ColourParameterMetadata {
    MaterialColour colour;
    std::string_view key;
    std::string_view displayName;
    Rgba8 defaultValue;
};

inline constexpr std::array<ColourParameterMetadata, 2> colourParameterMetadata{{
    {MaterialColour::low, "colour.low", "Low colour", {0, 0, 0, 255}},
    {MaterialColour::high, "colour.high", "High colour", {255, 255, 255, 255}},
}};

[[nodiscard]] constexpr const ParameterMetadata& metadataFor(MaterialParameter parameter)
{
    for (const auto& metadata : parameterMetadata) {
        if (metadata.parameter == parameter) {
            return metadata;
        }
    }
    return parameterMetadata.front();
}

[[nodiscard]] constexpr const ColourParameterMetadata& metadataFor(MaterialColour colour)
{
    for (const auto& metadata : colourParameterMetadata) {
        if (metadata.colour == colour) {
            return metadata;
        }
    }
    return colourParameterMetadata.front();
}

[[nodiscard]] std::optional<std::string> validateMaterial(const Material& material);
[[nodiscard]] std::optional<std::string> validateMaterialSettings(const Material& material);
[[nodiscard]] std::optional<std::string> validateMaterialLayer(
    const MaterialLayer& layer,
    std::string_view prefix = {});

} // namespace paperweight
