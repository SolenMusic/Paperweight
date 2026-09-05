#include <paperweight/structural.hpp>

#include <paperweight/hash.hpp>

#include "noise_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace paperweight {
namespace {

constexpr std::uint64_t courseDomain = 0x636f757273656964ULL;
constexpr std::uint64_t blockDomain = 0x636f75727365626cULL;
constexpr std::uint64_t horizontalCrookDomain = 0x636f757273656863ULL;
constexpr std::uint64_t verticalCrookDomain = 0x636f757273657663ULL;

struct LayoutCounts {
    std::uint32_t blocks;
    std::uint32_t courses;
};

struct PartitionLocation {
    std::uint32_t index;
    double start;
    double end;
    double local;
};

LayoutCounts resolvedCounts(
    const CourseLayoutOperation& operation,
    const PhysicalSize& materialSize)
{
    if (!operation.physicalDimensions) {
        return {operation.blocks, operation.courses};
    }
    const auto& physical = *operation.physicalDimensions;
    return {
        static_cast<std::uint32_t>(std::clamp(
            std::llround(materialSize.widthMetres / physical.blockWidthMetres),
            1LL,
            64LL)),
        static_cast<std::uint32_t>(std::clamp(
            std::llround(materialSize.heightMetres / physical.courseHeightMetres),
            1LL,
            64LL)),
    };
}

std::uint64_t profiledDomain(
    std::uint64_t domain,
    CourseLayoutProfile profile)
{
    return domain ^ mixBits(static_cast<std::uint64_t>(profile));
}

std::uint64_t courseKey(
    CourseLayoutProfile profile,
    std::uint32_t course)
{
    return makeRegionKey(
        profiledDomain(courseDomain, profile),
        static_cast<std::int64_t>(course),
        0);
}

std::array<double, LayerLimits::maximumPatternCount> courseWeights(
    const CourseLayoutOperation& operation,
    std::uint32_t count,
    std::uint64_t materialSeed)
{
    std::array<double, LayerLimits::maximumPatternCount> weights{};
    double total = 0.0;
    for (std::uint32_t index = 0; index < count; ++index) {
        const double random = regionRandom(
            materialSeed,
            courseKey(operation.profile, index),
            operation.seedOffset,
            0);
        const double weight = 1.0 +
            (random * 2.0 - 1.0) * operation.courseVariation * 0.75;
        weights[index] = weight;
        total += weight;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        weights[index] /= total;
    }
    return weights;
}

std::uint32_t blockCountForCourse(
    const CourseLayoutOperation& operation,
    std::uint32_t baseCount,
    std::uint32_t course,
    std::uint64_t materialSeed)
{
    if (operation.profile != CourseLayoutProfile::slabs ||
        operation.blockVariation == 0.0) {
        return baseCount;
    }
    const auto key = courseKey(operation.profile, course);
    const double random = regionRandom(
        materialSeed,
        key,
        operation.seedOffset,
        23);
    constexpr std::int32_t maximumDelta = 2;
    const auto delta = static_cast<std::int32_t>(std::llround(
        (random * 2.0 - 1.0) * operation.blockVariation * maximumDelta));
    const auto varied = static_cast<std::int32_t>(baseCount) + delta;
    return static_cast<std::uint32_t>(std::clamp(varied, 1, 64));
}

std::array<double, LayerLimits::maximumPatternCount> blockWeights(
    const CourseLayoutOperation& operation,
    std::uint32_t course,
    std::uint32_t count,
    std::uint64_t materialSeed)
{
    std::array<double, LayerLimits::maximumPatternCount> weights{};
    double total = 0.0;
    const auto domain = profiledDomain(blockDomain, operation.profile);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto key = makeRegionKey(
            domain,
            static_cast<std::int64_t>(course),
            static_cast<std::int64_t>(index));
        const double random = regionRandom(
            materialSeed,
            key,
            operation.seedOffset,
            1);
        const double weight = 1.0 +
            (random * 2.0 - 1.0) * operation.blockVariation * 0.75;
        weights[index] = weight;
        total += weight;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        weights[index] /= total;
    }
    return weights;
}

