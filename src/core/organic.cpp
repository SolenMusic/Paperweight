#include <paperweight/organic.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/structural.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace paperweight {
namespace {

constexpr std::uint64_t cellDomain = 0x6f7267616e696363ULL;
constexpr std::uint64_t crackDomain = 0x6272616e63686372ULL;
constexpr std::uint64_t leafDomain = 0x6c656166636c7573ULL;
constexpr std::uint64_t growthDomain = 0x6d6f73736c696368ULL;

std::int64_t positiveModulo(std::int64_t value, std::uint32_t modulus)
{
    const auto signedModulus = static_cast<std::int64_t>(modulus);
    const auto remainder = value % signedModulus;
    return remainder < 0 ? remainder + signedModulus : remainder;
}

double interpolate(double from, double to, double amount)
{
    return from + (to - from) * amount;
}

std::uint8_t interpolateChannel(std::uint8_t from, std::uint8_t to, double amount)
{
    return static_cast<std::uint8_t>(std::round(std::clamp(
        interpolate(static_cast<double>(from), static_cast<double>(to), amount),
        0.0,
        255.0)));
}

Rgba8 interpolateColour(const Rgba8& from, const Rgba8& to, double amount)
{
    return {
        interpolateChannel(from.red, to.red, amount),
        interpolateChannel(from.green, to.green, amount),
        interpolateChannel(from.blue, to.blue, amount),
        interpolateChannel(from.alpha, to.alpha, amount),
    };
}

double torusDelta(double value, double centre)
{
    double delta = wrapUnit(value) - wrapUnit(centre);
    if (delta > 0.5) delta -= 1.0;
    if (delta < -0.5) delta += 1.0;
    return delta;
}

double distanceToSegment(
    double pointX,
    double pointY,
    double startX,
    double startY,
    double endX,
    double endY,
    double* along = nullptr)
{
    const double edgeX = endX - startX;
    const double edgeY = endY - startY;
    const double lengthSquared = edgeX * edgeX + edgeY * edgeY;
    const double amount = lengthSquared == 0.0
        ? 0.0
        : std::clamp(
              ((pointX - startX) * edgeX + (pointY - startY) * edgeY) /
                  lengthSquared,
              0.0,
              1.0);
    if (along != nullptr) *along = amount;
    return std::hypot(
        pointX - interpolate(startX, endX, amount),
        pointY - interpolate(startY, endY, amount));
}

void orientPoint(
    OrganicDirection direction,
    double canonicalU,
    double canonicalV,
    double& u,
    double& v)
{
    if (direction == OrganicDirection::vertical) {
        u = canonicalU;
        v = canonicalV;
    } else {
        u = canonicalV;
        v = canonicalU;
    }
}

double signedLeafDistance(
    const LeafClusterOperation& operation,
    const LeafInstance& leaf,
    double deltaU,
    double deltaV,
    double& localX,
    double& localY,
    double& normalisedY)
{
    const double radians = leaf.rotationDegrees * std::numbers::pi / 180.0;
    const double sine = std::sin(radians);
    const double cosine = std::cos(radians);
    localX = cosine * deltaU + sine * deltaV;
    localY = -sine * deltaU + cosine * deltaV;
    const double halfLength = leaf.length * 0.5;
    const double halfWidth = leaf.width * 0.5;
    normalisedY = localY / halfLength;
    const double absoluteY = std::abs(normalisedY);
    const double boundedY = std::clamp(normalisedY, -1.0, 1.0);
    const double phase = (boundedY + 1.0) * 0.5;

    double envelope = 0.0;
    switch (leaf.profile) {
    case LeafProfile::ovate:
        envelope = std::sqrt(std::max(0.0, 1.0 - boundedY * boundedY));
        break;
    case LeafProfile::lanceolate:
        envelope = std::pow(std::max(0.0, 1.0 - absoluteY), 0.48);
        break;
    case LeafProfile::cordate:
        envelope = std::pow(std::sin(std::numbers::pi * phase), 0.62);
        envelope *= 0.88 + 0.24 * (1.0 - phase);
        break;
    case LeafProfile::lobed:
        envelope = std::sqrt(std::max(0.0, 1.0 - boundedY * boundedY));
        envelope *= 1.0 - operation.lobing * 0.5 +
            operation.lobing * 0.5 *
                (1.0 + std::cos(
                    static_cast<double>(operation.lobeCount) *
                    std::numbers::pi * phase));
        break;
    case LeafProfile::blob:
    case LeafProfile::rosette:
    case LeafProfile::lichen: {
        const double normalisedX = localX / halfWidth;
        const double angle = std::atan2(normalisedY, normalisedX);
        const double radius = std::hypot(normalisedX, normalisedY);
        const double phaseOffset = unitDouble(mixBits(leaf.key ^ 0xb10bULL)) *
            2.0 * std::numbers::pi;
        double boundary = 1.0;
        if (leaf.profile == LeafProfile::blob) {
            boundary = 0.9 + operation.lobing * 0.18 *
                std::sin(static_cast<double>(operation.lobeCount) * angle + phaseOffset);
        } else if (leaf.profile == LeafProfile::rosette) {
            boundary = 0.76 + (0.18 + operation.lobing * 0.22) *
                (0.5 + 0.5 * std::cos(
                    static_cast<double>(operation.lobeCount) * angle + phaseOffset));
        } else {
            boundary = 0.86 + operation.lobing * 0.16 *
                std::sin(static_cast<double>(operation.lobeCount) * angle + phaseOffset) +
                0.08 * std::sin(
                    static_cast<double>(operation.lobeCount + 3U) * angle - phaseOffset * 0.7);
        }
        return (radius - std::max(boundary, 0.28)) *
            std::min(halfWidth, halfLength);
    }
    }
    envelope = std::pow(std::max(envelope, 0.0), operation.taper);
    if (operation.serration > 0.0) {
        const double teeth = std::abs(std::sin(
            static_cast<double>(operation.serrationCount) *
            std::numbers::pi * phase));
        envelope *= 1.0 - operation.serration + operation.serration * teeth;
    }
    const double centreShift = operation.curvature * leaf.width *
        (1.0 - boundedY * boundedY) * (boundedY * 0.65 + 0.35);
    localX -= centreShift;
    double halfWidthAtY = halfWidth * envelope;

    double distanceX = std::abs(localX) - halfWidthAtY;
    if (leaf.profile == LeafProfile::cordate && normalisedY < -0.62) {
        const double notchDepth = std::clamp(
            (-normalisedY - 0.62) / 0.38,
            0.0,
            1.0) * operation.baseNotch * leaf.width;
        if (std::abs(localX) < notchDepth) {
            distanceX = std::max(distanceX, notchDepth - std::abs(localX));
        }
    }
    const double distanceY = (absoluteY - 1.0) * halfLength;
    return std::max(distanceX, distanceY);
}

double leafVeinCoverage(
    const LeafClusterOperation& operation,
    const LeafInstance& leaf,
    double localX,
    double localY,
    double fill)
{
    if (operation.veinPairs == 0 || fill <= 0.0) return 0.0;
    double nearest = std::numeric_limits<double>::infinity();
    const double halfLength = leaf.length * 0.5;
    const double halfWidth = leaf.width * 0.5;
    for (std::uint32_t index = 0; index < operation.veinPairs; ++index) {
        const double amount = (static_cast<double>(index) + 1.0) /
            (static_cast<double>(operation.veinPairs) + 1.0);
        const double y = interpolate(-halfLength * 0.62, halfLength * 0.58, amount);
        const double reach = halfWidth * (0.82 - std::abs(y / halfLength) * 0.32);
        const double tipY = y + halfLength * (0.14 + 0.08 * amount);
        nearest = std::min(nearest, distanceToSegment(localX, localY, 0.0, y, reach, tipY));
        nearest = std::min(nearest, distanceToSegment(localX, localY, 0.0, y, -reach, tipY));
    }
    return fill * smoothCoverage(
        operation.veinWidth * leaf.width - nearest,
        operation.softness);
}

} // namespace

