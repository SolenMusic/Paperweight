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
};

inline constexpr std::array<MaterialOutput, 5> materialOutputs{
    MaterialOutput::colour,
    MaterialOutput::height,
    MaterialOutput::normal,
    MaterialOutput::roughness,
    MaterialOutput::metalness,
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
    }
    return "unknown";
}

} // namespace paperweight
