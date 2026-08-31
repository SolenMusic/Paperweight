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
    if (operation.mortar == 0.0) {
        return 1.0;
    }
    const auto vertical = repeatedCoordinate(v, operation.rows);
    const bool offsetRow = (vertical.index % 2) != 0;
    const double offset = offsetRow
        ? operation.stagger / static_cast<double>(operation.columns)
        : 0.0;
    const auto horizontal = repeatedCoordinate(u - offset, operation.columns);
    if (operation.mortarSpace == BrickMortarSpace::texture) {
        const double distanceX =
            (0.5 - std::abs(horizontal.local)) / static_cast<double>(operation.columns) -
            operation.mortar * 0.5;
        const double distanceY =
            (0.5 - std::abs(vertical.local)) / static_cast<double>(operation.rows) -
            operation.mortar * 0.5;
        const double textureSoftness = operation.softness /
            static_cast<double>(std::max(operation.columns, operation.rows));
        return smoothCoverage(std::min(distanceX, distanceY), textureSoftness);
    }
    const double size = 1.0 - operation.mortar;
    return rectangleCoverage(
        horizontal.local,
        vertical.local,
        size,
        size,
        operation.softness);
}

double evaluateTileGrid(
    const TileGridOperation& operation,
    double u,
    double v)
{
    if (operation.grout == 0.0) {
        return 1.0;
    }
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    const double size = 1.0 - operation.grout;
    return rectangleCoverage(
        horizontal.local,
        vertical.local,
        size,
        size,
        operation.softness);
}

double evaluateWorleyCells(
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
            } else if (distanceSquared < secondNearest) {
                secondNearest = distanceSquared;
            }
        }
    }

    const double boundaryDistance = std::sqrt(secondNearest) - std::sqrt(nearest);
    const double value = std::clamp(boundaryDistance / operation.edgeWidth, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

double evaluateRandomCells(
    const RandomCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    constexpr std::uint64_t domain = 0xc4297d15a8e306bfULL;
    const auto seed = structuralSeed(materialSeed, operation.seedOffset, domain);
    return unitDouble(hashCoordinates(seed, horizontal.index, vertical.index));
}

double evaluateLines(
    const LinesOperation& operation,
    double u,
    double v)
{
    if (operation.width == 1.0) {
        return 1.0;
    }
    const double coordinate = operation.direction == LineDirection::vertical ? u : v;
    const auto repeated = repeatedCoordinate(coordinate, operation.count);
    const double distance = operation.width * 0.5 - std::abs(repeated.local);
    return smoothCoverage(distance, operation.softness);
}

double evaluateRectangles(
    const RectanglesOperation& operation,
    double u,
    double v)
{
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    return rectangleCoverage(
        horizontal.local,
        vertical.local,
        operation.width,
        operation.height,
        operation.softness);
}

double evaluateCircles(
    const CirclesOperation& operation,
    double u,
    double v)
{
    const auto horizontal = repeatedCoordinate(u, operation.columns);
    const auto vertical = repeatedCoordinate(v, operation.rows);
    const double distance = operation.radius - std::sqrt(
        horizontal.local * horizontal.local + vertical.local * vertical.local);
    return smoothCoverage(distance, operation.softness);
}

} // namespace paperweight