OrganicCellSample evaluateOrganicCells(
    const OrganicCellOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    double canonicalU = u;
    double canonicalV = v;
    if (operation.direction == OrganicDirection::horizontal) {
        std::swap(canonicalU, canonicalV);
    }
    double scaledX = wrapUnit(canonicalU) * static_cast<double>(operation.columns);
    const double scaledY = wrapUnit(canonicalV) * static_cast<double>(operation.rows);
    const auto seed = mixBits(materialSeed ^ mixBits(operation.seedOffset) ^ cellDomain);
    const double phase = unitDouble(mixBits(seed ^ 0x31a6ULL));
    scaledX += std::sin(
        2.0 * std::numbers::pi *
        (wrapUnit(canonicalV) * static_cast<double>(operation.rows) + phase)) *
        operation.irregularity * 0.34;
    const auto baseX = static_cast<std::int64_t>(std::floor(scaledX));
    const auto baseY = static_cast<std::int64_t>(std::floor(scaledY));
    const auto rangeY = static_cast<std::int64_t>(std::ceil(operation.anisotropy)) + 1;

    double nearest = std::numeric_limits<double>::infinity();
    double second = std::numeric_limits<double>::infinity();
    double nearestDeltaX{};
    double nearestDeltaY{};
    std::int64_t nearestX{};
    std::int64_t nearestY{};
    for (std::int64_t offsetY = -rangeY; offsetY <= rangeY; ++offsetY) {
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
            const double deltaX = (scaledX - featureX) * operation.anisotropy;
            const double deltaY = scaledY - featureY;
            const double distance = std::hypot(deltaX, deltaY);
            if (distance < nearest) {
                second = nearest;
                nearest = distance;
                nearestDeltaX = deltaX;
                nearestDeltaY = deltaY;
                nearestX = wrappedX;
                nearestY = wrappedY;
            } else if (distance < second) {
                second = distance;
            }
        }
    }

    const double boundaryDistance = std::max(0.0, second - nearest);
    const double plates = smoothCoverage(
        boundaryDistance - operation.gap,
        operation.softness);
    const double boundaries = 1.0 - plates;
    const auto key = makeRegionKey(cellDomain, nearestX, nearestY);
    const double random = regionRandom(materialSeed, key, operation.seedOffset, 0);
    double value = plates;
    switch (operation.field) {
    case OrganicCellField::plates: value = plates; break;
    case OrganicCellField::boundaries: value = boundaries; break;
    case OrganicCellField::cellRandom: value = random * plates; break;
    }
    double localU = std::clamp(0.5 + nearestDeltaX / (2.0 * operation.anisotropy), 0.0, 1.0);
    double localV = std::clamp(0.5 + nearestDeltaY * 0.5, 0.0, 1.0);
    if (operation.direction == OrganicDirection::horizontal) std::swap(localU, localV);
    return {
        value,
        boundaries,
        random,
        RegionSample{
            key,
            localU,
            localV,
            std::clamp(nearest / std::sqrt(2.0), 0.0, 1.0),
            std::clamp(boundaryDistance, 0.0, 1.0),
            true,
        },
    };
}

