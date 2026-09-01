#pragma once

#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>
#include <paperweight/output.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

struct RegionSurfaceSample {
    double height{};
    double cavity{};
    double outerEdge{};
    double exposedFace{};
    double facet{};
    double wear{};

    friend constexpr bool operator==(
        const RegionSurfaceSample&,
        const RegionSurfaceSample&) = default;
};

[[nodiscard]] RegionSurfaceSample evaluateRegionSurfaceFields(
    const RegionSurfaceOperation& operation,
    const Material& material,
    const RegionSample& region,
    double inputCoverage,
    double u,
    double v,
    MaterialOutput output = MaterialOutput::height);

[[nodiscard]] double evaluateRegionSurface(
    const RegionSurfaceOperation& operation,
    const Material& material,
    const RegionSample& region,
    double inputCoverage,
    double u,
    double v,
    MaterialOutput output = MaterialOutput::height);

} // namespace paperweight
