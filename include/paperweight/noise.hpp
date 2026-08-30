#pragma once

#include <cstdint>

#include <paperweight/material.hpp>

namespace paperweight {

[[nodiscard]] double periodicValueNoise2D(
    double x,
    double y,
    std::uint32_t periodX,
    std::uint32_t periodY,
    std::uint64_t seed);

[[nodiscard]] double periodicFbm2D(double u, double v, const Material& material);

} // namespace paperweight
