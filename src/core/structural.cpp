#include <paperweight/structural.hpp>

#include <paperweight/hash.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace paperweight {
namespace {

std::int64_t positiveModulo(std::int64_t value, std::uint32_t modulus)
{
    const auto signedModulus = static_cast<std::int64_t>(modulus);
    const auto remainder = value % signedModulus;
    return remainder < 0 ? remainder + signedModulus : remainder;
}

std::uint64_t structuralSeed(
    std::uint64_t materialSeed,
    std::uint64_t seedOffset,
    std::uint64_t domain)
{
    return mixBits(materialSeed ^ mixBits(seedOffset) ^ domain);
}

double rectangleCoverage(
    double localX,
    double localY,
    double width,
    double height,
    double softness)
{
    if (width == 1.0 && height == 1.0) {
        return 1.0;
    }
    const double distance = std::min(
        width * 0.5 - std::abs(localX),
        height * 0.5 - std::abs(localY));
    return smoothCoverage(distance, softness);
}

RegionSample gridRegion(
    std::uint64_t domain,
    const RepeatedCoordinate& horizontal,
    const RepeatedCoordinate& vertical)
{
    const double centreDistance = std::clamp(
        std::sqrt(horizontal.local * horizontal.local +
                  vertical.local * vertical.local) * std::sqrt(2.0),
        0.0,
        1.0);
    const double boundaryDistance = std::clamp(
        1.0 - 2.0 * std::max(
            std::abs(horizontal.local),
            std::abs(vertical.local)),
        0.0,
        1.0);
    return {
        makeRegionKey(domain, horizontal.index, vertical.index),
        horizontal.local + 0.5,
        vertical.local + 0.5,
        centreDistance,
        boundaryDistance,
        true,
    };
}

} // namespace

double wrapUnit(double value)
{
    return value - std::floor(value);
}

RepeatedCoordinate repeatedCoordinate(double value, std::uint32_t count)
{
    const double scaled = wrapUnit(value) * static_cast<double>(count);
    const double cell = std::floor(scaled);
    const auto index = std::min(
        static_cast<std::int64_t>(cell),
        static_cast<std::int64_t>(count) - 1);
    return {index, scaled - cell - 0.5};
}

