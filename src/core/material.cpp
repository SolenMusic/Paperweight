#include <paperweight/material.hpp>

#include <cmath>
#include <limits>

namespace paperweight {

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
