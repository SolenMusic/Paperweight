#pragma once

#include <cstdint>

namespace paperweight {

[[nodiscard]] constexpr std::uint64_t mixBits(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] std::uint64_t hashCoordinates(
    std::uint64_t seed,
    std::int64_t x,
    std::int64_t y,
    std::uint64_t stream = 0) noexcept;

[[nodiscard]] double unitDouble(std::uint64_t bits) noexcept;

} // namespace paperweight
