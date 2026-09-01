#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include <paperweight/image.hpp>

namespace paperweight {

struct StylisedLightingSettings {
    double lightAzimuthDegrees{315.0};
    double lightElevationDegrees{45.0};
    std::uint32_t diffuseBands{3};
    double highlightThreshold{0.82};
    double ambientContribution{0.22};
    double highlightContribution{0.12};
    double heightNormalStrength{2.0};

    friend constexpr bool operator==(
        const StylisedLightingSettings&,
        const StylisedLightingSettings&) = default;
};

struct StylisedLightingError {
    std::string message;
};

using StylisedLightingResult = std::variant<Image, StylisedLightingError>;

// The returned presentation image is independent of every input. A supplied
// normal map takes precedence; otherwise wrapped height samples derive normals.
[[nodiscard]] StylisedLightingResult bakeStylisedLighting(
    const Image& unlitColour,
    const Image* height,
    const Image* normal,
    const StylisedLightingSettings& settings = {});

} // namespace paperweight
