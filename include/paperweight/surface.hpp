#pragma once

#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>

namespace paperweight {

struct SurfaceNeighbourhood {
    double centre{};
    double left{};
    double right{};
    double up{};
    double down{};
    double upperLeft{};
    double upperRight{};
    double lowerLeft{};
    double lowerRight{};
};

[[nodiscard]] double evaluateSurfacePattern(
    const SurfacePatternOperation& operation,
    const Material& material,
    double u,
    double v);

[[nodiscard]] double evaluateSurfaceFilter(
    const SurfaceFilterOperation& operation,
    const SurfaceNeighbourhood& neighbourhood);

} // namespace paperweight
