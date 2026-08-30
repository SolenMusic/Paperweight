#include <paperweight/hash.hpp>

#include <bit>

namespace paperweight {

std::uint64_t hashCoordinates(
    std::uint64_t seed,
    std::int64_t x,
    std::int64_t y,
    std::uint64_t stream) noexcept
{
    const auto xBits = mixBits(static_cast<std::uint64_t>(x));
    const auto yBits = std::rotl(mixBits(static_cast<std::uint64_t>(y)), 23);
    return mixBits(seed ^ xBits ^ yBits ^ mixBits(stream));
}

double unitDouble(std::uint64_t bits) noexcept
{
    constexpr double denominator = 9007199254740992.0;
    return static_cast<double>(bits >> 11U) / denominator;
}

} // namespace paperweight
