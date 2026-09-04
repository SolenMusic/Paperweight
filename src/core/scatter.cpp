#include <paperweight/scatter.hpp>

#include <paperweight/hash.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/shape.hpp>
#include <paperweight/structural.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace paperweight {
namespace {

constexpr std::uint64_t scatterDomain = 0x7363617474657273ULL;

double interpolate(double low, double high, double amount)
{
    return low + (high - low) * amount;
}

std::uint8_t interpolateChannel(std::uint8_t low, std::uint8_t high, double amount)
{
    return static_cast<std::uint8_t>(std::llround(interpolate(
        static_cast<double>(low),
        static_cast<double>(high),
        amount)));
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

double torusDelta(double from, double to)
{
    double delta = from - to;
    delta -= std::round(delta);
    return delta;
}

double torusDistance(double firstU, double firstV, double secondU, double secondV)
{
    return std::hypot(
        torusDelta(firstU, secondU),
        torusDelta(firstV, secondV));
}

double maskValue(
    const ScatterMask& mask,
    double u,
    double v,
    std::uint64_t seed)
{
    if (!mask.enabled) {
        return 1.0;
    }
    const double frequency = static_cast<double>(mask.frequency);
    const double noise = periodicValueNoise2D(
        u * frequency,
        v * frequency,
        mask.frequency,
        mask.frequency,
        mixBits(seed ^ mask.seedOffset));
    double value = std::clamp(
        (noise - mask.inputLow) / (mask.inputHigh - mask.inputLow),
        0.0,
        1.0);
    if (mask.inverted) {
        value = 1.0 - value;
    }
    return value;
}

std::size_t selectPopulation(
    const std::vector<ScatterPopulation>& populations,
    double selector)
{
    double total = 0.0;
    for (const auto& population : populations) {
        total += population.weight;
    }
    const double target = selector * total;
    double cumulative = 0.0;
    for (std::size_t index = 0; index < populations.size(); ++index) {
        cumulative += populations[index].weight;
        if (target < cumulative || index + 1 == populations.size()) {
            return index;
        }
    }
    return 0;
}

double instanceRadius(const ScatterOperation& operation, double scale, double aspect)
{
    const double rootAspect = std::sqrt(aspect);
    const double width = operation.stamp.width * scale * rootAspect;
    const double height = operation.stamp.height * scale / rootAspect;
    const double softness = operation.stamp.softness * scale /
        std::max(rootAspect, 1.0 / rootAspect);
    return std::hypot(width, height) * 0.5 + softness;
}

double requiredSeparation(
    const ScatterOperation& operation,
    double firstRadius,
    double secondRadius)
{
    double footprintDistance = 0.0;
    switch (operation.overlapMode) {
    case ScatterOverlapMode::forbidden:
        footprintDistance = firstRadius + secondRadius;
        break;
    case ScatterOverlapMode::controlled:
        footprintDistance =
            (firstRadius + secondRadius) * (1.0 - operation.maximumOverlap);
        break;
    case ScatterOverlapMode::unrestricted:
        break;
    }
    return std::max(operation.minimumDistance, footprintDistance);
}

struct Candidate {
    ScatterInstance instance;
    double radius{};
};

bool occludes(const ScatterInstance& candidate, const ScatterInstance& selected)
{
    return candidate.occlusionPriority > selected.occlusionPriority ||
        (candidate.occlusionPriority == selected.occlusionPriority &&
         candidate.candidateIndex > selected.candidateIndex);
}

} // namespace

ScatterLayout buildScatterLayout(
    const ScatterOperation& operation,
    std::uint64_t materialSeed)
{
    ScatterLayout layout;
    layout.columns = operation.columns;
    layout.rows = operation.rows;
    const auto candidateCount = static_cast<std::size_t>(operation.columns) *
        operation.rows;
    layout.cellInstanceIndices.assign(candidateCount, -1);

    const auto seed = mixBits(materialSeed ^ operation.seedOffset ^ scatterDomain);
    const auto keyDomain = mixBits(seed ^ 0x696e7374616e6365ULL);
    std::vector<Candidate> candidates;
    candidates.reserve(candidateCount);
    for (std::uint32_t row = 0; row < operation.rows; ++row) {
        for (std::uint32_t column = 0; column < operation.columns; ++column) {
            const auto x = static_cast<std::int64_t>(column);
            const auto y = static_cast<std::int64_t>(row);
            const double jitterX = unitDouble(hashCoordinates(seed, x, y, 1));
            const double jitterY = unitDouble(hashCoordinates(seed, x, y, 2));
            const double centreU = wrapUnit(
                (static_cast<double>(column) + 0.5 +
                 (jitterX - 0.5) * operation.jitter) /
                static_cast<double>(operation.columns));
            const double centreV = wrapUnit(
                (static_cast<double>(row) + 0.5 +
                 (jitterY - 0.5) * operation.jitter) /
                static_cast<double>(operation.rows));

            double probability = operation.density;
            if (operation.densityMask.enabled) {
                probability *= maskValue(
                    operation.densityMask,
                    centreU,
                    centreV,
                    seed ^ 0x64656e736974796dULL);
            }
            if (operation.exclusionMask.enabled) {
                probability *= 1.0 - maskValue(
                    operation.exclusionMask,
                    centreU,
                    centreV,
                    seed ^ 0x6578636c7564656dULL);
            }
            if (unitDouble(hashCoordinates(seed, x, y, 3)) >= probability) {
                continue;
            }

            const auto populationIndex = selectPopulation(
                operation.populations,
                unitDouble(hashCoordinates(seed, x, y, 4)));
            const auto& population = operation.populations[populationIndex];
            const double scale = interpolate(
                population.minimumScale,
                population.maximumScale,
                unitDouble(hashCoordinates(seed, x, y, 5)));
            const double aspect = interpolate(
                population.minimumAspect,
                population.maximumAspect,
                unitDouble(hashCoordinates(seed, x, y, 6)));
            const double rotation = interpolate(
                population.minimumRotation,
                population.maximumRotation,
                unitDouble(hashCoordinates(seed, x, y, 7)));
            const double colourAmount = unitDouble(hashCoordinates(seed, x, y, 8));
            const auto candidateIndex = row * operation.columns + column;
            ScatterInstance instance{
                makeRegionKey(keyDomain, x, y),
                hashCoordinates(seed, x, y, 9),
                hashCoordinates(seed, x, y, 10),
                candidateIndex,
                static_cast<std::uint32_t>(populationIndex),
                centreU,
                centreV,
                scale,
                aspect,
                rotation,
                interpolateColour(population.lowColour, population.highColour, colourAmount),
                interpolate(
                    population.minimumHeight,
                    population.maximumHeight,
                    unitDouble(hashCoordinates(seed, x, y, 11))),
                interpolate(
                    population.minimumRoughness,
                    population.maximumRoughness,
                    unitDouble(hashCoordinates(seed, x, y, 12))),
                unitDouble(hashCoordinates(seed, x, y, 13)),
            };
            candidates.push_back({
                std::move(instance),
                instanceRadius(operation, scale, aspect),
            });
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& first, const Candidate& second) {
        return first.instance.placementPriority < second.instance.placementPriority ||
            (first.instance.placementPriority == second.instance.placementPriority &&
             first.instance.candidateIndex < second.instance.candidateIndex);
    });

    layout.instances.reserve(candidates.size());
    std::vector<double> acceptedRadii;
    acceptedRadii.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        bool accepted = true;
        if (operation.minimumDistance > 0.0 ||
            operation.overlapMode != ScatterOverlapMode::unrestricted) {
            for (std::size_t index = 0; index < layout.instances.size(); ++index) {
                if (torusDistance(
                        candidate.instance.centreU,
                        candidate.instance.centreV,
                        layout.instances[index].centreU,
                        layout.instances[index].centreV) <
                    requiredSeparation(
                        operation,
                        candidate.radius,
                        acceptedRadii[index])) {
                    accepted = false;
                    break;
                }
            }
        }
        if (!accepted) {
            continue;
        }
        const auto acceptedIndex = static_cast<std::int32_t>(layout.instances.size());
        layout.cellInstanceIndices[candidate.instance.candidateIndex] = acceptedIndex;
        layout.maximumRadius = std::max(layout.maximumRadius, candidate.radius);
        layout.instances.push_back(candidate.instance);
        acceptedRadii.push_back(candidate.radius);
    }
    return layout;
}

