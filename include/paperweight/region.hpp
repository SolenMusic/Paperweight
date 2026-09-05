#pragma once

#include <cstdint>

namespace paperweight {

struct RegionSample {
    std::uint64_t key{};
    double localU{};
    double localV{};
    double centreDistance{};
    double boundaryDistance{};
    bool valid{};
    std::uint64_t parentKey{};
    bool parentValid{};
    // Clockwise orientation of the region-local U axis, expressed as turns.
    // localU/localV are already evaluated in this frame; this value lets
    // consumers orient attached presentation or geometry consistently.
    double orientationTurns{};
    // Approximate region extents in material UV space. New physical-detail
    // processors use these to convert authored metre widths into region-local
    // distances without depending on output resolution.
    double extentU{1.0};
    double extentV{1.0};

    friend constexpr bool operator==(const RegionSample&, const RegionSample&) = default;
};

[[nodiscard]] std::uint64_t makeRegionKey(
    std::uint64_t domain,
    std::int64_t x,
    std::int64_t y) noexcept;

[[nodiscard]] double regionRandom(
    std::uint64_t materialSeed,
    std::uint64_t regionKey,
    std::uint64_t seedOffset,
    std::uint32_t channel) noexcept;

} // namespace paperweight
