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