ScatterSample evaluateScatter(
    const ScatterOperation& operation,
    const ScatterLayout& layout,
    double u,
    double v)
{
    if (layout.instances.empty()) {
        return {};
    }
    const double wrappedU = wrapUnit(u);
    const double wrappedV = wrapUnit(v);
    const auto baseColumn = static_cast<std::int64_t>(std::floor(
        wrappedU * static_cast<double>(layout.columns)));
    const auto baseRow = static_cast<std::int64_t>(std::floor(
        wrappedV * static_cast<double>(layout.rows)));
    const auto rangeX = static_cast<std::int64_t>(std::ceil(
        layout.maximumRadius * static_cast<double>(layout.columns))) + 1;
    const auto rangeY = static_cast<std::int64_t>(std::ceil(
        layout.maximumRadius * static_cast<double>(layout.rows))) + 1;
    const bool allColumns = rangeX * 2 + 1 >= static_cast<std::int64_t>(layout.columns);
    const bool allRows = rangeY * 2 + 1 >= static_cast<std::int64_t>(layout.rows);

    const ScatterInstance* selected = nullptr;
    double selectedCoverage = 0.0;
    double selectedDistance = 0.0;
    double selectedDeltaU = 0.0;
    double selectedDeltaV = 0.0;
    const auto visit = [&](std::uint32_t column, std::uint32_t row) {
        const auto cellIndex = static_cast<std::size_t>(row) * layout.columns + column;
        const auto instanceIndex = layout.cellInstanceIndices[cellIndex];
        if (instanceIndex < 0) {
            return;
        }
        const auto& instance = layout.instances[static_cast<std::size_t>(instanceIndex)];
        const double deltaU = torusDelta(wrappedU, instance.centreU);
        const double deltaV = torusDelta(wrappedV, instance.centreV);
        const double rootAspect = std::sqrt(instance.aspect);
        const double widthScale = instance.scale * rootAspect;
        const double heightScale = instance.scale / rootAspect;
        const double distance = shapeSignedDistance(
            operation.stamp,
            deltaU,
            deltaV,
            widthScale,
            heightScale,
            instance.rotationDegrees);
        const double distanceScale = std::min(widthScale, heightScale);
        const double coverage = shapeFieldCoverage(
            operation.stamp,
            distance,
            distanceScale);
        if (coverage <= 0.0 || (selected != nullptr && !occludes(instance, *selected))) {
            return;
        }
        selected = &instance;
        selectedCoverage = coverage;
        selectedDistance = distance;
        selectedDeltaU = deltaU;
        selectedDeltaV = deltaV;
    };

    const auto visitColumns = [&](std::uint32_t row) {
        if (allColumns) {
            for (std::uint32_t column = 0; column < layout.columns; ++column) {
                visit(column, row);
            }
            return;
        }
        for (std::int64_t offset = -rangeX; offset <= rangeX; ++offset) {
            visit(
                static_cast<std::uint32_t>(positiveModulo(baseColumn + offset, layout.columns)),
                row);
        }
    };
    if (allRows) {
        for (std::uint32_t row = 0; row < layout.rows; ++row) {
            visitColumns(row);
        }
    } else {
        for (std::int64_t offset = -rangeY; offset <= rangeY; ++offset) {
            visitColumns(static_cast<std::uint32_t>(
                positiveModulo(baseRow + offset, layout.rows)));
        }
    }

    if (selected == nullptr) {
        return {};
    }
    const double rootAspect = std::sqrt(selected->aspect);
    const double width = operation.stamp.width * selected->scale * rootAspect;
    const double height = operation.stamp.height * selected->scale / rootAspect;
    const double radians =
        (operation.stamp.rotationDegrees + selected->rotationDegrees) *
        std::numbers::pi / 180.0;
    const double sine = std::sin(radians);
    const double cosine = std::cos(radians);
    const double localX = cosine * selectedDeltaU + sine * selectedDeltaV;
    const double localY = -sine * selectedDeltaU + cosine * selectedDeltaV;
    const double halfWidth = width * 0.5;
    const double halfHeight = height * 0.5;
    const double centreDistance = std::clamp(
        std::hypot(localX / halfWidth, localY / halfHeight) / std::sqrt(2.0),
        0.0,
        1.0);
    const double boundaryScale = std::max(
        LayerLimits::minimumShapeDimension,
        std::min(halfWidth, halfHeight));
    const auto parentKey = mixBits(
        scatterDomain ^ static_cast<std::uint64_t>(selected->populationIndex));
    const RegionSample region{
        selected->key,
        std::clamp(localX / width + 0.5, 0.0, 1.0),
        std::clamp(localY / height + 0.5, 0.0, 1.0),
        centreDistance,
        std::clamp(-selectedDistance / boundaryScale, 0.0, 1.0),
        true,
        parentKey,
        true,
        (operation.stamp.rotationDegrees + selected->rotationDegrees) / 360.0,
    };
    return {
        selectedCoverage,
        selectedDistance,
        region.localU,
        region.localV,
        region.boundaryDistance,
        selected->colour,
        selected->height,
        selected->roughness,
        selected->random,
        selected->populationIndex,
        region,
    };
}

} // namespace paperweight