OrganicCrackLayout buildOrganicCrackLayout(
    const OrganicCrackOperation& operation,
    std::uint64_t materialSeed)
{
    OrganicCrackLayout layout;
    const auto seed = mixBits(materialSeed ^ mixBits(operation.seedOffset) ^ crackDomain);
    layout.segments.reserve(
        static_cast<std::size_t>(operation.roots) * operation.segments *
        (1U + operation.branchLevels));
    for (std::uint32_t root = 0; root < operation.roots; ++root) {
        const auto rootBits = hashCoordinates(seed, root, 0, 0);
        const double rootU =
            (static_cast<double>(root) + 0.2 + unitDouble(rootBits) * 0.6) /
            static_cast<double>(operation.roots);
        const double rootV = unitDouble(hashCoordinates(seed, root, 0, 1));
        std::vector<double> nodes(operation.segments + 1U, rootU);
        for (std::uint32_t segment = 1; segment < operation.segments; ++segment) {
            nodes[segment] +=
                (unitDouble(hashCoordinates(seed, root, segment, 2)) * 2.0 - 1.0) *
                operation.bend / static_cast<double>(operation.roots);
        }
        nodes.back() = nodes.front();
        for (std::uint32_t segment = 0; segment < operation.segments; ++segment) {
            double startU{};
            double startV{};
            double endU{};
            double endV{};
            orientPoint(
                operation.direction,
                nodes[segment],
                rootV + static_cast<double>(segment) / operation.segments,
                startU,
                startV);
            orientPoint(
                operation.direction,
                nodes[segment + 1U],
                rootV + static_cast<double>(segment + 1U) / operation.segments,
                endU,
                endV);
            const auto key = hashCoordinates(seed, root, segment, 3);
            layout.segments.push_back({
                startU, startV, endU, endV, operation.width, 0, key});

            double branchU = nodes[segment];
            double branchV = rootV + static_cast<double>(segment) / operation.segments;
            for (std::uint32_t level = 1; level <= operation.branchLevels; ++level) {
                const auto branchBits = hashCoordinates(
                    seed,
                    static_cast<std::int64_t>(root * operation.segments + segment),
                    level,
                    4);
                const double probability = operation.branchProbability *
                    std::pow(0.78, static_cast<double>(level - 1U));
                if (unitDouble(branchBits) >= probability) break;
                const double side = (branchBits & 1U) == 0 ? -1.0 : 1.0;
                const double angle = side * interpolate(
                    0.72,
                    1.18,
                    unitDouble(mixBits(branchBits ^ 0x5a51ULL)));
                const double length =
                    (0.19 / std::sqrt(static_cast<double>(operation.roots))) *
                    std::pow(0.72, static_cast<double>(level - 1U));
                const double nextU = branchU + std::sin(angle) * length;
                const double nextV = branchV + std::cos(angle) * length;
                double orientedStartU{};
                double orientedStartV{};
                double orientedEndU{};
                double orientedEndV{};
                orientPoint(operation.direction, branchU, branchV, orientedStartU, orientedStartV);
                orientPoint(operation.direction, nextU, nextV, orientedEndU, orientedEndV);
                layout.segments.push_back({
                    orientedStartU,
                    orientedStartV,
                    orientedEndU,
                    orientedEndV,
                    operation.width * std::pow(operation.taper, static_cast<double>(level)),
                    level,
                    mixBits(branchBits ^ static_cast<std::uint64_t>(level)),
                });
                branchU = nextU;
                branchV = nextV;
            }
        }
    }
    return layout;
}

