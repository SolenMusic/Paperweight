#pragma once

#include <cstdint>

#include <paperweight/layer.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

struct RegionAnchorPoint {
    double u{0.5};
    double v{0.5};
    double orientationTurns{};
    std::uint64_t key{};

    friend constexpr bool operator==(
        const RegionAnchorPoint&,
        const RegionAnchorPoint&) = default;
};

struct RegionAttachmentSample {
    double coverage{};
    double signedDistance{1.0};
    double localU{0.5};
    double localV{0.5};
    double variation{};
    RegionSample region;

    friend constexpr bool operator==(
        const RegionAttachmentSample&,
        const RegionAttachmentSample&) = default;
};

// Named anchors are derived entirely from the source region identity, material
// seed, operation seed offset, and attachment index. They therefore remain
// stable across output resolutions, worker counts, and traversal order.
[[nodiscard]] RegionAnchorPoint resolveRegionAnchor(
    const RegionSample& region,
    RegionAnchor anchor,
    std::uint32_t attachmentIndex,
    std::uint32_t attachmentCount,
    std::uint64_t materialSeed,
    std::uint64_t seedOffset,
    double inset = 0.12);

// Evaluate a reusable attached detail in the source region's local frame.
// The same sample coverage is used for every material output.
[[nodiscard]] RegionAttachmentSample evaluateRegionAttachment(
    const RegionAttachmentOperation& operation,
    const RegionSample& region,
    std::uint64_t materialSeed);

} // namespace paperweight
