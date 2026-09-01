#pragma once

#include <cstdint>
#include <vector>

#include <paperweight/layer.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

struct ScatterInstance {
    std::uint64_t key{};
    std::uint64_t placementPriority{};
    std::uint64_t occlusionPriority{};
    std::uint32_t candidateIndex{};
    std::uint32_t populationIndex{};
    double centreU{};
    double centreV{};
    double scale{1.0};
    double aspect{1.0};
    double rotationDegrees{};
    Rgba8 colour{};
    double height{};
    double roughness{};
    double random{};

    friend constexpr bool operator==(const ScatterInstance&, const ScatterInstance&) = default;
};

// The layout is a deterministic material-space result. It contains no pixel or
// worker information and may be reused for every output and resolution.
struct ScatterLayout {
    std::uint32_t columns{};
    std::uint32_t rows{};
    std::vector<ScatterInstance> instances;
    std::vector<std::int32_t> cellInstanceIndices;
    double maximumRadius{};

    friend bool operator==(const ScatterLayout&, const ScatterLayout&) = default;
};

struct ScatterSample {
    double coverage{};
    double signedDistance{};
    double localU{};
    double localV{};
    double boundaryDistance{};
    Rgba8 colour{};
    double height{};
    double roughness{};
    double random{};
    std::uint32_t populationIndex{};
    RegionSample region;

    friend constexpr bool operator==(const ScatterSample&, const ScatterSample&) = default;
};

[[nodiscard]] ScatterLayout buildScatterLayout(
    const ScatterOperation& operation,
    std::uint64_t materialSeed);

[[nodiscard]] ScatterSample evaluateScatter(
    const ScatterOperation& operation,
    const ScatterLayout& layout,
    double u,
    double v);

} // namespace paperweight