OrganicCrackSample evaluateOrganicCracks(
    const OrganicCrackOperation& operation,
    const OrganicCrackLayout& layout,
    double u,
    double v)
{
    const double wrappedU = wrapUnit(u);
    const double wrappedV = wrapUnit(v);
    const OrganicCrackSegment* selected = nullptr;
    double selectedDistance = std::numeric_limits<double>::infinity();
    double selectedAlong{};
    for (const auto& segment : layout.segments) {
        if ((operation.field == OrganicCrackField::trunks && segment.hierarchy != 0) ||
            (operation.field == OrganicCrackField::branches && segment.hierarchy == 0)) {
            continue;
        }
        for (int copyY = -1; copyY <= 1; ++copyY) {
            for (int copyX = -1; copyX <= 1; ++copyX) {
                double along{};
                const double distance = distanceToSegment(
                    wrappedU + copyX,
                    wrappedV + copyY,
                    segment.startU,
                    segment.startV,
                    segment.endU,
                    segment.endV,
                    &along);
                if (distance < selectedDistance ||
                    (distance == selectedDistance && selected != nullptr &&
                     segment.key < selected->key)) {
                    selected = &segment;
                    selectedDistance = distance;
                    selectedAlong = along;
                }
            }
        }
    }
    if (selected == nullptr) return {};
    const double coverage = smoothCoverage(
        selected->width * 0.5 - selectedDistance,
        operation.softness);
    const double hierarchy = operation.branchLevels == 0
        ? 1.0
        : 1.0 - static_cast<double>(selected->hierarchy) /
              static_cast<double>(operation.branchLevels + 1U);
    double value = coverage;
    if (operation.field == OrganicCrackField::hierarchy) value *= hierarchy;
    if (operation.field == OrganicCrackField::distance) value = 1.0 - coverage;
    return {
        value,
        selectedDistance,
        hierarchy,
        RegionSample{
            selected->key,
            selectedAlong,
            std::clamp(
                0.5 + selectedDistance / std::max(selected->width, 1.0e-9),
                0.0,
                1.0),
            selectedAlong,
            std::clamp(1.0 - selectedDistance / std::max(selected->width, 1.0e-9), 0.0, 1.0),
            coverage > 0.0,
        },
    };
}

