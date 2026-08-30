#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace paperweight {

struct Material {
    std::uint64_t seed{18431};
    std::uint32_t frequency{4};
    std::uint32_t octaves{5};
    std::uint32_t lacunarity{2};
    double gain{0.5};

    friend constexpr bool operator==(const Material&, const Material&) = default;
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
    static constexpr std::uint32_t maximumLatticePeriod = 4096;
};

enum class MaterialParameter {
    frequency,
    octaves,
    lacunarity,
    gain,
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

inline constexpr std::array<ParameterMetadata, 4> parameterMetadata{{
    {MaterialParameter::frequency, "noise.frequency", "Frequency", 4.0, 1.0, 64.0, 1.0, true},
    {MaterialParameter::octaves, "noise.octaves", "Octaves", 5.0, 1.0, 8.0, 1.0, true},
    {MaterialParameter::lacunarity, "noise.lacunarity", "Lacunarity", 2.0, 1.0, 4.0, 1.0, true},
    {MaterialParameter::gain, "noise.gain", "Gain", 0.5, 0.1, 0.9, 0.01, false},
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

[[nodiscard]] std::optional<std::string> validateMaterial(const Material& material);

} // namespace paperweight
