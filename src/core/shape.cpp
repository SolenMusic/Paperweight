#include <paperweight/shape.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/structural.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace paperweight {
namespace {

struct Point {
    double x;
    double y;
};

std::int64_t positiveModulo(std::int64_t value, std::uint32_t modulus)
{
    const auto signedModulus = static_cast<std::int64_t>(modulus);
    const auto remainder = value % signedModulus;
    return remainder < 0 ? remainder + signedModulus : remainder;
}

double length(Point point)
{
    return std::sqrt(point.x * point.x + point.y * point.y);
}

Point rotateIntoShape(Point point, double degrees)
{
    const double radians = degrees * std::numbers::pi / 180.0;
    const double sine = std::sin(radians);
    const double cosine = std::cos(radians);
    return {
        cosine * point.x + sine * point.y,
        -sine * point.x + cosine * point.y,
    };
}

double roundedRectangleDistance(
    Point point,
    double width,
    double height,
    double radius)
{
    const double halfWidth = width * 0.5;
    const double halfHeight = height * 0.5;
    const double rounded = std::min(radius, std::min(halfWidth, halfHeight));
    const Point q{
        std::abs(point.x) - (halfWidth - rounded),
        std::abs(point.y) - (halfHeight - rounded),
    };
    const Point outside{std::max(q.x, 0.0), std::max(q.y, 0.0)};
    return length(outside) + std::min(std::max(q.x, q.y), 0.0) - rounded;
}

double ellipseDistance(Point point, double width, double height)
{
    const double radiusX = width * 0.5;
    const double radiusY = height * 0.5;
    const double normalised = std::sqrt(
        (point.x * point.x) / (radiusX * radiusX) +
        (point.y * point.y) / (radiusY * radiusY));
    return (normalised - 1.0) * std::min(radiusX, radiusY);
}

double capsuleDistance(Point point, double width, double height)
{
    if (width >= height) {
        const double radius = height * 0.5;
        const double halfSegment = std::max(0.0, width * 0.5 - radius);
        point.x -= std::clamp(point.x, -halfSegment, halfSegment);
        return length(point) - radius;
    }
    const double radius = width * 0.5;
    const double halfSegment = std::max(0.0, height * 0.5 - radius);
    point.y -= std::clamp(point.y, -halfSegment, halfSegment);
    return length(point) - radius;
}

double diamondDistance(Point point, double width, double height)
{
    const double radiusX = width * 0.5;
    const double radiusY = height * 0.5;
    const double field = std::abs(point.x) / radiusX + std::abs(point.y) / radiusY - 1.0;
    return field / std::sqrt(
        1.0 / (radiusX * radiusX) + 1.0 / (radiusY * radiusY));
}

double distanceToSegment(Point point, Point start, Point end)
{
    const Point edge{end.x - start.x, end.y - start.y};
    const Point offset{point.x - start.x, point.y - start.y};
    const double lengthSquared = edge.x * edge.x + edge.y * edge.y;
    const double amount = std::clamp(
        (offset.x * edge.x + offset.y * edge.y) / lengthSquared,
        0.0,
        1.0);
    return length({
        offset.x - edge.x * amount,
        offset.y - edge.y * amount,
    });
}

double polygonDistance(
    Point point,
    double width,
    double height,
    const std::vector<ShapePoint>& vertices)
{
    bool inside = false;
    double nearest = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0, previous = vertices.size() - 1;
         index < vertices.size();
         previous = index++) {
        const Point a{
            vertices[previous].x * width,
            vertices[previous].y * height,
        };
        const Point b{
            vertices[index].x * width,
            vertices[index].y * height,
        };
        nearest = std::min(nearest, distanceToSegment(point, a, b));
        const bool crosses = (a.y > point.y) != (b.y > point.y);
        if (crosses) {
            const double crossingX =
                (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x;
            if (point.x < crossingX) {
                inside = !inside;
            }
        }
    }
    return inside ? -nearest : nearest;
}

double primitiveDistance(
    const ShapePrimitiveOperation& operation,
    Point point,
    double widthScale,
    double heightScale,
    double rotationOffsetDegrees)
{
    point = rotateIntoShape(
        point,
        operation.rotationDegrees + rotationOffsetDegrees);
    const double width = operation.width * widthScale;
    const double height = operation.height * heightScale;
    switch (operation.kind) {
    case ShapePrimitiveKind::roundedRectangle:
        return roundedRectangleDistance(
            point,
            width,
            height,
            operation.cornerRadius * std::min(widthScale, heightScale));
    case ShapePrimitiveKind::ellipse:
        return ellipseDistance(point, width, height);
    case ShapePrimitiveKind::capsule:
        return capsuleDistance(point, width, height);
    case ShapePrimitiveKind::diamond:
        return diamondDistance(point, width, height);
    case ShapePrimitiveKind::convexPolygon:
        return polygonDistance(
            point,
            width,
            height,
            operation.vertices);
    }
    return std::numeric_limits<double>::infinity();
}

double fieldCoverage(
    const ShapePrimitiveOperation& operation,
    double distance,
    double distanceScale)
{
    const double softness = operation.softness * distanceScale;
    const double inset = operation.inset * distanceScale;
    const double borderWidth = operation.borderWidth * distanceScale;
    switch (operation.field) {
    case ShapeFieldKind::fill:
        return smoothCoverage(-distance, softness);
    case ShapeFieldKind::inset:
        return smoothCoverage(-distance - inset, softness);
    case ShapeFieldKind::outline:
        return smoothCoverage(
            borderWidth * 0.5 - std::abs(distance),
            softness);
    case ShapeFieldKind::border:
        return std::min(
            smoothCoverage(-distance, softness),
            smoothCoverage(distance + borderWidth, softness));
    }
    return 0.0;
}

double lineCoverage(double coordinate, double width, double softness)
{
    const double local = wrapUnit(coordinate) - 0.5;
    return smoothCoverage(width * 0.5 - std::abs(local), softness);
}

} // namespace