LeafClusterLayout buildLeafClusterLayout(
    const LeafClusterOperation& operation,
    std::uint64_t materialSeed)
{
    LeafClusterLayout layout;
    layout.columns = operation.columns;
    layout.rows = operation.rows;
    layout.cellInstanceIndices.resize(
        static_cast<std::size_t>(operation.columns) * operation.rows);
    const auto seed = mixBits(materialSeed ^ mixBits(operation.seedOffset) ^ leafDomain);
    for (std::uint32_t row = 0; row < operation.rows; ++row) {
        for (std::uint32_t column = 0; column < operation.columns; ++column) {
            const auto cellIndex = row * operation.columns + column;
            const auto clusterBits = hashCoordinates(seed, column, row, 0);
            if (unitDouble(clusterBits) >= operation.density) continue;
            const double centreU =
                (static_cast<double>(column) + 0.5 +
                 (unitDouble(hashCoordinates(seed, column, row, 1)) - 0.5) * 0.7) /
                static_cast<double>(operation.columns);
            const double centreV =
                (static_cast<double>(row) + 0.5 +
                 (unitDouble(hashCoordinates(seed, column, row, 2)) - 0.5) * 0.7) /
                static_cast<double>(operation.rows);
            const auto clusterKey = makeRegionKey(leafDomain, column, row);
            const double clusterRandom = unitDouble(mixBits(clusterBits ^ 0xc1757eULL));
            for (std::uint32_t leafIndex = 0;
                 leafIndex < operation.leavesPerCluster;
                 ++leafIndex) {
                const auto key = hashCoordinates(
                    seed,
                    static_cast<std::int64_t>(cellIndex),
                    leafIndex,
                    3);
                const double randomAngle = unitDouble(mixBits(key ^ 0xa11eULL));
                const double randomRadius = unitDouble(mixBits(key ^ 0xb22fULL));
                double angleDegrees = operation.directionDegrees;
                double radius = operation.clusterSpread * std::sqrt(randomRadius);
                switch (operation.pattern) {
                case LeafClusterPattern::radial:
                    angleDegrees += 360.0 * static_cast<double>(leafIndex) /
                        static_cast<double>(operation.leavesPerCluster) +
                        (randomAngle - 0.5) * operation.rotationVariation;
                    radius = operation.clusterSpread *
                        (0.28 + 0.72 * static_cast<double>(leafIndex + 1U) /
                            static_cast<double>(operation.leavesPerCluster));
                    break;
                case LeafClusterPattern::fan:
                    angleDegrees += interpolate(
                        -72.0,
                        72.0,
                        operation.leavesPerCluster == 1
                            ? 0.5
                            : static_cast<double>(leafIndex) /
                                  static_cast<double>(operation.leavesPerCluster - 1U));
                    radius *= 0.72;
                    break;
                case LeafClusterPattern::vine:
                    angleDegrees += (leafIndex % 2U == 0 ? -68.0 : 68.0) +
                        (randomAngle - 0.5) * operation.rotationVariation;
                    radius = operation.clusterSpread *
                        (static_cast<double>(leafIndex) /
                         std::max(1.0, static_cast<double>(operation.leavesPerCluster - 1U))) -
                        operation.clusterSpread * 0.5;
                    break;
                case LeafClusterPattern::canopy:
                case LeafClusterPattern::groundScatter:
                    angleDegrees += randomAngle * 360.0;
                    break;
                }
                const double radians = angleDegrees * std::numbers::pi / 180.0;
                double leafU = centreU + std::sin(radians) * radius;
                double leafV = centreV + std::cos(radians) * radius;
                if (operation.pattern == LeafClusterPattern::vine) {
                    leafU = centreU + std::sin(
                        operation.directionDegrees * std::numbers::pi / 180.0) * radius;
                    leafV = centreV + std::cos(
                        operation.directionDegrees * std::numbers::pi / 180.0) * radius;
                }
                double scale = 1.0 + operation.scaleVariation *
                    (unitDouble(mixBits(key ^ 0xc33dULL)) * 2.0 - 1.0);
                const double aspectVariation = 1.0 + operation.scaleVariation * 0.35 *
                    (unitDouble(mixBits(key ^ 0xd44cULL)) * 2.0 - 1.0);
                const double instanceColourRandom = unitDouble(mixBits(key ^ 0xe55bULL));
                double colourAmount = instanceColourRandom;
                if (operation.clusterColourVariation != 0.0 ||
                    operation.instanceColourVariation != 1.0) {
                    colourAmount = std::clamp(
                        0.5 + (clusterRandom - 0.5) * operation.clusterColourVariation +
                            (instanceColourRandom - 0.5) * operation.instanceColourVariation,
                        0.0,
                        1.0);
                }
                LeafProfile profile = operation.profile;
                const Rgba8* lowColour = &operation.lowColour;
                const Rgba8* highColour = &operation.highColour;
                std::uint32_t population = 0;
                const double populationRandom = unitDouble(mixBits(key ^ 0x90a17ULL));
                if (populationRandom < operation.secondaryWeight) {
                    profile = operation.secondaryProfile;
                    lowColour = &operation.secondaryLowColour;
                    highColour = &operation.secondaryHighColour;
                    scale *= operation.secondaryScale;
                    population = 1;
                } else if (populationRandom <
                           operation.secondaryWeight + operation.tertiaryWeight) {
                    profile = operation.tertiaryProfile;
                    lowColour = &operation.tertiaryLowColour;
                    highColour = &operation.tertiaryHighColour;
                    scale *= operation.tertiaryScale;
                    population = 2;
                }
                const double heightAmount = unitDouble(mixBits(key ^ 0xf66aULL));
                const double roughnessAmount = unitDouble(mixBits(key ^ 0x1779ULL));
                const auto instanceIndex = static_cast<std::uint32_t>(layout.instances.size());
                const double length = operation.leafLength * scale;
                const double width = operation.leafWidth * scale * aspectVariation;
                layout.instances.push_back({
                    wrapUnit(leafU),
                    wrapUnit(leafV),
                    length,
                    width,
                    angleDegrees + (unitDouble(mixBits(key ^ 0x2888ULL)) - 0.5) *
                        operation.rotationVariation,
                    interpolateColour(*lowColour, *highColour, colourAmount),
                    interpolate(operation.minimumHeight, operation.maximumHeight, heightAmount),
                    interpolate(
                        operation.minimumRoughness,
                        operation.maximumRoughness,
                        roughnessAmount),
                    colourAmount,
                    key,
                    clusterKey,
                    mixBits(key ^ 0x3997ULL),
                    cellIndex,
                    profile,
                    population,
                    clusterRandom,
                });
                const auto leafColumn = std::min(
                    static_cast<std::uint32_t>(wrapUnit(leafU) * operation.columns),
                    operation.columns - 1U);
                const auto leafRow = std::min(
                    static_cast<std::uint32_t>(wrapUnit(leafV) * operation.rows),
                    operation.rows - 1U);
                layout.cellInstanceIndices[
                    static_cast<std::size_t>(leafRow) * operation.columns + leafColumn]
                    .push_back(instanceIndex);
                layout.maximumRadius = std::max(
                    layout.maximumRadius,
                    std::hypot(length, width) * 0.5);
            }
        }
    }
    return layout;
}

