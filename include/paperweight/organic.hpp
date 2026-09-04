#pragma once

#include <cstdint>
#include <vector>

#include <paperweight/layer.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

struct OrganicCellSample {
    double value{};
    double boundary{};
    double random{};
    RegionSample region;

    friend constexpr bool operator==(
        const OrganicCellSample&,
        const OrganicCellSample&) = default;
};

struct OrganicCrackSegment {
    double startU{};
    double startV{};
    double endU{};
    double endV{};
    double width{};
    std::uint32_t hierarchy{};
    std::uint64_t key{};

    friend constexpr bool operator==(
        const OrganicCrackSegment&,
        const OrganicCrackSegment&) = default;
};

struct OrganicCrackLayout {
    std::vector<OrganicCrackSegment> segments;

    friend bool operator==(
        const OrganicCrackLayout&,
        const OrganicCrackLayout&) = default;
};

struct OrganicCrackSample {
    double coverage{};
    double distance{1.0};
    double hierarchy{};
    RegionSample region;

    friend constexpr bool operator==(
        const OrganicCrackSample&,
        const OrganicCrackSample&) = default;
};

struct LeafInstance {
    double centreU{};
    double centreV{};
    double length{};
    double width{};
    double rotationDegrees{};
    Rgba8 colour{};
    double height{};
    double roughness{};
    double random{};
    std::uint64_t key{};
    std::uint64_t clusterKey{};
    std::uint64_t occlusionOrder{};
    std::uint32_t clusterCell{};
    LeafProfile profile{LeafProfile::ovate};
    std::uint32_t population{};
    double clusterRandom{};

    friend constexpr bool operator==(
        const LeafInstance&,
        const LeafInstance&) = default;
};

struct LeafClusterLayout {
    std::uint32_t columns{};
    std::uint32_t rows{};
    double maximumRadius{};
    std::vector<LeafInstance> instances;
    std::vector<std::vector<std::uint32_t>> cellInstanceIndices;

    friend bool operator==(
        const LeafClusterLayout&,
        const LeafClusterLayout&) = default;
};

struct LeafSample {
    double coverage{};
    double edge{};
    double midrib{};
    double veins{};
    double localU{};
    double localV{};
    Rgba8 colour{};
    double height{};
    double roughness{};
    double random{};
    RegionSample region;
    double outline{};
    double innerHighlight{};
    double clusterRandom{};
    double population{};

    friend constexpr bool operator==(const LeafSample&, const LeafSample&) = default;
};

struct OrganicAccumulationSample {
    double amount{};
    double variation{};
    double fill{};
    double outline{};
    double innerHighlight{};
    double detail{};

    friend constexpr bool operator==(
        const OrganicAccumulationSample&,
        const OrganicAccumulationSample&) = default;
};

[[nodiscard]] OrganicCellSample evaluateOrganicCells(
    const OrganicCellOperation& operation,
    double u,
    double v,
    std::uint64_t materialSeed);

[[nodiscard]] OrganicCrackLayout buildOrganicCrackLayout(
    const OrganicCrackOperation& operation,
    std::uint64_t materialSeed);

[[nodiscard]] OrganicCrackSample evaluateOrganicCracks(
    const OrganicCrackOperation& operation,
    const OrganicCrackLayout& layout,
    double u,
    double v);

[[nodiscard]] LeafClusterLayout buildLeafClusterLayout(
    const LeafClusterOperation& operation,
    std::uint64_t materialSeed);

[[nodiscard]] LeafSample evaluateLeafCluster(
    const LeafClusterOperation& operation,
    const LeafClusterLayout& layout,
    double u,
    double v);

[[nodiscard]] LeafClusterOperation leafSpeciesPreset(LeafSpecies species);

[[nodiscard]] OrganicAccumulationSample evaluateOrganicAccumulation(
    const OrganicAccumulationOperation& operation,
    double inputScalar,
    const RegionSample& region,
    double u,
    double v,
    std::uint64_t materialSeed);

} // namespace paperweight
