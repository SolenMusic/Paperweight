#include <paperweight/surface.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/structural.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace paperweight {
namespace {

constexpr std::uint64_t ridgeDomain = 0xd173c69a4b285f01ULL;
constexpr std::uint64_t bandDomain = 0x5b26e1f90ca4738dULL;
constexpr std::uint64_t ringDomain = 0xa84cf207bd136e59ULL;
constexpr std::uint64_t scatterDomain = 0x34e7ab81c5f2906dULL;
constexpr std::uint64_t streakDomain = 0xf0158c42ae739bd6ULL;

std::int64_t positiveModulo(std::int64_t value, std::uint32_t modulus)
{
    const auto signedModulus = static_cast<std::int64_t>(modulus);
    const auto remainder = value % signedModulus;
    return remainder < 0 ? remainder + signedModulus : remainder;
}

double smoothStep(double edge0, double edge1, double value)
{
    if (edge0 == edge1) {
        return value >= edge1 ? 1.0 : 0.0;
    }
    const double t = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

std::uint64_t operationSeed(
    const Material& material,
    const SurfacePatternOperation& operation,
    std::uint64_t domain)
{
    return mixBits(material.seed ^ mixBits(operation.seedOffset) ^ domain);
}

double surfaceFbm(
    double u,
    double v,
    std::uint32_t scale,
    double detail,
    std::uint64_t seed)
{
    const auto octaves = static_cast<std::uint32_t>(1U + std::llround(detail * 4.0));
    double total = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    auto frequency = scale;
    for (std::uint32_t octave = 0; octave < octaves; ++octave) {
        total += periodicValueNoise2D(
            u * static_cast<double>(frequency),
            v * static_cast<double>(frequency),
            frequency,
            frequency,
            mixBits(seed ^ octave)) * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
        frequency *= 2U;
    }
    return total / amplitudeSum;
}

struct WarpedCoordinates {
    double u;
    double v;
};

WarpedCoordinates warpCoordinates(
    const SurfacePatternOperation& operation,
    double u,
    double v,
    std::uint64_t seed)
{
    if (operation.distortion == 0.0) {
        return {u, v};
    }
    const double displacementScale = operation.distortion /
        static_cast<double>(operation.scale);
    const double x = surfaceFbm(
        u,
        v,
        operation.scale,
        operation.detail,
        mixBits(seed ^ 0x91e6a4c3ULL));
    const double y = surfaceFbm(
        u,
        v,
        operation.scale,
        operation.detail,
        mixBits(seed ^ 0x27bd508fULL));
    return {
        u + (x * 2.0 - 1.0) * displacementScale,
        v + (y * 2.0 - 1.0) * displacementScale,
    };
}

double bandCoverage(double coordinate, double width, double detail)
{
    const double local = std::abs(wrapUnit(coordinate) - 0.5);
    const double halfWidth = width * 0.5;
    const double softness = 0.0025 + (1.0 - detail) * 0.08;
    return smoothCoverage(halfWidth - local, softness);
}

double evaluateRidged(
    const SurfacePatternOperation& operation,
    const Material& material,
    double u,
    double v)
{
    const auto seed = operationSeed(material, operation, ridgeDomain);
    const auto coordinates = warpCoordinates(operation, u, v, seed);
    const double source = surfaceFbm(
        coordinates.u,
        coordinates.v,
        operation.scale,
        operation.detail,
        seed);
    const double ridge = 1.0 - std::abs(source * 2.0 - 1.0);
    const double threshold = 1.0 - operation.width;
    double value = smoothStep(threshold - 0.08, threshold + 0.08, ridge);
    if (operation.variation > 0.0) {
        const double variation = surfaceFbm(
            coordinates.u,
            coordinates.v,
            operation.scale,
            operation.detail,
            mixBits(seed ^ 0xc51f87a2ULL));
        value *= 1.0 - operation.variation + operation.variation * variation;
    }
    return std::clamp(value, 0.0, 1.0);
}

double evaluateBands(
    const SurfacePatternOperation& operation,
    const Material& material,
    double u,
    double v)
{
    const auto seed = operationSeed(material, operation, bandDomain);
    const auto coordinates = warpCoordinates(operation, u, v, seed);
    const double phaseNoise = surfaceFbm(
        coordinates.u,
        coordinates.v,
        operation.scale,
        operation.detail,
        mixBits(seed ^ 0x492db761ULL));
    const double phase = (phaseNoise - 0.5) * operation.variation;
    return bandCoverage(
        coordinates.u * static_cast<double>(operation.scale) + phase,
        operation.width,
        operation.detail);
}

double evaluateRings(
    const SurfacePatternOperation& operation,
    const Material& material,
    double u,
    double v)
{
    const auto seed = operationSeed(material, operation, ringDomain);
    const auto coordinates = warpCoordinates(operation, u, v, seed);
    const double x = std::sin(std::numbers::pi * coordinates.u);
    const double y = std::sin(std::numbers::pi * coordinates.v);
    const double radius = std::sqrt(x * x + y * y);
    const double phaseNoise = surfaceFbm(
        coordinates.u,
        coordinates.v,
        operation.scale,
        operation.detail,
        mixBits(seed ^ 0xb615c2e9ULL));
    const double phase = (phaseNoise - 0.5) * operation.variation;
    return bandCoverage(
        radius * static_cast<double>(operation.scale) + phase,
        operation.width,
        operation.detail);
}

double evaluateScatter(
    const SurfacePatternOperation& operation,
    const Material& material,
    double u,
    double v)
{
    const auto seed = operationSeed(material, operation, scatterDomain);
    const auto coordinates = warpCoordinates(operation, u, v, seed);
    const double scale = static_cast<double>(operation.scale);
    const double x = coordinates.u * scale;
    const double y = coordinates.v * scale;
    const auto baseX = static_cast<std::int64_t>(std::floor(x));
    const auto baseY = static_cast<std::int64_t>(std::floor(y));
    double best = 0.0;
    for (std::int64_t offsetY = -1; offsetY <= 1; ++offsetY) {
        for (std::int64_t offsetX = -1; offsetX <= 1; ++offsetX) {
            const auto cellX = baseX + offsetX;
            const auto cellY = baseY + offsetY;
            const auto wrappedX = positiveModulo(cellX, operation.scale);
            const auto wrappedY = positiveModulo(cellY, operation.scale);
            const double randomX = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 0));
            const double randomY = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 1));
            const double randomSize = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 2));
            const double centreX = static_cast<double>(cellX) + 0.5 +
                (randomX - 0.5) * operation.variation;
            const double centreY = static_cast<double>(cellY) + 0.5 +
                (randomY - 0.5) * operation.variation;
            const double radius = operation.width * 0.5 *
                (1.0 - operation.variation * 0.6 +
                 operation.variation * 0.6 * randomSize);
            const double distance = std::hypot(x - centreX, y - centreY);
            const double softness = 0.005 + (1.0 - operation.detail) * 0.08;
            best = std::max(best, smoothCoverage(radius - distance, softness));
        }
    }
    return best;
}