LeafSample evaluateLeafCluster(
    const LeafClusterOperation& operation,
    const LeafClusterLayout& layout,
    double u,
    double v)
{
    if (layout.instances.empty()) return {};
    const double wrappedU = wrapUnit(u);
    const double wrappedV = wrapUnit(v);
    const auto baseColumn = static_cast<std::int64_t>(std::floor(wrappedU * layout.columns));
    const auto baseRow = static_cast<std::int64_t>(std::floor(wrappedV * layout.rows));
    const auto rangeX = static_cast<std::int64_t>(
        std::ceil(layout.maximumRadius * layout.columns));
    const auto rangeY = static_cast<std::int64_t>(
        std::ceil(layout.maximumRadius * layout.rows));
    const bool allColumns = rangeX * 2 + 1 >= static_cast<std::int64_t>(layout.columns);
    const bool allRows = rangeY * 2 + 1 >= static_cast<std::int64_t>(layout.rows);

    const LeafInstance* selected = nullptr;
    double selectedDistance{};
    double selectedLocalX{};
    double selectedLocalY{};
    double selectedNormalisedY{};
    double selectedCoverage{};
    const auto visitCell = [&](std::uint32_t column, std::uint32_t row) {
        const auto cellIndex = static_cast<std::size_t>(row) * layout.columns + column;
        for (const auto instanceIndex : layout.cellInstanceIndices[cellIndex]) {
            const auto& leaf = layout.instances[instanceIndex];
            double localX{};
            double localY{};
            double normalisedY{};
            const double distance = signedLeafDistance(
                operation,
                leaf,
                torusDelta(wrappedU, leaf.centreU),
                torusDelta(wrappedV, leaf.centreV),
                localX,
                localY,
                normalisedY);
            const double coverage = smoothCoverage(-distance, operation.softness);
            if (coverage <= 0.0) continue;
            const bool occludes = selected == nullptr || leaf.height > selected->height ||
                (leaf.height == selected->height && leaf.occlusionOrder > selected->occlusionOrder);
            if (!occludes) continue;
            selected = &leaf;
            selectedDistance = distance;
            selectedLocalX = localX;
            selectedLocalY = localY;
            selectedNormalisedY = normalisedY;
            selectedCoverage = coverage;
        }
    };
    const auto visitColumns = [&](std::uint32_t row) {
        if (allColumns) {
            for (std::uint32_t column = 0; column < layout.columns; ++column) visitCell(column, row);
        } else {
            for (std::int64_t offset = -rangeX; offset <= rangeX; ++offset) {
                visitCell(
                    static_cast<std::uint32_t>(positiveModulo(baseColumn + offset, layout.columns)),
                    row);
            }
        }
    };
    if (allRows) {
        for (std::uint32_t row = 0; row < layout.rows; ++row) visitColumns(row);
    } else {
        for (std::int64_t offset = -rangeY; offset <= rangeY; ++offset) {
            visitColumns(static_cast<std::uint32_t>(positiveModulo(baseRow + offset, layout.rows)));
        }
    }
    if (selected == nullptr) return {};

    const double midrib = selectedCoverage * smoothCoverage(
        operation.midribWidth * selected->width - std::abs(selectedLocalX),
        operation.softness);
    const double veins = leafVeinCoverage(
        operation,
        *selected,
        selectedLocalX,
        selectedLocalY,
        selectedCoverage);
    const double edge = selectedCoverage * smoothCoverage(
        operation.edgeWidth * std::min(selected->width, selected->length) -
            std::abs(selectedDistance),
        operation.softness);
    const double minimumDimension = std::min(selected->width, selected->length);
    const double depth = std::max(-selectedDistance, 0.0);
    const double highlightStart = operation.innerHighlightInset * minimumDimension;
    const double highlightEnd =
        (operation.innerHighlightInset + operation.innerHighlightWidth) * minimumDimension;
    const double innerHighlight = selectedCoverage *
        smoothCoverage(depth - highlightStart, operation.softness) *
        (1.0 - smoothCoverage(depth - highlightEnd, operation.softness));
    const double localU = std::clamp(selectedLocalX / selected->width + 0.5, 0.0, 1.0);
    const double localV = std::clamp(selectedNormalisedY * 0.5 + 0.5, 0.0, 1.0);
    return {
        selectedCoverage,
        edge,
        midrib,
        veins,
        localU,
        localV,
        selected->colour,
        selected->height,
        selected->roughness,
        selected->random,
        RegionSample{
            selected->key,
            localU,
            localV,
            std::clamp(std::hypot(
                selectedLocalX / (selected->width * 0.5),
                selectedLocalY / (selected->length * 0.5)) / std::sqrt(2.0), 0.0, 1.0),
            std::clamp(
                -selectedDistance / std::max(
                    LayerLimits::minimumShapeDimension,
                    std::min(selected->width, selected->length)),
                0.0,
                1.0),
            true,
            selected->clusterKey,
            true,
            selected->rotationDegrees / 360.0,
        },
        edge,
        innerHighlight,
        selected->clusterRandom,
        static_cast<double>(selected->population) * 0.5,
    };
}

