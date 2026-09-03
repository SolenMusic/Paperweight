#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace paperweight {

enum class MaterialOutput : std::uint8_t {
    colour = 0,
    height = 1,
    normal = 2,
    roughness = 3,
    metalness = 4,
    coating = 5,
    occlusion = 6,
    clearCoat = 7,
    clearCoatRoughness = 8,
    emissive = 9,
};

inline constexpr std::array<MaterialOutput, 10> materialOutputs{
    MaterialOutput::colour,
    MaterialOutput::height,
    MaterialOutput::normal,
    MaterialOutput::roughness,
    MaterialOutput::metalness,
    MaterialOutput::coating,
    MaterialOutput::occlusion,
    MaterialOutput::clearCoat,
    MaterialOutput::clearCoatRoughness,
    MaterialOutput::emissive,
};

[[nodiscard]] constexpr std::size_t materialOutputIndex(MaterialOutput output)
{
    return static_cast<std::size_t>(output);
}

[[nodiscard]] constexpr std::string_view materialOutputName(MaterialOutput output)
{
    switch (output) {
    case MaterialOutput::colour:
        return "colour";
    case MaterialOutput::height:
        return "height";
    case MaterialOutput::normal:
        return "normal";
    case MaterialOutput::roughness:
        return "roughness";
    case MaterialOutput::metalness:
        return "metalness";
    case MaterialOutput::coating:
        return "coating";
    case MaterialOutput::occlusion:
        return "occlusion";
    case MaterialOutput::clearCoat:
        return "clear_coat";
    case MaterialOutput::clearCoatRoughness:
        return "clear_coat_roughness";
    case MaterialOutput::emissive:
        return "emissive";
    }
    return "unknown";
}

} // namespace paperweight