double shapeSignedDistance(
    const ShapePrimitiveOperation& operation,
    double localX,
    double localY,
    double widthScale,
    double heightScale,
    double rotationOffsetDegrees)
{
    return primitiveDistance(
        operation,
        Point{localX, localY},
        widthScale,
        heightScale,
        rotationOffsetDegrees);
}

double shapeFieldCoverage(
    const ShapePrimitiveOperation& operation,
    double signedDistance,
    double distanceScale)
{
    return fieldCoverage(operation, signedDistance, distanceScale);
}

ShapeSample evaluateShapePrimitive(
    const ShapePrimitiveOperation& operation,
    double u,
    double v)
{
    const double scaledX = wrapUnit(u) * static_cast<double>(operation.columns);
    const double scaledY = wrapUnit(v) * static_cast<double>(operation.rows);
    const auto baseY = static_cast<std::int64_t>(std::floor(scaledY));
    constexpr std::uint64_t baseDomain = 0x7368617065666965ULL;
    const auto regionDomain = mixBits(baseDomain ^ mixBits(operation.seedOffset));

    double nearest = std::numeric_limits<double>::infinity();
    Point nearestPoint{};
    std::int64_t nearestColumn{};
    std::int64_t nearestRow{};
    for (std::int64_t rowOffset = -2; rowOffset <= 2; ++rowOffset) {
        const auto row = baseY + rowOffset;
        const auto wrappedRow = positiveModulo(row, operation.rows);
        const double rowShift = (wrappedRow % 2) != 0 ? operation.stagger : 0.0;
        const auto baseX = static_cast<std::int64_t>(
            std::floor(scaledX - rowShift));
        for (std::int64_t columnOffset = -2; columnOffset <= 2; ++columnOffset) {
            const auto column = baseX + columnOffset;
            const Point point{
                scaledX - (static_cast<double>(column) + 0.5 +
                    operation.offsetX + rowShift),
                scaledY - (static_cast<double>(row) + 0.5 + operation.offsetY),
            };
            const double distance = shapeSignedDistance(
                operation,
                point.x,
                point.y);
            if (distance < nearest) {
                nearest = distance;
                nearestPoint = rotateIntoShape(point, operation.rotationDegrees);
                nearestColumn = positiveModulo(column, operation.columns);
                nearestRow = wrappedRow;
            }
        }
    }

    const double halfWidth = operation.width * 0.5;
    const double halfHeight = operation.height * 0.5;
    const double centreDistance = std::clamp(
        std::sqrt(
            (nearestPoint.x * nearestPoint.x) / (halfWidth * halfWidth) +
            (nearestPoint.y * nearestPoint.y) / (halfHeight * halfHeight)) /
            std::sqrt(2.0),
        0.0,
        1.0);
    const double insideScale = std::max(
        LayerLimits::minimumShapeDimension,
        std::min(halfWidth, halfHeight));
    return {
        shapeFieldCoverage(operation, nearest),
        nearest,
        RegionSample{
            makeRegionKey(regionDomain, nearestColumn, nearestRow),
            std::clamp(nearestPoint.x / operation.width + 0.5, 0.0, 1.0),
            std::clamp(nearestPoint.y / operation.height + 0.5, 0.0, 1.0),
            centreDistance,
            std::clamp(-nearest / insideScale, 0.0, 1.0),
            true,
        },
    };
}

ShapeSample evaluateLattice(
    const LatticeOperation& operation,
    double u,
    double v)
{
    const double wrappedU = wrapUnit(u);
    const double wrappedV = wrapUnit(v);
    const double first =
        static_cast<double>(operation.windingX) * wrappedU +
        static_cast<double>(operation.windingY) * wrappedV + operation.phase;
    double value = lineCoverage(first, operation.width, operation.softness);
    double second = first;
    if (operation.kind == LatticeKind::diamonds) {
        second =
            static_cast<double>(operation.windingX) * wrappedU -
            static_cast<double>(operation.windingY) * wrappedV + operation.phase;
        value = std::max(
            value,
            lineCoverage(second, operation.width, operation.softness));
    }
    constexpr std::uint64_t regionDomain = 0x6c61747469636573ULL;
    const auto firstIndex = static_cast<std::int64_t>(std::floor(first));
    const auto secondIndex = static_cast<std::int64_t>(std::floor(second));
    return {
        value,
        -value,
        RegionSample{
            makeRegionKey(regionDomain, firstIndex, secondIndex),
            wrapUnit(first),
            wrapUnit(second),
            std::clamp(1.0 - value, 0.0, 1.0),
            value,
            true,
        },
    };
}

double combineShapeMasks(double input, double shape, ShapeBooleanMode mode)
{
    input = std::clamp(input, 0.0, 1.0);
    shape = std::clamp(shape, 0.0, 1.0);
    switch (mode) {
    case ShapeBooleanMode::unionMask:
        return std::max(input, shape);
    case ShapeBooleanMode::intersection:
        return std::min(input, shape);
    case ShapeBooleanMode::subtraction:
        return std::min(input, 1.0 - shape);
    }
    return input;
}

} // namespace paperweight