double horizontalBoundaryOffset(
    const CourseLayoutOperation& operation,
    const std::array<double, LayerLimits::maximumPatternCount>& heights,
    std::uint32_t boundary,
    std::uint32_t courseCount,
    double u,
    std::uint64_t materialSeed)
{
    if (boundary == 0 || boundary == courseCount || operation.crookedness == 0.0) {
        return 0.0;
    }
    const double safeHeight = std::min(heights[boundary - 1], heights[boundary]);
    const auto seed = mixBits(
        materialSeed ^ mixBits(operation.seedOffset) ^
        horizontalCrookDomain ^ static_cast<std::uint64_t>(operation.profile));
    const double noise = detail::periodicValueNoise2DUnchecked(
        wrapUnit(u) * 4.0,
        static_cast<double>(boundary),
        4,
        std::max(1U, courseCount),
        seed);
    return (noise * 2.0 - 1.0) * safeHeight *
        operation.crookedness * 0.22;
}

PartitionLocation locateCourse(
    const CourseLayoutOperation& operation,
    const std::array<double, LayerLimits::maximumPatternCount>& heights,
    std::uint32_t courseCount,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const double wrapped = wrapUnit(v);
    std::uint32_t index = 0;
    double baseStart = 0.0;
    while (index + 1 < courseCount && wrapped >= baseStart + heights[index]) {
        baseStart += heights[index];
        ++index;
    }

    const auto boundsFor = [&](std::uint32_t selected, double selectedBaseStart) {
        const double selectedBaseEnd = selectedBaseStart + heights[selected];
        const double start = selectedBaseStart + horizontalBoundaryOffset(
            operation, heights, selected, courseCount, u, materialSeed);
        const double end = selected + 1 == courseCount
            ? 1.0
            : selectedBaseEnd + horizontalBoundaryOffset(
                operation, heights, selected + 1, courseCount, u, materialSeed);
        return std::array<double, 2>{start, end};
    };

    auto bounds = boundsFor(index, baseStart);
    if (wrapped < bounds[0] && index > 0) {
        --index;
        baseStart -= heights[index];
        bounds = boundsFor(index, baseStart);
    } else if (wrapped >= bounds[1] && index + 1 < courseCount) {
        baseStart += heights[index];
        ++index;
        bounds = boundsFor(index, baseStart);
    }

    const double local = std::clamp(
        (wrapped - bounds[0]) / (bounds[1] - bounds[0]),
        0.0,
        1.0);
    return {index, bounds[0], bounds[1], local};
}

double verticalBoundaryOffset(
    const CourseLayoutOperation& operation,
    std::uint32_t course,
    const std::array<double, LayerLimits::maximumPatternCount>& widths,
    std::uint32_t boundary,
    std::uint32_t blockCount,
    double localV,
    std::uint64_t materialSeed)
{
    if (boundary == 0 || boundary == blockCount || operation.crookedness == 0.0) {
        return 0.0;
    }
    const double safeWidth = std::min(widths[boundary - 1], widths[boundary]);
    const auto seed = mixBits(mixBits(
        materialSeed ^ mixBits(operation.seedOffset) ^
        verticalCrookDomain ^ static_cast<std::uint64_t>(operation.profile)) ^
        courseKey(operation.profile, course) ^ mixBits(boundary));
    const double noise = detail::periodicValueNoise2DUnchecked(
        localV * 4.0,
        0.0,
        4,
        1,
        seed);
    return (noise * 2.0 - 1.0) * safeWidth *
        operation.crookedness * 0.22;
}

PartitionLocation locateBlock(
    const CourseLayoutOperation& operation,
    std::uint32_t course,
    const std::array<double, LayerLimits::maximumPatternCount>& widths,
    std::uint32_t blockCount,
    double coordinate,
    double localV,
    std::uint64_t materialSeed)
{
    const double wrapped = wrapUnit(coordinate);
    std::uint32_t index = 0;
    double start = 0.0;
    while (index + 1 < blockCount && wrapped >= start + widths[index]) {
        start += widths[index];
        ++index;
    }

    const auto boundsFor = [&](std::uint32_t selected, double selectedStart) {
        const double selectedEnd = selectedStart + widths[selected];
        const double crookedStart = selectedStart + verticalBoundaryOffset(
            operation,
            course,
            widths,
            selected,
            blockCount,
            localV,
            materialSeed);
        const double crookedEnd = selected + 1 == blockCount
            ? 1.0
            : selectedEnd + verticalBoundaryOffset(
                operation,
                course,
                widths,
                selected + 1,
                blockCount,
                localV,
                materialSeed);
        return std::array<double, 2>{crookedStart, crookedEnd};
    };

    auto bounds = boundsFor(index, start);
    if (wrapped < bounds[0] && index > 0) {
        --index;
        start -= widths[index];
        bounds = boundsFor(index, start);
    } else if (wrapped >= bounds[1] && index + 1 < blockCount) {
        start += widths[index];
        ++index;
        bounds = boundsFor(index, start);
    }

    const double local = std::clamp(
        (wrapped - bounds[0]) / (bounds[1] - bounds[0]),
        0.0,
        1.0);
    return {index, bounds[0], bounds[1], local};
}

