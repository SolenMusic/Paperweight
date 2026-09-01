#include <paperweight/sculpt.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/structural.hpp>

#include "noise_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace paperweight {
namespace {

constexpr std::uint64_t chipDomain = 0x7363756c70746368ULL;
constexpr std::uint64_t wearDomain = 0x7363756c70747772ULL;

double smoothStep(double value)
{
    const double t = std::clamp(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double profileHeight(
    BevelProfile profile,
    double distance,
    double handCutBlend)
{
    const double t = std::clamp(distance, 0.0, 1.0);
    switch (profile) {
    case BevelProfile::rounded:
        return std::sqrt(std::max(0.0, t * (2.0 - t)));
    case BevelProfile::chamfered:
        return t;
    case BevelProfile::handCut: {
        const double cut = handCutBlend < 0.5
            ? t * t
            : std::sqrt(t);
        return std::clamp(t * 0.72 + cut * 0.28, 0.0, 1.0);
    }
    }
    return 0.0;
}

struct UnitDirection {
    double x;
    double y;
};

UnitDirection regionDirection(
    std::uint64_t materialSeed,
    std::uint64_t regionKey,
    std::uint64_t seedOffset,
    std::uint32_t channel)
{
    double x = regionRandom(materialSeed, regionKey, seedOffset, channel) * 2.0 - 1.0;
    double y = regionRandom(materialSeed, regionKey, seedOffset, channel + 1U) * 2.0 - 1.0;
    const double length = std::sqrt(x * x + y * y);
    if (length < 1.0e-9) {
        return {1.0, 0.0};
    }
    return {x / length, y / length};
}

double selectedValue(
    const RegionSurfaceSample& sample,
    RegionSurfaceField field)
{
    switch (field) {
    case RegionSurfaceField::height:
        return sample.height;
    case RegionSurfaceField::cavity:
        return sample.cavity;
    case RegionSurfaceField::outerEdge:
        return sample.outerEdge;
    case RegionSurfaceField::exposedFace:
        return sample.exposedFace;
    case RegionSurfaceField::facet:
        return sample.facet;
    case RegionSurfaceField::wear:
        return sample.wear;
    }
    return 0.0;
}

} // namespace

RegionSurfaceSample evaluateRegionSurfaceFields(
    const RegionSurfaceOperation& operation,
    const Material& material,
    const RegionSample& region,
    double inputCoverage,
    double u,
    double v,
    MaterialOutput output)
{
    const double coverage = std::clamp(inputCoverage, 0.0, 1.0);
    if (!region.valid) {
        return {0.0, 1.0 - coverage, 0.0, 0.0, 0.0, 0.0};
    }

    const double handCutBlend = regionRandom(
        material.seed, region.key, operation.seedOffset, 2);
    const double peakX =
        (regionRandom(material.seed, region.key, operation.seedOffset, 3) * 2.0 - 1.0) *
        0.22;
    const double peakY =
        (regionRandom(material.seed, region.key, operation.seedOffset, 4) * 2.0 - 1.0) *
        0.22;
    const double localX = (region.localU - 0.5) * 2.0 - peakX;
    const double localY = (region.localV - 0.5) * 2.0 - peakY;

    const auto slopeDirection = regionDirection(
        material.seed, region.key, operation.seedOffset, 6);
    const double directionalSlope = 0.5 *
        (localX * slopeDirection.x + localY * slopeDirection.y);

    double strongestPlane = -std::numeric_limits<double>::infinity();
    double secondPlane = strongestPlane;
    for (std::uint32_t facet = 0; facet < operation.facetCount; ++facet) {
        const auto direction = regionDirection(
            material.seed,
            region.key,
            operation.seedOffset,
            16U + facet * 3U);
        const double bias =
            (regionRandom(
                material.seed,
                region.key,
                operation.seedOffset,
                18U + facet * 3U) - 0.5) * 0.16;
        const double plane =
            (localX * direction.x + localY * direction.y) * 0.5 + bias;
        if (plane > strongestPlane) {
            secondPlane = strongestPlane;
            strongestPlane = plane;
        } else if (plane > secondPlane) {
            secondPlane = plane;
        }
    }
    const double facetPlane = std::clamp(strongestPlane, -0.5, 0.5);
    const double facetSeparation = secondPlane == -std::numeric_limits<double>::infinity()
        ? 1.0
        : std::clamp(
            (strongestPlane - secondPlane) *
                static_cast<double>(operation.facetCount) * 1.5,
            0.0,
            1.0);

    const auto chipSeed = mixBits(
        material.seed ^ mixBits(operation.seedOffset) ^ region.key ^ chipDomain);
    const auto wearSeed = mixBits(
        material.seed ^ mixBits(operation.seedOffset) ^ region.key ^ wearDomain);
    const double scale = static_cast<double>(operation.chipScale);
    const double chipNoise = detail::periodicValueNoise2DUnchecked(
        wrapUnit(u) * scale,
        wrapUnit(v) * scale,
        operation.chipScale,
        operation.chipScale,
        chipSeed);
    const auto wearPeriod = std::max(1U, operation.chipScale / 2U);
    const double wearNoise = detail::periodicValueNoise2DUnchecked(
        wrapUnit(u) * static_cast<double>(wearPeriod),
        wrapUnit(v) * static_cast<double>(wearPeriod),
        wearPeriod,
        wearPeriod,
        wearSeed);

    const double boundary = std::clamp(region.boundaryDistance, 0.0, 1.0);
    const double edgeWindow = 1.0 - std::clamp(
        boundary / std::max(operation.bevelWidth * 1.35, 0.001),
        0.0,
        1.0);
    const double chip = operation.chipAmount * edgeWindow *
        std::clamp((chipNoise - 0.48) / 0.52, 0.0, 1.0);
    const double erosion = operation.erosionAmount * edgeWindow *
        (0.35 + wearNoise * 0.65);
    const double carvedBoundary = std::clamp(boundary - chip - erosion * 0.35, 0.0, 1.0);
    const double bevelCoordinate = std::clamp(
        carvedBoundary / operation.bevelWidth,
        0.0,
        1.0);
    const double bevel = profileHeight(
        operation.profile,
        bevelCoordinate,
        handCutBlend);
    const double outerEdge = coverage *
        std::clamp(1.0 - std::abs(bevelCoordinate * 2.0 - 1.0), 0.0, 1.0);
    const double wear = std::clamp(
        outerEdge * operation.wearAmount * (0.35 + wearNoise * 0.65) +
            coverage * erosion * 0.65,
        0.0,
        1.0);
    const double peakDistance = std::sqrt(localX * localX + localY * localY);
    const double centrePeak = std::clamp(1.0 - peakDistance / 1.4142135623730951, 0.0, 1.0);
    const double normalFacetBoost =
        output == MaterialOutput::normal && operation.facetedNormals
        ? std::max(0.18, (1.0 - operation.facetStrength) * 0.32)
        : 0.0;
    const double constructed = operation.bevelHeight * bevel +
        operation.centrePeak * centrePeak +
        (operation.facetStrength + normalFacetBoost) * facetPlane +
        operation.slopeStrength * directionalSlope -
        wear * 0.38;
    const double height = coverage * std::clamp(constructed, 0.0, 1.0);
    const double exposedFace = coverage * smoothStep(
        (bevelCoordinate - 0.68) / 0.32) * (1.0 - wear * 0.5);
    const double facetMask = exposedFace * facetSeparation;

    return {
        height,
        std::clamp(1.0 - height, 0.0, 1.0),
        outerEdge,
        std::clamp(exposedFace, 0.0, 1.0),
        std::clamp(facetMask, 0.0, 1.0),
        wear,
    };
}

double evaluateRegionSurface(
    const RegionSurfaceOperation& operation,
    const Material& material,
    const RegionSample& region,
    double inputCoverage,
    double u,
    double v,
    MaterialOutput output)
{
    return selectedValue(
        evaluateRegionSurfaceFields(
            operation,
            material,
            region,
            inputCoverage,
            u,
            v,
            output),
        operation.field);
}

} // namespace paperweight