double smoothCoverage(double signedDistance, double softness)
{
    if (softness == 0.0) {
        return signedDistance >= 0.0 ? 1.0 : 0.0;
    }
    const double value = std::clamp(signedDistance / softness + 0.5, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

double evaluateBrickGrid(
    const BrickGridOperation& operation,
    double u,
    double v)
{
    return evaluateBrickGridSample(operation, PhysicalSize{}, u, v).value;
}

double evaluateBrickGrid(
    const BrickGridOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v)
{
    return evaluateBrickGridSample(operation, materialSize, u, v).value;
}

StructuralSample evaluateBrickGridSample(
    const BrickGridOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v)
{
    constexpr std::uint64_t regionDomain = 0x627269636b677269ULL;
    if (operation.physicalDimensions) {
        const auto& physical = *operation.physicalDimensions;
        const auto columns = static_cast<std::uint32_t>(
            std::llround(materialSize.widthMetres / physical.widthMetres));
        const auto rows = static_cast<std::uint32_t>(
            std::llround(materialSize.heightMetres / physical.heightMetres));
        const auto vertical = repeatedCoordinate(v, rows);
        const bool offsetRow = (vertical.index % 2) != 0;
        const double offset = offsetRow
            ? operation.stagger / static_cast<double>(columns)
            : 0.0;
        const auto horizontal = repeatedCoordinate(u - offset, columns);
        const auto region = gridRegion(regionDomain, horizontal, vertical);
        if (physical.mortarMetres == 0.0) {
            return {1.0, region};
        }
        const double distanceX =
            (0.5 - std::abs(horizontal.local)) * physical.widthMetres -
            physical.mortarMetres * 0.5;
        const double distanceY =
            (0.5 - std::abs(vertical.local)) * physical.heightMetres -
            physical.mortarMetres * 0.5;
        const double softnessMetres = operation.softness *
            std::min(physical.widthMetres, physical.heightMetres);
        return {
            smoothCoverage(std::min(distanceX, distanceY), softnessMetres),
            region,
        };
    }

    const auto vertical = repeatedCoordinate(v, operation.rows);
    const bool offsetRow = (vertical.index % 2) != 0;
    const double offset = offsetRow
        ? operation.stagger / static_cast<double>(operation.columns)
        : 0.0;
    const auto horizontal = repeatedCoordinate(u - offset, operation.columns);
    const auto region = gridRegion(regionDomain, horizontal, vertical);
    if (operation.mortar == 0.0) {
        return {1.0, region};
    }
    if (operation.mortarSpace == BrickMortarSpace::texture) {
        const double distanceX =
            (0.5 - std::abs(horizontal.local)) / static_cast<double>(operation.columns) -
            operation.mortar * 0.5;
        const double distanceY =
            (0.5 - std::abs(vertical.local)) / static_cast<double>(operation.rows) -
            operation.mortar * 0.5;
        const double textureSoftness = operation.softness /
            static_cast<double>(std::max(operation.columns, operation.rows));
        return {
            smoothCoverage(std::min(distanceX, distanceY), textureSoftness),
            region,
        };
    }
    const double size = 1.0 - operation.mortar;
    return {
        rectangleCoverage(
            horizontal.local,
            vertical.local,
            size,
            size,
            operation.softness),
        region,
    };
}

double evaluateTileGrid(
    const TileGridOperation& operation,
    double u,
    double v)
{
    return evaluateTileGridSample(operation, u, v).value;
}

StructuralSample evaluateTileGridSample(
    const TileGridOperation& operation,
    double u,
    double v)
{
    constexpr std::uint64_t regionDomain = 0x74696c6567726964ULL;
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    const auto region = gridRegion(regionDomain, horizontal, vertical);
    if (operation.grout == 0.0) {
        return {1.0, region};
    }
    const double size = 1.0 - operation.grout;
    return {
        rectangleCoverage(
            horizontal.local,
            vertical.local,
            size,
            size,
            operation.softness),
        region,
    };
}

double evaluateWorleyCells(
    const WorleyCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    return evaluateWorleyCellsSample(operation, u, v, materialSeed).value;
}

StructuralSample evaluateWorleyCellsSample(
    const WorleyCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const double scaledX = wrapUnit(u) * static_cast<double>(operation.columns);
    const double scaledY = wrapUnit(v) * static_cast<double>(operation.rows);
    const auto baseX = static_cast<std::int64_t>(std::floor(scaledX));
    const auto baseY = static_cast<std::int64_t>(std::floor(scaledY));
    constexpr std::uint64_t domain = 0x37b1e4c96a2508dfULL;
    const auto seed = structuralSeed(materialSeed, operation.seedOffset, domain);

    double nearest = std::numeric_limits<double>::infinity();
    double secondNearest = std::numeric_limits<double>::infinity();
    std::int64_t nearestX = 0;
    std::int64_t nearestY = 0;
    double nearestDeltaX = 0.0;
    double nearestDeltaY = 0.0;
    for (std::int64_t offsetY = -2; offsetY <= 2; ++offsetY) {
        for (std::int64_t offsetX = -2; offsetX <= 2; ++offsetX) {
            const auto candidateX = baseX + offsetX;
            const auto candidateY = baseY + offsetY;
            const auto wrappedX = positiveModulo(candidateX, operation.columns);
            const auto wrappedY = positiveModulo(candidateY, operation.rows);
            const double randomX = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 0));
            const double randomY = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 1));
            const double featureX = static_cast<double>(candidateX) + 0.5 +
                (randomX - 0.5) * operation.jitter;
            const double featureY = static_cast<double>(candidateY) + 0.5 +
                (randomY - 0.5) * operation.jitter;
            const double deltaX = scaledX - featureX;
            const double deltaY = scaledY - featureY;
            const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
            if (distanceSquared < nearest) {
                secondNearest = nearest;
                nearest = distanceSquared;
                nearestX = wrappedX;
                nearestY = wrappedY;
                nearestDeltaX = deltaX;
                nearestDeltaY = deltaY;
            } else if (distanceSquared < secondNearest) {
                secondNearest = distanceSquared;
            }
        }
    }

    const double boundaryDistance = std::sqrt(secondNearest) - std::sqrt(nearest);
    const double value = std::clamp(boundaryDistance / operation.edgeWidth, 0.0, 1.0);
    constexpr std::uint64_t regionDomain = 0x776f726c65796365ULL;
    return {
        value * value * (3.0 - 2.0 * value),
        RegionSample{
            makeRegionKey(regionDomain, nearestX, nearestY),
            std::clamp(0.5 + nearestDeltaX * 0.5, 0.0, 1.0),
            std::clamp(0.5 + nearestDeltaY * 0.5, 0.0, 1.0),
            std::clamp(std::sqrt(nearest) / std::sqrt(2.0), 0.0, 1.0),
            value,
            true,
        },
    };
}