double selectedValue(
    const CourseLayoutSample& sample,
    CourseLayoutField field)
{
    switch (field) {
    case CourseLayoutField::blocks:
        return sample.blocks;
    case CourseLayoutField::mortar:
        return sample.mortar;
    case CourseLayoutField::course:
        return sample.course;
    case CourseLayoutField::overlap:
        return sample.overlap;
    }
    return 0.0;
}

} // namespace

CourseLayoutSample evaluateCourseLayoutFields(
    const CourseLayoutOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const auto counts = resolvedCounts(operation, materialSize);
    const auto heights = courseWeights(operation, counts.courses, materialSeed);
    const auto course = locateCourse(
        operation, heights, counts.courses, u, v, materialSeed);
    const auto blockCount = blockCountForCourse(
        operation, counts.blocks, course.index, materialSeed);
    const auto widths = blockWeights(
        operation, course.index, blockCount, materialSeed);
    const double stagger = (course.index % 2U == 0U)
        ? 0.0
        : operation.stagger / static_cast<double>(blockCount);
    const auto block = locateBlock(
        operation,
        course.index,
        widths,
        blockCount,
        u - stagger,
        course.local,
        materialSeed);

    const double averageBlockWidth = 1.0 / static_cast<double>(counts.blocks);
    const double averageCourseHeight = 1.0 / static_cast<double>(counts.courses);
    double gapX = operation.gap * std::min(averageBlockWidth, averageCourseHeight);
    double gapY = gapX;
    double overlapFraction = operation.overlap;
    if (operation.physicalDimensions) {
        const auto& physical = *operation.physicalDimensions;
        gapX = physical.gapMetres / materialSize.widthMetres;
        gapY = physical.gapMetres / materialSize.heightMetres;
        overlapFraction = physical.overlapMetres / physical.courseHeightMetres;
    }
    const double softness = operation.softness *
        std::min(averageBlockWidth, averageCourseHeight);
    const double blockWidth = block.end - block.start;
    const double courseHeight = course.end - course.start;
    const double blockEdge = std::min(block.local, 1.0 - block.local) * blockWidth;
    const double courseEdge = std::min(course.local, 1.0 - course.local) * courseHeight;
    const double verticalCoverage = smoothCoverage(blockEdge - gapX * 0.5, softness);
    const double courseCoverage = smoothCoverage(courseEdge - gapY * 0.5, softness);
    const double blocks = verticalCoverage * courseCoverage;

    double overlap = 0.0;
    if (operation.profile == CourseLayoutProfile::slates && overlapFraction > 0.0) {
        const double localSoftness = courseHeight > 0.0 ? softness / courseHeight : 0.0;
        overlap = verticalCoverage * smoothCoverage(
            course.local - (1.0 - overlapFraction),
            localSoftness);
    }

    const double centreDistance = std::clamp(
        std::sqrt(
            (block.local - 0.5) * (block.local - 0.5) +
            (course.local - 0.5) * (course.local - 0.5)) * std::sqrt(2.0),
        0.0,
        1.0);
    const double boundaryDistance = std::clamp(
        2.0 * std::min({
            block.local,
            1.0 - block.local,
            course.local,
            1.0 - course.local,
        }),
        0.0,
        1.0);
    const auto parent = courseKey(operation.profile, course.index);
    const auto region = RegionSample{
        makeRegionKey(
            profiledDomain(blockDomain, operation.profile),
            static_cast<std::int64_t>(course.index),
            static_cast<std::int64_t>(block.index)),
        block.local,
        course.local,
        centreDistance,
        boundaryDistance,
        true,
        parent,
        true,
        0.0,
        blockWidth,
        courseHeight,
    };
    return {
        blocks,
        1.0 - blocks,
        courseCoverage,
        overlap,
        region,
    };
}

StructuralSample evaluateCourseLayoutSample(
    const CourseLayoutOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v,
    std::uint64_t materialSeed)
{
    const auto fields = evaluateCourseLayoutFields(
        operation, materialSize, u, v, materialSeed);
    return {selectedValue(fields, operation.field), fields.region};
}

} // namespace paperweight
