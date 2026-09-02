#include <paperweight/material_template.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace paperweight {
namespace {

TemplateControl control(
    std::string_view key,
    std::string_view name,
    double minimum,
    double maximum,
    double defaultValue,
    double step,
    std::initializer_list<TemplateControlBinding> bindings)
{
    return {key, name, minimum, maximum, defaultValue, step, bindings};
}

std::optional<std::string> applyBinding(
    Material& material,
    const TemplateControlBinding& binding,
    double value)
{
    if (binding.property == TemplateProperty::normalStrength) {
        material.normalStrength = std::clamp(
            value,
            MaterialLimits::minimumNormalStrength,
            MaterialLimits::maximumNormalStrength);
        return std::nullopt;
    }
    if (binding.layerIndex >= material.layers.size()) {
        return "template control refers to a missing layer";
    }

    auto& layer = material.layers[binding.layerIndex];
    const auto integral = [value]() {
        return static_cast<std::uint32_t>(std::max(0.0, std::round(value)));
    };
    switch (binding.property) {
    case TemplateProperty::normalStrength:
        break;
    case TemplateProperty::layerOpacity:
        layer.opacity = std::clamp(value, 0.0, 1.0);
        return std::nullopt;
    case TemplateProperty::courseBlocks:
    case TemplateProperty::courseCount:
    case TemplateProperty::courseBlockVariation:
    case TemplateProperty::courseCrookedness:
    case TemplateProperty::courseGap:
    case TemplateProperty::courseOverlap:
        if (auto* operation = std::get_if<CourseLayoutOperation>(&layer.operation)) {
            if (binding.property == TemplateProperty::courseBlocks) {
                operation->blocks = integral();
                if (operation->physicalDimensions) {
                    operation->physicalDimensions->blockWidthMetres =
                        material.physicalSize.widthMetres /
                        static_cast<double>(std::max(1U, operation->blocks));
                }
            } else if (binding.property == TemplateProperty::courseCount) {
                operation->courses = integral();
                if (operation->physicalDimensions) {
                    operation->physicalDimensions->courseHeightMetres =
                        material.physicalSize.heightMetres /
                        static_cast<double>(std::max(1U, operation->courses));
                }
            } else if (binding.property == TemplateProperty::courseBlockVariation) {
                operation->blockVariation = value;
            } else if (binding.property == TemplateProperty::courseCrookedness) {
                operation->crookedness = value;
            } else if (binding.property == TemplateProperty::courseGap) {
                operation->gap = value;
                if (operation->physicalDimensions) {
                    operation->physicalDimensions->gapMetres = value * std::min(
                        operation->physicalDimensions->blockWidthMetres,
                        operation->physicalDimensions->courseHeightMetres);
                }
            } else {
                operation->overlap = value;
                if (operation->physicalDimensions) {
                    operation->physicalDimensions->overlapMetres = value *
                        operation->physicalDimensions->courseHeightMetres;
                }
            }
            return std::nullopt;
        }
        break;
    case TemplateProperty::latticeWidth:
        if (auto* operation = std::get_if<LatticeOperation>(&layer.operation)) {
            operation->width = value;
            return std::nullopt;
        }
        break;
    case TemplateProperty::shapeBorderWidth:
        if (auto* operation = std::get_if<ShapePrimitiveOperation>(&layer.operation)) {
            operation->borderWidth = value;
            return std::nullopt;
        }
        if (auto* operation = std::get_if<ShapeBooleanOperation>(&layer.operation)) {
            operation->shape.borderWidth = value;
            return std::nullopt;
        }
        break;
    case TemplateProperty::scatterDensity:
    case TemplateProperty::scatterMinimumDistance:
    case TemplateProperty::scatterStampSize:
        if (auto* operation = std::get_if<ScatterOperation>(&layer.operation)) {
            if (binding.property == TemplateProperty::scatterDensity) {
                operation->density = value;
            } else if (binding.property == TemplateProperty::scatterMinimumDistance) {
                operation->minimumDistance = value;
            } else {
                operation->stamp.width = value;
                operation->stamp.height = value;
            }
            return std::nullopt;
        }
        break;
    case TemplateProperty::organicCellColumns:
    case TemplateProperty::organicCellRows:
    case TemplateProperty::organicCellAnisotropy:
    case TemplateProperty::organicCellGap:
        if (auto* operation = std::get_if<OrganicCellOperation>(&layer.operation)) {
            if (binding.property == TemplateProperty::organicCellColumns) {
                operation->columns = integral();
            } else if (binding.property == TemplateProperty::organicCellRows) {
                operation->rows = integral();
            } else if (binding.property == TemplateProperty::organicCellAnisotropy) {
                operation->anisotropy = value;
            } else {
                operation->gap = value;
            }
            return std::nullopt;
        }
        break;
    case TemplateProperty::organicCrackWidth:
        if (auto* operation = std::get_if<OrganicCrackOperation>(&layer.operation)) {
            operation->width = value;
            return std::nullopt;
        }
        break;
    case TemplateProperty::organicAccumulationCoverage:
        if (auto* operation = std::get_if<OrganicAccumulationOperation>(&layer.operation)) {
            operation->coverage = value;
            return std::nullopt;
        }
        break;
    case TemplateProperty::leafDensity:
    case TemplateProperty::leafClusterSpread:
    case TemplateProperty::leafLength:
    case TemplateProperty::leafWidth:
        if (auto* operation = std::get_if<LeafClusterOperation>(&layer.operation)) {
            if (binding.property == TemplateProperty::leafDensity) {
                operation->density = std::clamp(value, 0.0, 1.0);
            } else if (binding.property == TemplateProperty::leafClusterSpread) {
                operation->clusterSpread = value;
            } else if (binding.property == TemplateProperty::leafLength) {
                operation->leafLength = value;
            } else {
                operation->leafWidth = value;
            }
            return std::nullopt;
        }
        break;
    case TemplateProperty::regionBevelWidth:
    case TemplateProperty::regionFacetStrength:
    case TemplateProperty::regionWear:
        if (auto* operation = std::get_if<RegionSurfaceOperation>(&layer.operation)) {
            if (binding.property == TemplateProperty::regionBevelWidth) {
                operation->bevelWidth = value;
            } else if (binding.property == TemplateProperty::regionFacetStrength) {
                operation->facetStrength = value;
            } else {
                operation->wearAmount = value;
            }
            return std::nullopt;
        }
        break;
    case TemplateProperty::surfaceDetail:
    case TemplateProperty::surfaceDistortion:
        if (auto* operation = std::get_if<SurfacePatternOperation>(&layer.operation)) {
            if (binding.property == TemplateProperty::surfaceDetail) {
                operation->detail = value;
            } else {
                operation->distortion = value;
            }
            return std::nullopt;
        }
        break;
    case TemplateProperty::worleyColumns:
    case TemplateProperty::worleyRows:
    case TemplateProperty::worleyEdgeWidth:
        if (auto* operation = std::get_if<WorleyCellsOperation>(&layer.operation)) {
            if (binding.property == TemplateProperty::worleyColumns) {
                operation->columns = integral();
            } else if (binding.property == TemplateProperty::worleyRows) {
                operation->rows = integral();
            } else {
                operation->edgeWidth = value;
            }
            return std::nullopt;
        }
        break;
    }
    return "template control does not match its target layer";
}

} // namespace