double evaluateRandomCells(
    const RandomCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    return evaluateRandomCellsSample(operation, u, v, materialSeed).value;
}

StructuralSample evaluateRandomCellsSample(
    const RandomCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    constexpr std::uint64_t domain = 0xc4297d15a8e306bfULL;
    const auto seed = structuralSeed(materialSeed, operation.seedOffset, domain);
    constexpr std::uint64_t regionDomain = 0x72616e646f6d6365ULL;
    return {
        unitDouble(hashCoordinates(seed, horizontal.index, vertical.index)),
        gridRegion(regionDomain, horizontal, vertical),
    };
}

double evaluateLines(
    const LinesOperation& operation,
    double u,
    double v)
{
    return evaluateLinesSample(operation, u, v).value;
}

StructuralSample evaluateLinesSample(
    const LinesOperation& operation,
    double u,
    double v)
{
    if (operation.width == 1.0) {
        const double coordinate = operation.direction == LineDirection::vertical ? u : v;
        const auto repeated = repeatedCoordinate(coordinate, operation.count);
        constexpr std::uint64_t regionDomain = 0x6c696e6573726567ULL;
        return {
            1.0,
            RegionSample{
                makeRegionKey(regionDomain, repeated.index, 0),
                operation.direction == LineDirection::vertical
                    ? repeated.local + 0.5 : wrapUnit(u),
                operation.direction == LineDirection::horizontal
                    ? repeated.local + 0.5 : wrapUnit(v),
                std::clamp(std::abs(repeated.local) * 2.0, 0.0, 1.0),
                std::clamp(1.0 - std::abs(repeated.local) * 2.0, 0.0, 1.0),
                true,
            },
        };
    }
    const double coordinate = operation.direction == LineDirection::vertical ? u : v;
    const auto repeated = repeatedCoordinate(coordinate, operation.count);
    const double distance = operation.width * 0.5 - std::abs(repeated.local);
    constexpr std::uint64_t regionDomain = 0x6c696e6573726567ULL;
    return {
        smoothCoverage(distance, operation.softness),
        RegionSample{
            makeRegionKey(regionDomain, repeated.index, 0),
            operation.direction == LineDirection::vertical
                ? repeated.local + 0.5 : wrapUnit(u),
            operation.direction == LineDirection::horizontal
                ? repeated.local + 0.5 : wrapUnit(v),
            std::clamp(std::abs(repeated.local) * 2.0, 0.0, 1.0),
            std::clamp(1.0 - std::abs(repeated.local) * 2.0, 0.0, 1.0),
            true,
        },
    };
}

double evaluateRectangles(
    const RectanglesOperation& operation,
    double u,
    double v)
{
    return evaluateRectanglesSample(operation, u, v).value;
}

StructuralSample evaluateRectanglesSample(
    const RectanglesOperation& operation,
    double u,
    double v)
{
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    constexpr std::uint64_t regionDomain = 0x72656374616e676cULL;
    return {
        rectangleCoverage(
            horizontal.local,
            vertical.local,
            operation.width,
            operation.height,
            operation.softness),
        gridRegion(regionDomain, horizontal, vertical),
    };
}

double evaluateCircles(
    const CirclesOperation& operation,
    double u,
    double v)
{
    return evaluateCirclesSample(operation, u, v).value;
}

StructuralSample evaluateCirclesSample(
    const CirclesOperation& operation,
    double u,
    double v)
{
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    const double distance = operation.radius - std::sqrt(
        horizontal.local * horizontal.local + vertical.local * vertical.local);
    constexpr std::uint64_t regionDomain = 0x636972636c657265ULL;
    return {
        smoothCoverage(distance, operation.softness),
        gridRegion(regionDomain, horizontal, vertical),
    };
}

} // namespace paperweight
