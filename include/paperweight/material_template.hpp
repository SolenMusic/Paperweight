#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <paperweight/material.hpp>

namespace paperweight {

// A recipe deliberately has no seed. It becomes a Material only when a caller
// chooses a seed, which keeps templates reusable without hiding identity in them.
struct MaterialRecipe {
    std::uint32_t frequency{4};
    std::uint32_t octaves{5};
    std::uint32_t lacunarity{2};
    double gain{0.5};
    Rgba8 lowColour{0, 0, 0, 255};
    Rgba8 highColour{255, 255, 255, 255};
    double normalStrength{1.0};
    double roughnessLow{0.25};
    double roughnessHigh{0.85};
    std::vector<MaterialLayer> layers;
    PhysicalSize physicalSize;

    friend bool operator==(const MaterialRecipe&, const MaterialRecipe&) = default;
};

[[nodiscard]] MaterialRecipe makeMaterialRecipe(const Material& material);
[[nodiscard]] Material instantiateMaterial(
    const MaterialRecipe& recipe,
    std::uint64_t seed);

enum class TemplateProperty : std::uint8_t {
    normalStrength,
    layerOpacity,
    courseBlocks,
    courseCount,
    courseBlockVariation,
    courseCrookedness,
    courseGap,
    courseOverlap,
    latticeWidth,
    shapeBorderWidth,
    scatterDensity,
    scatterMinimumDistance,
    scatterStampSize,
    organicCellColumns,
    organicCellRows,
    organicCellAnisotropy,
    organicCellGap,
    organicCrackWidth,
    organicAccumulationCoverage,
    leafDensity,
    leafClusterSpread,
    leafLength,
    leafWidth,
    regionBevelWidth,
    regionFacetStrength,
    regionWear,
    surfaceDetail,
    surfaceDistortion,
    worleyColumns,
    worleyRows,
    worleyEdgeWidth,
};

struct TemplateControlBinding {
    TemplateProperty property{};
    std::size_t layerIndex{};
    double outputMinimum{};
    double outputMaximum{1.0};

    friend constexpr bool operator==(
        const TemplateControlBinding&,
        const TemplateControlBinding&) = default;
};

struct TemplateControl {
    std::string_view key;
    std::string_view displayName;
    double minimumValue{};
    double maximumValue{1.0};
    double defaultValue{0.5};
    double step{0.01};
    std::vector<TemplateControlBinding> bindings;
};

struct ReferenceMaterialTemplate {
    std::string_view identifier;
    std::string_view displayName;
    std::string_view recipeResourceName;
    std::string_view referenceFileName;
    std::string_view description;
    std::vector<TemplateControl> controls;
};

[[nodiscard]] const std::vector<ReferenceMaterialTemplate>&
referenceMaterialTemplates();

[[nodiscard]] const ReferenceMaterialTemplate* findReferenceMaterialTemplate(
    std::string_view identifier);

// Applies one high-level control to an instantiated material. One control may
// drive several low-level properties while preserving all unrelated settings.
[[nodiscard]] std::optional<std::string> applyTemplateControl(
    Material& material,
    const TemplateControl& control,
    double value);

} // namespace paperweight
