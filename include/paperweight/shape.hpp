#pragma once

#include <paperweight/layer.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

struct ShapeSample {
    double value{};
    double signedDistance{};
    RegionSample region;

    friend constexpr bool operator==(const ShapeSample&, const ShapeSample&) = default;
};

// signedDistance is negative inside the selected repeated shape and positive outside.
// Local rotation applies to each bounded instance; it never rotates the periodic domain.
[[nodiscard]] ShapeSample evaluateShapePrimitive(
    const ShapePrimitiveOperation& operation,
    double u,
    double v);

// Lattice windings are integer cycles across the tile and therefore repeat exactly.
[[nodiscard]] ShapeSample evaluateLattice(
    const LatticeOperation& operation,
    double u,
    double v);

[[nodiscard]] double combineShapeMasks(
    double input,
    double shape,
    ShapeBooleanMode mode);

} // namespace paperweight
