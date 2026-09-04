#include <paperweight/region_detail.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/shape.hpp>
#include <paperweight/structural.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace paperweight {
namespace {

constexpr std::uint64_t attachmentDomain = 0x7265676174746163ULL;

struct Point {
    double x{};
    double y{};
};

double randomChannel(
    const RegionSample& region,
    std::uint64_t materialSeed,
    std::uint64_t seedOffset,
    std::uint32_t attachmentIndex,
    std::uint32_t channel)
{
    return regionRandom(
        materialSeed,
        mixBits(region.key ^ mixBits(attachmentIndex) ^ attachmentDomain),
        seedOffset,
        channel);
}

double pointSegmentDistance(Point point, Point start, Point end)
{
    const Point edge{end.x - start.x, end.y - start.y};
    const Point offset{point.x - start.x, point.y - start.y};
    const double lengthSquared = edge.x * edge.x + edge.y * edge.y;
    if (lengthSquared <= std::numeric_limits<double>::epsilon()) {
        return std::hypot(offset.x, offset.y);
    }
    const double amount = std::clamp(
        (offset.x * edge.x + offset.y * edge.y) / lengthSquared,
        0.0,
        1.0);
    return std::hypot(
        offset.x - edge.x * amount,
        offset.y - edge.y * amount);
}

Point rotate(Point point, double turns)
{
    const double radians = turns * 2.0 * std::numbers::pi;
    const double sine = std::sin(radians);
    const double cosine = std::cos(radians);
    return {
        cosine * point.x + sine * point.y,
        -sine * point.x + cosine * point.y,
    };
}

double glyphDistance(RegionGlyph glyph, Point point, double size, double width)
{
    const double half = size * 0.5;
    const double stroke = width * 0.5;
    const auto segment = [point, stroke](Point start, Point end) {
        return pointSegmentDistance(point, start, end) - stroke;
    };
    switch (glyph) {
    case RegionGlyph::cross:
        return std::min(
            segment({-half, 0.0}, {half, 0.0}),
            segment({0.0, -half}, {0.0, half}));
    case RegionGlyph::chevron:
        return std::min(
            segment({-half, -half * 0.65}, {0.0, half * 0.65}),
            segment({0.0, half * 0.65}, {half, -half * 0.65}));
    case RegionGlyph::triangle:
        return std::min({
            segment({0.0, half}, {-half, -half}),
            segment({-half, -half}, {half, -half}),
            segment({half, -half}, {0.0, half}),
        });
    case RegionGlyph::rune:
        return std::min({
            segment({-half * 0.45, -half}, {-half * 0.45, half}),
            segment({-half * 0.45, half}, {half * 0.45, half * 0.2}),
            segment({-half * 0.45, 0.0}, {half * 0.5, -half}),
        });
    }
    return 1.0;
}

double primitiveDistance(
    const RegionAttachmentOperation& operation,
    Point point,
    double variation,
    std::uint32_t angularChannel)
{
    ShapePrimitiveOperation shape;
    shape.columns = 1;
    shape.rows = 1;
    shape.width = operation.size;
    shape.height = operation.size / operation.aspect;
    shape.cornerRadius = operation.size * 0.12;
    shape.softness = operation.softness;
    switch (operation.kind) {
    case RegionAttachmentKind::fastener:
        shape.kind = ShapePrimitiveKind::ellipse;
        return shapeSignedDistance(shape, point.x, point.y);
    case RegionAttachmentKind::inlay:
        shape.kind = ShapePrimitiveKind::diamond;
        return shapeSignedDistance(shape, point.x, point.y);
    case RegionAttachmentKind::glyph:
        return glyphDistance(
            operation.glyph,
            point,
            operation.size,
            operation.lineWidth);
    case RegionAttachmentKind::chip: {
        shape.kind = ShapePrimitiveKind::ellipse;
        const double base = shapeSignedDistance(shape, point.x, point.y);
        const double angle = std::atan2(point.y, point.x) / (2.0 * std::numbers::pi) + 0.5;
        const auto sector = static_cast<std::uint32_t>(
            std::clamp(std::floor(angle * 8.0), 0.0, 7.0));
        const double jagged = (regionRandom(
            angularChannel,
            attachmentDomain,
            sector,
            0) - 0.5) * operation.size * 0.22;
        return base + jagged * (0.55 + variation * 0.45);
    }
    case RegionAttachmentKind::crack:
    case RegionAttachmentKind::damage:
        break;
    }
    return 1.0;
}

double crackDistance(
    const RegionAttachmentOperation& operation,
    const RegionSample& region,
    std::uint32_t index,
    std::uint64_t materialSeed,
    Point point)
{
    const auto start = resolveRegionAnchor(
        region,
        operation.startAnchor,
        index,
        operation.count,
        materialSeed,
        operation.seedOffset,
        operation.inset);
    const auto target = resolveRegionAnchor(
        region,
        operation.endAnchor,
        index + operation.count + 1U,
        operation.count,
        materialSeed,
        operation.seedOffset ^ 0x9e3779b97f4a7c15ULL,
        operation.inset);
    Point a{start.u, start.v};
    Point b{
        a.x + (target.u - a.x) * operation.length,
        a.y + (target.v - a.y) * operation.length,
    };
    Point midpoint{(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
    const Point direction{b.x - a.x, b.y - a.y};
    const double bend = (randomChannel(
        region, materialSeed, operation.seedOffset, index, 19) * 2.0 - 1.0) *
        operation.jitter;
    midpoint.x += -direction.y * bend;
    midpoint.y += direction.x * bend;
    double distance = std::min(
        pointSegmentDistance(point, a, midpoint),
        pointSegmentDistance(point, midpoint, b));
    if (operation.branching > 0.0) {
        const double side = randomChannel(
            region, materialSeed, operation.seedOffset, index, 20) < 0.5 ? -1.0 : 1.0;
        const Point branchEnd{
            midpoint.x - direction.y * operation.branching * side,
            midpoint.y + direction.x * operation.branching * side,
        };
        distance = std::min(distance, pointSegmentDistance(point, midpoint, branchEnd));
    }
    return distance - operation.lineWidth * 0.5;
}

} // namespace

RegionAnchorPoint resolveRegionAnchor(
    const RegionSample& region,
    RegionAnchor anchor,
    std::uint32_t attachmentIndex,
    std::uint32_t attachmentCount,
    std::uint64_t materialSeed,
    std::uint64_t seedOffset,
    double authoredInset)
{
    if (!region.valid) return {};
    const double first = randomChannel(
        region, materialSeed, seedOffset, attachmentIndex, 1);
    const double second = randomChannel(
        region, materialSeed, seedOffset, attachmentIndex, 2);
    const double safeInset = std::clamp(
        authoredInset * (0.8 +
            randomChannel(region, materialSeed, seedOffset, attachmentIndex, 3) * 0.4),
        0.02,
        0.45);
    RegionAnchorPoint result;
    result.key = mixBits(region.key ^ mixBits(seedOffset) ^ mixBits(attachmentIndex));
    switch (anchor) {
    case RegionAnchor::centre: {
        if (attachmentCount <= 1U) return {0.5, 0.5, 0.0, result.key};
        const double turns =
            (static_cast<double>(attachmentIndex) + first * 0.35) /
            static_cast<double>(attachmentCount);
        const double radius = 0.12 + second * 0.08;
        return {
            0.5 + std::cos(turns * 2.0 * std::numbers::pi) * radius,
            0.5 + std::sin(turns * 2.0 * std::numbers::pi) * radius,
            turns,
            result.key,
        };
    }
    case RegionAnchor::edge:
    case RegionAnchor::cavity: {
        const auto side = static_cast<std::uint32_t>(first * 4.0) % 4U;
        const double inset = anchor == RegionAnchor::cavity
            ? safeInset * 0.55
            : safeInset;
        const double along = inset + second * (1.0 - inset * 2.0);
        switch (side) {
        case 0: return {along, inset, 0.0, result.key};
        case 1: return {1.0 - inset, along, 0.25, result.key};
        case 2: return {1.0 - along, 1.0 - inset, 0.5, result.key};
        default: return {inset, 1.0 - along, 0.75, result.key};
        }
    }
    case RegionAnchor::corner: {
        const auto corner = static_cast<std::uint32_t>(first * 4.0) % 4U;
        switch (corner) {
        case 0: return {safeInset, safeInset, 0.125, result.key};
        case 1: return {1.0 - safeInset, safeInset, 0.375, result.key};
        case 2: return {1.0 - safeInset, 1.0 - safeInset, 0.625, result.key};
        default: return {safeInset, 1.0 - safeInset, 0.875, result.key};
        }
    }
    }
    return result;
}

RegionAttachmentSample evaluateRegionAttachment(
    const RegionAttachmentOperation& operation,
    const RegionSample& region,
    std::uint64_t materialSeed)
{
    RegionAttachmentSample selected;
    selected.region = region;
    if (!region.valid) return selected;

    for (std::uint32_t index = 0; index < operation.count; ++index) {
        if (randomChannel(region, materialSeed, operation.seedOffset, index, 0) >
            operation.selection) {
            continue;
        }
        const auto anchor = resolveRegionAnchor(
            region,
            operation.startAnchor,
            index,
            operation.count,
            materialSeed,
            operation.seedOffset,
            operation.inset);
        Point local{region.localU - anchor.u, region.localV - anchor.v};
        const double randomRotation =
            (randomChannel(region, materialSeed, operation.seedOffset, index, 7) * 2.0 - 1.0) *
            operation.jitter;
        local = rotate(
            local,
            anchor.orientationTurns + operation.rotationDegrees / 360.0 + randomRotation);
        const double variation = randomChannel(
            region, materialSeed, operation.seedOffset, index, 8);
        double distance = 1.0;
        if (operation.kind == RegionAttachmentKind::crack ||
            operation.kind == RegionAttachmentKind::damage) {
            distance = crackDistance(
                operation,
                region,
                index,
                materialSeed,
                {region.localU, region.localV});
        }
        if (operation.kind != RegionAttachmentKind::crack) {
            auto primitiveOperation = operation;
            if (primitiveOperation.kind == RegionAttachmentKind::damage) {
                primitiveOperation.kind = RegionAttachmentKind::chip;
            }
            const double primitive = primitiveDistance(
                primitiveOperation,
                local,
                variation,
                static_cast<std::uint32_t>(anchor.key));
            distance = std::min(distance, primitive);
        }
        double coverage = smoothCoverage(-distance, operation.softness);
        if (operation.startAnchor == RegionAnchor::cavity) {
            coverage *= smoothCoverage(
                0.38 - region.boundaryDistance,
                operation.softness * 2.0);
        }
        if (coverage > selected.coverage) {
            selected.coverage = coverage;
            selected.signedDistance = distance;
            selected.localU = std::clamp(
                local.x / operation.size + 0.5,
                0.0,
                1.0);
            selected.localV = std::clamp(
                local.y / (operation.size / operation.aspect) + 0.5,
                0.0,
                1.0);
            selected.variation = variation;
        }
    }
    return selected;
}

} // namespace paperweight
