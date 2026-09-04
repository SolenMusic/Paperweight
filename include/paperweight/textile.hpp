#pragma once

#include <paperweight/layer.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

// All fields are evaluated in normalised material space. The operation is
// periodic, seed-stable, and independent of output resolution and worker count.
struct TextileSample {
    double coverage{};
    double height{};
    double warp{};
    double weft{};
    double overUnder{};
    double fibres{};
    double pile{};
    double damage{};
    double colourVariation{};
    double direction{};
    double roughness{1.0};
    Rgba8 colour{};
    RegionSample region;

    friend constexpr bool operator==(const TextileSample&, const TextileSample&) = default;
};

[[nodiscard]] TextileSample evaluateTextile(
    const TextileOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

} // namespace paperweight