LeafClusterOperation leafSpeciesPreset(LeafSpecies species)
{
    LeafClusterOperation operation;
    switch (species) {
    case LeafSpecies::ivy:
        operation.profile = LeafProfile::cordate;
        operation.pattern = LeafClusterPattern::canopy;
        operation.baseNotch = 0.2;
        operation.curvature = 0.1;
        operation.serration = 0.06;
        operation.lowColour = {32, 67, 29, 255};
        operation.highColour = {93, 132, 59, 255};
        break;
    case LeafSpecies::laurel:
        operation.profile = LeafProfile::lanceolate;
        operation.pattern = LeafClusterPattern::fan;
        operation.leafLength = 0.15;
        operation.leafWidth = 0.052;
        operation.taper = 0.55;
        operation.serration = 0.03;
        operation.lowColour = {38, 74, 37, 255};
        operation.highColour = {108, 145, 76, 255};
        break;
    case LeafSpecies::oak:
        operation.profile = LeafProfile::lobed;
        operation.pattern = LeafClusterPattern::radial;
        operation.lobing = 0.48;
        operation.lobeCount = 5;
        operation.leafWidth = 0.08;
        operation.lowColour = {49, 77, 31, 255};
        operation.highColour = {132, 139, 63, 255};
        break;
    case LeafSpecies::ash:
        operation.profile = LeafProfile::lanceolate;
        operation.pattern = LeafClusterPattern::vine;
        operation.leavesPerCluster = 9;
        operation.clusterSpread = 0.16;
        operation.leafLength = 0.085;
        operation.leafWidth = 0.038;
        operation.rotationVariation = 24.0;
        operation.lowColour = {40, 78, 35, 255};
        operation.highColour = {112, 151, 65, 255};
        break;
    }
    return operation;
}

