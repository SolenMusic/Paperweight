#pragma once

#include <cstdint>

#include <paperweight/layer.hpp>

namespace paperweight {

struct RepeatedCoordinate {
    std::int64_t index{};
    double local{};

    friend constexpr bool operator==(
        const RepeatedCoordinate&,
        const RepeatedCoordinate&) = default;
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

[[nodiscard]] double evaluateTileGrid(
    const TileGridOperation& operation,
    double u,
    double v);

[[nodiscard]] double evaluateWorleyCells(
    const WorleyCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] double evaluateRandomCells(
    const RandomCellsOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] double evaluateLines(
    const LinesOperation& operation,
    double u,
    double v);

[[nodiscard]] double evaluateRectangles(
    const RectanglesOperation& operation,
    double u,
    double v);

[[nodiscard]] double evaluateCircles(
    const CirclesOperation& operation,
    double u,
    double v);

} // namespace paperweight
