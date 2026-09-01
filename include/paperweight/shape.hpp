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

// Evaluate one bounded shape in local material-space coordinates. Width and
// height scales are independent so callers such as scatter populations can
// vary aspect without copying or mutating the authored stamp.
[[nodiscard]] double shapeSignedDistance(
    const ShapePrimitiveOperation& operation,
    double localX,
    double localY,
    double widthScale = 1.0,
    double heightScale = 1.0,
    double rotationOffsetDegrees = 0.0);

[[nodiscard]] double shapeFieldCoverage(
    const ShapePrimitiveOperation& operation,
    double signedDistance,
    double distanceScale = 1.0);

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