MaterialRecipe makeMaterialRecipe(const Material& material)
{
    return {
        material.frequency,
        material.octaves,
        material.lacunarity,
        material.gain,
        material.lowColour,
        material.highColour,
        material.normalStrength,
        material.roughnessLow,
        material.roughnessHigh,
        material.layers,
        material.physicalSize,
    };
}

Material instantiateMaterial(const MaterialRecipe& recipe, std::uint64_t seed)
{
    return {
        seed,
        recipe.frequency,
        recipe.octaves,
        recipe.lacunarity,
        recipe.gain,
        recipe.lowColour,
        recipe.highColour,
        recipe.normalStrength,
        recipe.roughnessLow,
        recipe.roughnessHigh,
        recipe.layers,
        recipe.physicalSize,
        std::nullopt,
    };
}

const std::vector<ReferenceMaterialTemplate>& referenceMaterialTemplates()
{
    using P = TemplateProperty;
    static const std::vector<ReferenceMaterialTemplate> templates{
        {"castle-flagstone", "Castle Flagstone", "castle-flagstone", "castle_flagstone.bmp",
         "Broad irregular floor slabs with worn joints.",
         {control("slab-count", "Slab count", 2, 7, 4, 1, {{P::courseBlocks, 0, 2, 7}, {P::courseCount, 0, 2, 7}}),
          control("unevenness", "Unevenness", 0, 100, 72, 1, {{P::courseBlockVariation, 0, 0, 1}, {P::courseCrookedness, 0, 0, 0.25}}),
          control("joint-width", "Joint width", 0.02, 0.16, 0.08, 0.005, {{P::courseGap, 0, 0.02, 0.16}}),
          control("surface-detail", "Surface detail", 0, 0.35, 0.16, 0.01, {{P::layerOpacity, 2, 0, 0.35}})}},
        {"castle-foliage", "Castle Foliage", "castle-foliage", "castle_foliage.bmp",
         "Dense overlapping broadleaf canopy with mossy variation.",
         {control("foliage-density", "Foliage density", 0, 100, 50, 1, {{P::leafDensity, 0, 0.78, 1.10}, {P::leafDensity, 1, 0.52, 1.0}, {P::leafDensity, 2, 0.52, 1.0}}),
          control("leaf-size", "Leaf size", 0, 100, 50, 1, {{P::leafLength, 0, 0.075, 0.155}, {P::leafWidth, 0, 0.045, 0.099}, {P::leafLength, 1, 0.09, 0.19}, {P::leafLength, 2, 0.09, 0.19}}),
          control("cluster-spread", "Cluster spread", 0.035, 0.14, 0.075, 0.005, {{P::leafClusterSpread, 0, 0.035, 0.14}, {P::leafClusterSpread, 1, 0.045, 0.165}, {P::leafClusterSpread, 2, 0.045, 0.165}}),
          control("moss", "Moss coverage", 0.15, 0.9, 0.55, 0.01, {{P::organicAccumulationCoverage, 3, 0.15, 0.9}})}},
        {"castle-roof", "Castle Roof", "castle-roof", "castle_roof.bmp",
         "Overlapping, slightly crooked slate courses.",
         {control("slate-count", "Slate count", 4, 14, 8, 1, {{P::courseBlocks, 0, 4, 14}, {P::courseCount, 0, 4, 14}, {P::courseBlocks, 2, 4, 14}, {P::courseCount, 2, 4, 14}}),
          control("overlap", "Course overlap", 0.05, 0.55, 0.3125, 0.01, {{P::courseOverlap, 0, 0.05, 0.55}, {P::courseOverlap, 2, 0.05, 0.55}}),
          control("crookedness", "Crookedness", 0, 0.18, 0.06, 0.005, {{P::courseCrookedness, 0, 0, 0.18}, {P::courseCrookedness, 2, 0, 0.18}}),
          control("variation", "Slate variation", 0, 0.7, 0.3, 0.01, {{P::courseBlockVariation, 0, 0, 0.7}, {P::courseBlockVariation, 2, 0, 0.7}})}},
        {"castle-stone", "Castle Stone", "castle-stone", "castle_stone.bmp",
         "Weathered variable-course castle masonry.",
         {control("block-count", "Block count", 3, 11, 6, 1, {{P::courseBlocks, 0, 3, 11}}),
          control("course-count", "Course count", 4, 13, 8, 1, {{P::courseCount, 0, 4, 13}}),
          control("unevenness", "Unevenness", 0, 100, 56, 1, {{P::courseBlockVariation, 0, 0.3, 1.0}, {P::courseCrookedness, 0, 0, 0.25}}),
          control("joint-width", "Joint width", 0.025, 0.17, 0.09, 0.005, {{P::courseGap, 0, 0.025, 0.17}})}},
        {"castle-window", "Castle Window", "castle-window", "castle_window.bmp",
         "Diamond lead lattice set into a recessed frame.",
         {control("lead-width", "Lead width", 0.025, 0.16, 0.085, 0.005, {{P::latticeWidth, 0, 0.025, 0.16}}),
          control("frame-width", "Frame width", 0.02, 0.12, 0.055, 0.005, {{P::shapeBorderWidth, 1, 0.02, 0.12}}),
          control("relief", "Surface relief", 0, 8, 2.2, 0.1, {{P::normalStrength, 0, 0, 8}})}},
        {"cel-castle-stone", "Cel Castle Stone", "cel-castle-stone", "cel_castle_stone.bmp",
         "Graphic rounded masonry with restrained surface detail.",
         {control("block-count", "Block count", 3, 11, 6, 1, {{P::courseBlocks, 0, 3, 11}}),
          control("course-count", "Course count", 4, 12, 7, 1, {{P::courseCount, 0, 4, 12}}),
          control("character", "Hand-cut character", 0, 100, 60, 1, {{P::courseBlockVariation, 0, 0.3, 0.9}, {P::courseCrookedness, 0, 0, 0.2}}),
          control("joint-width", "Joint width", 0.03, 0.18, 0.1, 0.005, {{P::courseGap, 0, 0.03, 0.18}})}},
        {"cel-courtyard-gravel", "Cel Courtyard Gravel", "cel-courtyard-gravel", "cel_courtyard_gravel.bmp",
         "Several deterministic populations of faceted gravel.",
         {control("density", "Stone density", 0.35, 1, 0.92, 0.01, {{P::scatterDensity, 0, 0.35, 1}}),
          control("pebble-size", "Pebble size", 0.055, 0.15, 0.105, 0.005, {{P::scatterStampSize, 0, 0.055, 0.15}}),
          control("spacing", "Minimum spacing", 0, 0.035, 0.008, 0.001, {{P::scatterMinimumDistance, 0, 0, 0.035}}),
          control("facets", "Facet strength", 0, 0.65, 0.28, 0.01, {{P::regionFacetStrength, 1, 0, 0.65}})}},
        {"cel-forest-bark", "Cel Forest Bark", "cel-forest-bark", "cel_forest_bark.png",
         "Hand-cut bark plates, branching cracks and lichen.",
         {control("plate-columns", "Plate columns", 6, 20, 12, 1, {{P::organicCellColumns, 0, 6, 20}}),
          control("plate-length", "Plate length", 0, 100, 50, 1, {{P::organicCellRows, 0, 8, 2}, {P::organicCellAnisotropy, 0, 1.8, 4.6}}),
          control("crack-width", "Crack width", 0.006, 0.035, 0.016, 0.001, {{P::organicCrackWidth, 3, 0.006, 0.035}}),
          control("lichen", "Lichen coverage", 0.05, 0.85, 0.42, 0.01, {{P::organicAccumulationCoverage, 4, 0.05, 0.85}})}},
        {"cel-forest-crate", "Cel Forest Crate", "detailed-crate", "cel_forest_crate.png",
         "Illustrated timber planks with grain, bevels and fastener details.",
         {control("plank-gap", "Plank gap", 0.02, 0.1, 0.055, 0.005, {{P::shapeBorderWidth, 0, 0.02, 0.1}}),
          control("grain-detail", "Grain detail", 0.2, 1, 0.7, 0.01, {{P::surfaceDetail, 3, 0.2, 1}, {P::surfaceDetail, 4, 0.28, 1}}),
          control("grain-wander", "Grain wander", 0.1, 0.9, 0.5, 0.01, {{P::surfaceDistortion, 3, 0.1, 0.9}, {P::surfaceDistortion, 4, 0.08, 0.88}}),
          control("wear", "Edge wear", 0, 0.25, 0.04, 0.01, {{P::regionWear, 2, 0, 0.25}})}},
        {"cel-forest-rock", "Cel Forest Rock", "cel-forest-rock", "cel_forest_rock.png",
         "Large deliberately faceted stones with deep green joints.",
         {control("columns", "Stone columns", 3, 10, 6, 1, {{P::worleyColumns, 0, 3, 10}}),
          control("rows", "Stone rows", 3, 10, 6, 1, {{P::worleyRows, 0, 3, 10}}),
          control("rounding", "Edge rounding", 0.18, 0.75, 0.5, 0.01, {{P::regionBevelWidth, 1, 0.18, 0.75}}),
          control("facets", "Facet strength", 0.08, 0.75, 0.42, 0.01, {{P::regionFacetStrength, 1, 0.08, 0.75}})}},
    };
    return templates;
}

const ReferenceMaterialTemplate* findReferenceMaterialTemplate(
    std::string_view identifier)
{
    const auto& templates = referenceMaterialTemplates();
    const auto found = std::find_if(
        templates.begin(),
        templates.end(),
        [identifier](const auto& candidate) {
            return candidate.identifier == identifier;
        });
    return found == templates.end() ? nullptr : &*found;
}

std::optional<std::string> applyTemplateControl(
    Material& material,
    const TemplateControl& control,
    double value)
{
    if (!std::isfinite(value) || control.minimumValue > control.maximumValue ||
        value < control.minimumValue || value > control.maximumValue) {
        return "template control value is outside its supported range";
    }
    const double extent = control.maximumValue - control.minimumValue;
    const double normalised = extent > 0.0
        ? (value - control.minimumValue) / extent
        : 0.0;
    Material candidate = material;
    for (const auto& binding : control.bindings) {
        const double mapped = binding.outputMinimum +
            (binding.outputMaximum - binding.outputMinimum) * normalised;
        if (const auto error = applyBinding(candidate, binding, mapped)) {
            return error;
        }
    }
    if (const auto error = validateMaterial(candidate)) {
        return *error;
    }
    material = std::move(candidate);
    return std::nullopt;
}

} // namespace paperweight
