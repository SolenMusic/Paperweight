#include <paperweight/region.hpp>

#include <paperweight/hash.hpp>

namespace paperweight {

std::uint64_t makeRegionKey(
    std::uint64_t domain,
    std::int64_t x,
    std::int64_t y) noexcept
{
    return hashCoordinates(domain, x, y, 0x726567696f6e6b65ULL);
}

double regionRandom(
    std::uint64_t materialSeed,
    std::uint64_t regionKey,
    std::uint64_t seedOffset,
    std::uint32_t channel) noexcept
{
    constexpr std::uint64_t domain = 0x726567696f6e726eULL;
    const auto seed = mixBits(materialSeed ^ mixBits(seedOffset) ^ domain);
    return unitDouble(mixBits(seed ^ regionKey ^ mixBits(channel)));
}

} // namespace paperweight
