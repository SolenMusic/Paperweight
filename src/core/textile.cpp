#include <paperweight/textile.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/region.hpp>
#include <paperweight/structural.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace paperweight {
namespace {

constexpr std::uint64_t textileDomain = 0x74657874696c6573ULL;

double interpolate(double low, double high, double amount)
{
    return low + (high - low) * amount;
}

double smoothStep(double value)
{
    const double clamped = std::clamp(value, 0.0, 1.0);
    return clamped * clamped * (3.0 - 2.0 * clamped);
}

std::uint8_t interpolateChannel(std::uint8_t low, std::uint8_t high, double amount)
{
    return static_cast<std::uint8_t>(std::llround(interpolate(
        static_cast<double>(low),
        static_cast<double>(high),
        std::clamp(amount, 0.0, 1.0))));
}

Rgba8 interpolateColour(const Rgba8& low, const Rgba8& high, double amount)
{
    return {
        interpolateChannel(low.red, high.red, amount),
        interpolateChannel(low.green, high.green, amount),
        interpolateChannel(low.blue, high.blue, amount),
        interpolateChannel(low.alpha, high.alpha, amount),
    };
}

std::int64_t positiveModulo(std::int64_t value, std::uint32_t modulus)
{
    const auto signedModulus = static_cast<std::int64_t>(modulus);
    const auto remainder = value % signedModulus;
    return remainder < 0 ? remainder + signedModulus : remainder;
}

double signedHash(std::uint64_t seed, std::int64_t x, std::int64_t y, std::uint64_t stream)
{
    return unitDouble(hashCoordinates(seed, x, y, stream)) * 2.0 - 1.0;
}

struct OrientedCoordinate {
    double u{};
    double v{};
    std::int64_t tileX{};
    std::int64_t tileY{};
    bool rotated{};
};

OrientedCoordinate orientCoordinate(
    const TextileOperation& operation,
    double u,
    double v)
{
    const double scaledU = wrapUnit(u) * static_cast<double>(operation.tileColumns);
    const double scaledV = wrapUnit(v) * static_cast<double>(operation.tileRows);
    const auto tileX = static_cast<std::int64_t>(std::floor(scaledU));
    const auto tileY = static_cast<std::int64_t>(std::floor(scaledV));
    const double localU = scaledU - std::floor(scaledU);
    const double localV = scaledV - std::floor(scaledV);

    bool rotated = false;
    switch (operation.tileOrientation) {
    case TextileTileOrientation::uniform:
        break;
    case TextileTileOrientation::alternatingRows:
        rotated = positiveModulo(tileY, 2) != 0;
        break;
    case TextileTileOrientation::alternatingColumns:
        rotated = positiveModulo(tileX, 2) != 0;
        break;
    case TextileTileOrientation::checkerboard:
        rotated = positiveModulo(tileX + tileY, 2) != 0;
        break;
    }

    if (!rotated) {
        return {localU, localV, tileX, tileY, false};
    }
    return {localV, 1.0 - localU, tileX, tileY, true};
}

double yarnCrossSection(
    YarnProfile profile,
    double normalisedDistance,
    double roundness,
    double tangent,
    double twist)
{
    const double distance = std::clamp(normalisedDistance, 0.0, 1.0);
    const double dome = std::sqrt(std::max(0.0, 1.0 - distance * distance));
    double result = interpolate(1.0 - distance, dome, roundness);
    if (profile == YarnProfile::flat) {
        result = interpolate(result, smoothStep((1.0 - distance) * 2.0), 0.72);
    } else if (profile == YarnProfile::twisted) {
        const double twistWave = 0.5 + 0.5 * std::sin(
            2.0 * std::numbers::pi * (tangent * 2.0 + twist));
        result *= 0.78 + twistWave * 0.22;
    }
    return std::clamp(result, 0.0, 1.0);
}

bool warpOnTop(
    const TextileOperation& operation,
    std::int64_t column,
    std::int64_t row)
{
    const auto span = operation.weaveSpan;
    switch (operation.pattern) {
    case TextilePattern::plainWeave:
        return positiveModulo(column + row, 2) == 0;
    case TextilePattern::basketWeave:
        return positiveModulo(
            column / static_cast<std::int64_t>(span) +
                row / static_cast<std::int64_t>(span),
            2) == 0;
    case TextilePattern::twillWeave:
        return positiveModulo(
            column - row * static_cast<std::int64_t>(operation.twillStep),
            span * 2U) < static_cast<std::int64_t>(span);
    case TextilePattern::loopPile:
    case TextilePattern::cutPile:
        return false;
    }
    return false;
}

double threadOffset(
    std::uint64_t seed,
    std::int64_t fixedIndex,
    std::int64_t segmentIndex,
    double segmentLocal,
    std::uint64_t stream,
    double jitter)
{
    const double first = signedHash(seed, fixedIndex, segmentIndex, stream);
    const double second = signedHash(seed, fixedIndex, segmentIndex + 1, stream);
    return interpolate(first, second, smoothStep(segmentLocal)) * jitter * 0.34;
}

struct ThreadSample {
    double coverage{};
    double height{};
    double fibre{};
    double damage{};
    double colourVariation{};
    bool accent{};
    std::uint64_t key{};
};

ThreadSample sampleThread(
    const TextileOperation& operation,
    std::uint64_t seed,
    std::int64_t fixedIndex,
    std::int64_t segmentIndex,
    double across,
    double along,
    bool warp)
{
    const std::uint64_t directionStream = warp ? 0x77617270ULL : 0x77656674ULL;
    const auto key = hashCoordinates(seed, fixedIndex, 0, directionStream);
    const auto segmentKey = hashCoordinates(seed, fixedIndex, segmentIndex, directionStream);
    const double missing = unitDouble(mixBits(key ^ 0x6d697373696e67ULL)) <
        operation.missingAmount ? 1.0 : 0.0;

    const double subsegment = along * static_cast<double>(operation.fibreFrequency);
    const auto fibreIndex = static_cast<std::int64_t>(std::floor(subsegment));
    const double fibreLocal = subsegment - std::floor(subsegment);
    const double damageHash = unitDouble(hashCoordinates(
        segmentKey,
        fibreIndex,
        0,
        0x64616d616765ULL));
    const double damageShape = 1.0 - std::abs(fibreLocal * 2.0 - 1.0);
    const double damage = damageHash < operation.damageAmount
        ? smoothStep(damageShape)
        : 0.0;

    const double halfWidth = operation.yarnWidth * 0.5;
    const double signedDistance = std::abs(across) - halfWidth;
    double coverage = smoothCoverage(signedDistance, operation.softness);
    coverage *= (1.0 - missing) * (1.0 - damage);
    const double normalisedDistance = std::abs(across) / std::max(halfWidth, 0.000001);
    const double fibrePhase = unitDouble(mixBits(segmentKey ^ 0x666962726573ULL));
    const double fibre = 0.5 + 0.5 * std::sin(
        2.0 * std::numbers::pi *
        (along * static_cast<double>(operation.fibreFrequency) + fibrePhase));
    const double height = yarnCrossSection(
        operation.yarnProfile,
        normalisedDistance,
        operation.yarnRoundness,
        along,
        operation.twist + fibrePhase) *
        (1.0 + (fibre * 2.0 - 1.0) * operation.fibreStrength) * coverage;
    const double variation = unitDouble(mixBits(key ^ 0x636f6c6f7572ULL));
    const bool accent = unitDouble(mixBits(segmentKey ^ 0x616363656e74ULL)) <
        operation.differentColourAmount;
    return {
        coverage,
        std::clamp(height, 0.0, 1.0),
        fibre * coverage,
        std::max(missing, damage) * smoothCoverage(signedDistance, operation.softness),
        variation,
        accent,
        key,
    };
}

Rgba8 threadColour(const TextileOperation& operation, const ThreadSample& thread)
{
    if (thread.accent) {
        return operation.accentColour;
    }
    const double amount = std::clamp(
        0.5 + (thread.colourVariation - 0.5) * operation.colourVariation,
        0.0,
        1.0);
    return interpolateColour(operation.lowColour, operation.highColour, amount);
}

TextileSample evaluateWeave(
    const TextileOperation& operation,
    const OrientedCoordinate& coordinate,
    std::uint64_t seed)
{
    const double scaledU = coordinate.u * static_cast<double>(operation.columns);
    const double scaledV = coordinate.v * static_cast<double>(operation.rows);
    const auto column = static_cast<std::int64_t>(std::floor(scaledU));
    const auto row = static_cast<std::int64_t>(std::floor(scaledV));
    const double cellU = scaledU - std::floor(scaledU);
    const double cellV = scaledV - std::floor(scaledV);
    const double warpOffset = threadOffset(
        seed, column, row, cellV, 1, operation.jitter);
    const double weftOffset = threadOffset(
        seed, row, column, cellU, 2, operation.jitter);
    const auto warp = sampleThread(
        operation, seed, column, row, cellU - 0.5 - warpOffset, cellV, true);
    const auto weft = sampleThread(
        operation, seed, row, column, cellV - 0.5 - weftOffset, cellU, false);
    const bool topIsWarp = warpOnTop(operation, column, row);
    const auto& top = topIsWarp ? warp : weft;
    const auto& bottom = topIsWarp ? weft : warp;
    const double topHeight = top.height + operation.crossingHeight * top.coverage;
    const double bottomHeight = bottom.height * (1.0 - operation.crossingHeight * top.coverage);
    const bool topVisible = top.coverage >= bottom.coverage * 0.35;
    const auto& visible = topVisible ? top : bottom;
    const double coverage = std::max(warp.coverage, weft.coverage);
    const double height = std::clamp(std::max(topHeight, bottomHeight), 0.0, 1.0);
    Rgba8 colour = threadColour(operation, visible);
    const double damage = std::max(warp.damage, weft.damage);
    if (damage > 0.0) {
        colour = interpolateColour(colour, operation.damageColour, damage);
    }
    double direction = topVisible && topIsWarp ? 0.0 : 0.5;
    if (coordinate.rotated) {
        direction = std::fmod(direction + 0.5, 1.0);
    }
    const auto regionKey = visible.key;
    return {
        coverage,
        height,
        warp.coverage,
        weft.coverage,
        (topIsWarp ? 1.0 : 0.0) * std::min(warp.coverage, weft.coverage),
        std::max(warp.fibre, weft.fibre),
        0.0,
        damage,
        visible.colourVariation * coverage,
        direction * coverage,
        std::clamp(0.92 - height * 0.2 + visible.fibre * 0.08, 0.0, 1.0),
        colour,
        RegionSample{
            regionKey,
            cellU,
            cellV,
            std::hypot(cellU - 0.5, cellV - 0.5),
            std::min(0.5 - std::abs(cellU - 0.5), 0.5 - std::abs(cellV - 0.5)),
            coverage > 0.0,
            makeRegionKey(seed, coordinate.tileX, coordinate.tileY),
            true,
            coordinate.rotated ? 0.25 : 0.0,
        },
    };
}

double torusDelta(double from, double to, double period)
{
    double delta = from - to;
    delta -= std::round(delta / period) * period;
    return delta;
}

TextileSample evaluatePile(
    const TextileOperation& operation,
    const OrientedCoordinate& coordinate,
    std::uint64_t seed)
{
    const double scaledU = coordinate.u * static_cast<double>(operation.columns);
    const double scaledV = coordinate.v * static_cast<double>(operation.rows);
    const auto baseColumn = static_cast<std::int64_t>(std::floor(scaledU));
    const auto baseRow = static_cast<std::int64_t>(std::floor(scaledV));
    const double periodU = static_cast<double>(operation.columns);
    const double periodV = static_cast<double>(operation.rows);
    double bestCoverage = 0.0;
    double bestHeight = 0.0;
    double bestDistance = std::numeric_limits<double>::max();
    double bestAngle = 0.0;
    double bestVariation = 0.0;
    double bestDamage = 0.0;
    double bestFibre = 0.0;
    bool bestAccent = false;
    std::uint64_t bestKey = 0;

    for (std::int64_t rowOffset = -1; rowOffset <= 1; ++rowOffset) {
        for (std::int64_t columnOffset = -1; columnOffset <= 1; ++columnOffset) {
            const auto column = baseColumn + columnOffset;
            const auto row = baseRow + rowOffset;
            const auto wrappedColumn = positiveModulo(column, operation.columns);
            const auto wrappedRow = positiveModulo(row, operation.rows);
            const auto key = hashCoordinates(seed, wrappedColumn, wrappedRow, 0x70696c65ULL);
            if (unitDouble(mixBits(key ^ 0x6d697373696e67ULL)) < operation.missingAmount) {
                continue;
            }
            const double jitterX = signedHash(seed, wrappedColumn, wrappedRow, 20) *
                operation.jitter * 0.42;
            const double jitterY = signedHash(seed, wrappedColumn, wrappedRow, 21) *
                operation.jitter * 0.42;
            const double centreU = static_cast<double>(column) + 0.5 + jitterX;
            const double centreV = static_cast<double>(row) + 0.5 + jitterY;
            const double deltaU = torusDelta(scaledU, centreU, periodU);
            const double deltaV = torusDelta(scaledV, centreV, periodV);
            const double distance = std::hypot(deltaU, deltaV);
            const double damageHash = unitDouble(mixBits(key ^ 0x64616d616765ULL));
            const double damage = damageHash < operation.damageAmount
                ? smoothStep(1.0 - distance / std::max(operation.pileRadius, 0.000001))
                : 0.0;
            const double radial = std::abs(distance - operation.pileRadius);
            const double signedDistance = operation.pattern == TextilePattern::loopPile
                ? radial - operation.yarnWidth * operation.pileRadius * 0.32
                : distance - operation.pileRadius;
            const double coverage = smoothCoverage(signedDistance, operation.softness) *
                (1.0 - damage);
            if (coverage < bestCoverage && distance >= bestDistance) {
                continue;
            }
            const double dome = operation.pattern == TextilePattern::loopPile
                ? std::sqrt(std::max(0.0, 1.0 - std::pow(
                    radial / std::max(operation.yarnWidth * operation.pileRadius * 0.32, 0.000001),
                    2.0)))
                : std::sqrt(std::max(0.0, 1.0 - std::pow(
                    distance / std::max(operation.pileRadius, 0.000001),
                    2.0)));
            const double angle = std::atan2(deltaV, deltaU) /
                (2.0 * std::numbers::pi) + 0.5;
            const double fibrePhase = unitDouble(mixBits(key ^ 0x666962726573ULL));
            // Loop yarn exposes fibres around each ring. Cut pile instead follows
            // one authored direction across the complete carpet tile: orientCoordinate
            // rotates that direction for alternating tiles while the tuft domes stay
            // local. Keeping this signal at tile scale makes fibre lay visible at
            // practical preview resolutions rather than hiding it inside each tuft.
            const double fibreCoordinate = operation.pattern == TextilePattern::loopPile
                ? angle
                : coordinate.u;
            const double fibre = 0.5 + 0.5 * std::sin(
                2.0 * std::numbers::pi *
                (fibreCoordinate * static_cast<double>(operation.fibreFrequency) +
                 fibrePhase));
            bestCoverage = coverage;
            bestHeight = std::clamp(
                dome * operation.pileHeight *
                (1.0 + (fibre * 2.0 - 1.0) * operation.fibreStrength),
                0.0,
                1.0);
            bestDistance = distance;
            bestAngle = angle;
            bestVariation = unitDouble(mixBits(key ^ 0x636f6c6f7572ULL));
            bestDamage = damage;
            bestFibre = fibre;
            bestAccent = unitDouble(mixBits(key ^ 0x616363656e74ULL)) <
                operation.differentColourAmount;
            bestKey = key;
        }
    }

    Rgba8 colour = bestAccent
        ? operation.accentColour
        : interpolateColour(
            operation.lowColour,
            operation.highColour,
            std::clamp(
                0.5 + (bestVariation - 0.5) * operation.colourVariation +
                    (bestFibre - 0.5) * operation.fibreStrength * 0.35,
                0.0,
                1.0));
    if (bestDamage > 0.0) {
        colour = interpolateColour(colour, operation.damageColour, bestDamage);
    }
    double direction = bestAngle;
    if (coordinate.rotated) {
        direction = std::fmod(direction + 0.25, 1.0);
    }
    return {
        bestCoverage,
        bestHeight,
        0.0,
        0.0,
        0.0,
        bestCoverage * bestFibre,
        bestCoverage,
        bestDamage,
        bestVariation * bestCoverage,
        direction * bestCoverage,
        std::clamp(1.0 - bestHeight * 0.12 + bestCoverage * 0.08, 0.0, 1.0),
        colour,
        RegionSample{
            bestKey,
            std::cos(bestAngle * 2.0 * std::numbers::pi) * bestDistance,
            std::sin(bestAngle * 2.0 * std::numbers::pi) * bestDistance,
            bestDistance,
            operation.pileRadius - bestDistance,
            bestCoverage > 0.0,
            makeRegionKey(seed, coordinate.tileX, coordinate.tileY),
            true,
        },
    };
}

} // namespace

TextileSample evaluateTextile(
    const TextileOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const auto coordinate = orientCoordinate(operation, u, v);
    const auto tileKey = makeRegionKey(
        mixBits(materialSeed ^ operation.seedOffset ^ textileDomain),
        coordinate.tileX,
        coordinate.tileY);
    const auto seed = mixBits(tileKey ^ textileDomain);
    if (operation.pattern == TextilePattern::loopPile ||
        operation.pattern == TextilePattern::cutPile) {
        return evaluatePile(operation, coordinate, seed);
    }
    return evaluateWeave(operation, coordinate, seed);
}

} // namespace paperweight