double pointSegmentDistance(
    double px,
    double py,
    double ax,
    double ay,
    double bx,
    double by)
{
    const double segmentX = bx - ax;
    const double segmentY = by - ay;
    const double lengthSquared = segmentX * segmentX + segmentY * segmentY;
    if (lengthSquared == 0.0) {
        return std::hypot(px - ax, py - ay);
    }
    const double amount = std::clamp(
        ((px - ax) * segmentX + (py - ay) * segmentY) / lengthSquared,
        0.0,
        1.0);
    return std::hypot(px - (ax + segmentX * amount), py - (ay + segmentY * amount));
}

double evaluateStreaks(
    const SurfacePatternOperation& operation,
    const Material& material,
    double u,
    double v)
{
    const auto seed = operationSeed(material, operation, streakDomain);
    const auto coordinates = warpCoordinates(operation, u, v, seed);
    const double scale = static_cast<double>(operation.scale);
    const double x = coordinates.u * scale;
    const double y = coordinates.v * scale;
    const auto baseX = static_cast<std::int64_t>(std::floor(x));
    const auto baseY = static_cast<std::int64_t>(std::floor(y));
    double best = 0.0;
    for (std::int64_t offsetY = -1; offsetY <= 1; ++offsetY) {
        for (std::int64_t offsetX = -1; offsetX <= 1; ++offsetX) {
            const auto cellX = baseX + offsetX;
            const auto cellY = baseY + offsetY;
            const auto wrappedX = positiveModulo(cellX, operation.scale);
            const auto wrappedY = positiveModulo(cellY, operation.scale);
            const double randomX = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 0));
            const double randomY = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 1));
            const double randomAngle = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 2));
            const double randomLength = unitDouble(hashCoordinates(seed, wrappedX, wrappedY, 3));
            const double centreX = static_cast<double>(cellX) + randomX;
            const double centreY = static_cast<double>(cellY) + randomY;
            const double angle = (randomAngle - 0.5) * operation.variation *
                std::numbers::pi;
            const double length = 0.15 + operation.detail * 0.75 *
                (1.0 - operation.variation * 0.5 +
                 operation.variation * 0.5 * randomLength);
            const double directionX = std::sin(angle) * length * 0.5;
            const double directionY = std::cos(angle) * length * 0.5;
            const double distance = pointSegmentDistance(
                x,
                y,
                centreX - directionX,
                centreY - directionY,
                centreX + directionX,
                centreY + directionY);
            const double halfWidth = operation.width * 0.5;
            const double softness = 0.002 + (1.0 - operation.detail) * 0.04;
            best = std::max(best, smoothCoverage(halfWidth - distance, softness));
        }
    }
    return best;
}

} // namespace