OrganicAccumulationSample evaluateOrganicAccumulation(
    const OrganicAccumulationOperation& operation,
    double inputScalar,
    const RegionSample& region,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const auto seed = mixBits(materialSeed ^ mixBits(operation.seedOffset) ^ growthDomain);
    const double broad = periodicValueNoise2D(
        u * operation.scale,
        v * operation.scale,
        operation.scale,
        operation.scale,
        seed);
    const auto detailScale = std::min<std::uint32_t>(operation.scale * 3U, 192U);
    const double detail = periodicValueNoise2D(
        u * detailScale,
        v * detailScale,
        detailScale,
        detailScale,
        mixBits(seed ^ 0x51c3ULL));
    double source = 1.0;
    switch (operation.source) {
    case OrganicAccumulationSource::cavity:
    case OrganicAccumulationSource::lowHeight:
        source = 1.0 - std::clamp(inputScalar, 0.0, 1.0);
        break;
    case OrganicAccumulationSource::boundary:
        source = region.valid
            ? 1.0 - std::clamp(region.boundaryDistance, 0.0, 1.0)
            : 0.0;
        break;
    case OrganicAccumulationSource::authoredMask:
        source = std::clamp(inputScalar, 0.0, 1.0);
        break;
    }
    const double threshold = 1.0 - operation.coverage;
    double colonySource = broad;
    if (operation.profile == OrganicAccumulationProfile::colonies) {
        colonySource = broad * 0.72 + detail * 0.28;
    } else if (operation.profile == OrganicAccumulationProfile::speckles) {
        colonySource = std::max(broad * 0.75, detail * detail);
    }
    const double fill = smoothCoverage(colonySource - threshold, operation.softness);
    const double outline = std::clamp(
        smoothCoverage(
            colonySource - (threshold - operation.outlineWidth),
            operation.softness) - fill,
        0.0,
        1.0);
    const double highlightStart = threshold + operation.innerHighlightInset;
    const double highlightEnd = highlightStart + operation.innerHighlightWidth;
    const double innerHighlight = std::clamp(
        smoothCoverage(colonySource - highlightStart, operation.softness) -
            smoothCoverage(colonySource - highlightEnd, operation.softness),
        0.0,
        1.0);
    double colony = fill;
    colony *= interpolate(1.0, detail, operation.breakup);
    const double amount = std::clamp(
        colony * interpolate(1.0, source, operation.moistureBias),
        0.0,
        1.0);
    const double stableVariation = region.valid
        ? regionRandom(materialSeed, region.key, operation.seedOffset, 17)
        : detail;
    const double sourceInfluence = interpolate(1.0, source, operation.moistureBias);
    return {
        amount,
        stableVariation,
        std::clamp(fill * sourceInfluence, 0.0, 1.0),
        std::clamp(outline * sourceInfluence, 0.0, 1.0),
        std::clamp(innerHighlight * sourceInfluence, 0.0, 1.0),
        detail,
    };
}

} // namespace paperweight
