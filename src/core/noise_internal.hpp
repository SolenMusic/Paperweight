#pragma once

#include <cstdint>

#include <paperweight/material.hpp>

namespace paperweight::detail {

[[nodiscard]] double periodicValueNoise2DUnchecked(
    double x,
    double y,
    std::uint32_t periodX,
    std::uint32_t periodY,
    std::uint64_t seed);

[[nodiscard]] double periodicFbm2DUnchecked(
    double u,
    double v,
    const Material& material,
    std::uint64_t seed);

} // namespace paperweight::detail