double evaluateSurfacePattern(
    const SurfacePatternOperation& operation,
    const Material& material,
    double u,
    double v)
{
    switch (operation.kind) {
    case SurfacePatternKind::ridgedNoise:
        return evaluateRidged(operation, material, u, v);
    case SurfacePatternKind::bands:
        return evaluateBands(operation, material, u, v);
    case SurfacePatternKind::rings:
        return evaluateRings(operation, material, u, v);
    case SurfacePatternKind::scatter:
        return evaluateScatter(operation, material, u, v);
    case SurfacePatternKind::streaks:
        return evaluateStreaks(operation, material, u, v);
    }
    return 0.0;
}

double evaluateSurfaceFilter(
    const SurfaceFilterOperation& operation,
    const SurfaceNeighbourhood& neighbourhood)
{
    const std::array<double, 9> samples{
        neighbourhood.centre,
        neighbourhood.left,
        neighbourhood.right,
        neighbourhood.up,
        neighbourhood.down,
        neighbourhood.upperLeft,
        neighbourhood.upperRight,
        neighbourhood.lowerLeft,
        neighbourhood.lowerRight,
    };
    const double neighbours =
        (neighbourhood.left + neighbourhood.right + neighbourhood.up +
         neighbourhood.down + neighbourhood.upperLeft + neighbourhood.upperRight +
         neighbourhood.lowerLeft + neighbourhood.lowerRight) / 8.0;
    double filtered = neighbourhood.centre;
    switch (operation.kind) {
    case SurfaceFilterKind::invert:
        filtered = 1.0 - neighbourhood.centre;
        break;
    case SurfaceFilterKind::soften:
        filtered = (neighbourhood.centre + neighbourhood.left +
                    neighbourhood.right + neighbourhood.up + neighbourhood.down +
                    neighbourhood.upperLeft + neighbourhood.upperRight +
                    neighbourhood.lowerLeft + neighbourhood.lowerRight) / 9.0;
        break;
    case SurfaceFilterKind::expand:
        filtered = *std::max_element(samples.begin(), samples.end());
        break;
    case SurfaceFilterKind::contract:
        filtered = *std::min_element(samples.begin(), samples.end());
        break;
    case SurfaceFilterKind::edge:
        filtered = *std::max_element(samples.begin(), samples.end()) -
            *std::min_element(samples.begin(), samples.end());
        break;
    case SurfaceFilterKind::slope: {
        const double x = (neighbourhood.right - neighbourhood.left) * 0.5;
        const double y = (neighbourhood.down - neighbourhood.up) * 0.5;
        filtered = std::clamp(std::hypot(x, y) * std::sqrt(2.0), 0.0, 1.0);
        break;
    }
    case SurfaceFilterKind::cavity:
        filtered = std::clamp((neighbours - neighbourhood.centre) * 2.0, 0.0, 1.0);
        break;
    case SurfaceFilterKind::peaks:
        filtered = std::clamp((neighbourhood.centre - neighbours) * 2.0, 0.0, 1.0);
        break;
    }
    return std::clamp(
        neighbourhood.centre +
            (filtered - neighbourhood.centre) * operation.strength,
        0.0,
        1.0);
}

} // namespace paperweight
