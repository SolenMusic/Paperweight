#pragma once

#include <cstdint>

#include <paperweight/layer.hpp>
#include <paperweight/physical.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

struct RepeatedCoordinate {
    std::int64_t index{};
    double local{};

    friend constexpr bool operator==(
        const RepeatedCoordinate&,
        const RepeatedCoordinate&) = default;
};

struct StructuralSample {
    double value{};
    RegionSample region;

    friend constexpr bool operator==(
        const StructuralSample&,
        const StructuralSample&) = default;
};

struct CourseLayoutSample {
    double blocks{};
    double mortar{};
    double course{};
    double overlap{};
    RegionSample region;

    friend constexpr bool operator==(
        const CourseLayoutSample&,
        const CourseLayoutSample&) = default;
};

[[nodiscard]] double wrapUnit(double value);

[[nodiscard]] RepeatedCoordinate repeatedCoordinate(
    double value,
    std::uint32_t count);

[[nodiscard]] double smoothCoverage(
    double signedDistance,
    double softness);

[[nodiscard]] double evaluateBrickGrid(
    const BrickGridOperation& operation,
    double u,
    double v);

[[nodiscard]] StructuralSample evaluateBrickGridSample(
    const BrickGridOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v);

[[nodiscard]] double evaluateBrickGrid(
    const BrickGridOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v);

[[nodiscard]] double evaluateTileGrid(
    const TileGridOperation& operation,
    double u,
    double v);

[[nodiscard]] StructuralSample evaluateTileGridSample(
    const TileGridOperation& operation,
    double u,
    double v);

[[nodiscard]] CourseLayoutSample evaluateCourseLayoutFields(
    const CourseLayoutOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] StructuralSample evaluateCourseLayoutSample(
    const CourseLayoutOperation& operation,
    const PhysicalSize& materialSize,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] double evaluateWorleyCells(
    const WorleyCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] StructuralSample evaluateWorleyCellsSample(
    const WorleyCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] double evaluateRandomCells(
    const RandomCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] StructuralSample evaluateRandomCellsSample(
    const RandomCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] double evaluateLines(
    const LinesOperation& operation,
    double u,
    double v);

[[nodiscard]] StructuralSample evaluateLinesSample(
    const LinesOperation& operation,
    double u,
    double v);

[[nodiscard]] double evaluateRectangles(
    const RectanglesOperation& operation,
    double u,
    double v);

[[nodiscard]] StructuralSample evaluateRectanglesSample(
    const RectanglesOperation& operation,
    double u,
    double v);

[[nodiscard]] double evaluateCircles(
    const CirclesOperation& operation,
    double u,
    double v);

[[nodiscard]] StructuralSample evaluateCirclesSample(
    const CirclesOperation& operation,
    double u,
    double v);

} // namespace paperweight
