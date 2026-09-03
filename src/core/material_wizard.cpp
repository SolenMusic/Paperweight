#include <paperweight/material_wizard.hpp>

#include <paperweight/hash.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace paperweight {
namespace {

constexpr std::array<WizardTemplateOption, 37> templateOptions{{
    {WizardMaterialFamily::masonry, "castle-flagstone"},
    {WizardMaterialFamily::masonry, "castle-stone"},
    {WizardMaterialFamily::masonry, "cel-castle-stone"},
    {WizardMaterialFamily::masonry, "castle-roof"},
    {WizardMaterialFamily::masonry, "wet-mortar"},
    {WizardMaterialFamily::stone, "cel-forest-rock"},
    {WizardMaterialFamily::stone, "castle-flagstone"},
    {WizardMaterialFamily::stone, "graphic-marble"},
    {WizardMaterialFamily::stone, "mossy-pebbles"},
    {WizardMaterialFamily::stone, "polished-marble"},
    {WizardMaterialFamily::stone, "glazed-ceramic"},
    {WizardMaterialFamily::stone, "wet-stone"},
    {WizardMaterialFamily::wood, "cel-forest-crate"},
    {WizardMaterialFamily::wood, "knotty-wood"},
    {WizardMaterialFamily::wood, "cel-forest-bark"},
    {WizardMaterialFamily::wood, "varnished-wood"},
    {WizardMaterialFamily::wood, "lacquered-wood"},
    {WizardMaterialFamily::metal, "painted-metal"},
    {WizardMaterialFamily::metal, "weathered-metal"},
    {WizardMaterialFamily::metal, "engraved-metal"},
    {WizardMaterialFamily::metal, "chrome"},
    {WizardMaterialFamily::metal, "steel"},
    {WizardMaterialFamily::metal, "copper"},
    {WizardMaterialFamily::metal, "brass"},
    {WizardMaterialFamily::metal, "painted-steel"},
    {WizardMaterialFamily::metal, "corroded-metal"},
    {WizardMaterialFamily::metal, "machinery-panels"},
    {WizardMaterialFamily::organic, "cel-forest-bark"},
    {WizardMaterialFamily::organic, "castle-foliage"},
    {WizardMaterialFamily::foliage, "castle-foliage"},
    {WizardMaterialFamily::gravelDebris, "cel-courtyard-gravel"},
    {WizardMaterialFamily::gravelDebris, "scattered-debris"},
    {WizardMaterialFamily::gravelDebris, "mossy-pebbles"},
    {WizardMaterialFamily::abstract, "graphic-marble"},
    {WizardMaterialFamily::abstract, "ember"},
    {WizardMaterialFamily::abstract, "castle-window"},
    {WizardMaterialFamily::abstract, "illuminated-scifi"},
}};

TemplateControl wizardControl(
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

const std::vector<ReferenceMaterialTemplate>& additionalWizardTemplates()
{
    using P = TemplateProperty;
    static const std::vector<ReferenceMaterialTemplate> templates{
        {"chrome", "Polished Chrome", "chrome", "",
         "Mirror-bright chrome whose polishing marks become visible in the studio reflection.",
         {wizardControl("polish", "Polish", 0, 100, 88, 1,
              {{P::roughnessLow, 0, 0.20, 0.025}, {P::roughnessHigh, 0, 0.30, 0.055}}),
          wizardControl("polishing-marks", "Polishing marks", 0, 100, 35, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("surface-relief", "Relief depth (mm)", 0, 0.5, 0.08, 0.01,
              {{P::reliefDepthMetres, 0, 0, 0.0005}})}},
        {"steel", "Brushed Steel", "steel", "",
         "Cool steel with long machining marks and an adjustable satin finish.",
         {wizardControl("polish", "Finish polish", 0, 100, 62, 1,
              {{P::roughnessLow, 0, 0.48, 0.10}, {P::roughnessHigh, 0, 0.64, 0.22}}),
          wizardControl("brushing", "Brushing detail", 0, 100, 78, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("surface-relief", "Relief depth (mm)", 0, 1, 0.18, 0.02,
              {{P::reliefDepthMetres, 0, 0, 0.001}})}},
        {"copper", "Polished Copper", "copper", "",
         "Warm conductive copper with rolled-metal grain and coloured reflections.",
         {wizardControl("polish", "Finish polish", 0, 100, 72, 1,
              {{P::roughnessLow, 0, 0.42, 0.08}, {P::roughnessHigh, 0, 0.56, 0.18}}),
          wizardControl("grain", "Rolled grain", 0, 100, 48, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("surface-relief", "Relief depth (mm)", 0, 1, 0.12, 0.02,
              {{P::reliefDepthMetres, 0, 0, 0.001}})}},
        {"brass", "Polished Brass", "brass", "",
         "Golden brass with fine finishing marks and a controllable polish.",
         {wizardControl("polish", "Finish polish", 0, 100, 76, 1,
              {{P::roughnessLow, 0, 0.40, 0.07}, {P::roughnessHigh, 0, 0.52, 0.16}}),
          wizardControl("grain", "Finishing grain", 0, 100, 50, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("surface-relief", "Relief depth (mm)", 0, 1, 0.10, 0.02,
              {{P::reliefDepthMetres, 0, 0, 0.001}})}},
        {"painted-steel", "Painted Steel", "painted-steel", "",
         "Industrial paint with scratches that reveal reflective bare steel underneath.",
         {wizardControl("paint-wear", "Paint wear", 0, 100, 54, 1,
              {{P::surfaceDetail, 0, 0.25, 1.0}}),
          wizardControl("bare-metal", "Bare-metal response", 0, 100, 86, 1,
              {{P::metalnessHigh, 0, 0.45, 1.0}}),
          wizardControl("coating", "Coating reflectivity", 1.3, 1.8, 1.52, 0.01,
              {{P::dielectricIor, 0, 1.3, 1.8}}),
          wizardControl("scratch-depth", "Scratch depth (mm)", 0, 2, 0.45, 0.05,
              {{P::reliefDepthMetres, 0, 0, 0.002}})}},
        {"corroded-metal", "Corroded Metal", "corroded-metal", "",
         "Rough oxidation broken by surviving islands of reflective steel.",
         {wizardControl("corrosion", "Corrosion detail", 0, 100, 78, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("surviving-metal", "Surviving metal", 0, 100, 82, 1,
              {{P::metalnessHigh, 0, 0.25, 1.0}}),
          wizardControl("pitting-depth", "Pitting depth (mm)", 0, 6, 2.2, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.006}})}},
        {"glazed-ceramic", "Glazed Ceramic", "glazed-ceramic", "",
         "Hand-glazed tiles with deep clear coat and occluded grout.",
         {wizardControl("glaze", "Glaze coverage", 0, 100, 92, 1,
              {{P::clearCoatHigh, 0, 0.25, 1.0}, {P::coatingHigh, 0, 0.45, 1.0}}),
          wizardControl("glaze-polish", "Glaze polish", 0, 100, 88, 1,
              {{P::clearCoatRoughnessHigh, 0, 0.28, 0.025}}),
          wizardControl("grout-depth", "Grout depth (mm)", 0, 10, 4, 0.25,
              {{P::reliefDepthMetres, 0, 0, 0.01}}),
          wizardControl("surface-detail", "Glaze variation", 0, 0.3, 0.13, 0.01,
              {{P::layerOpacity, 1, 0, 0.3}})}},
        {"lacquered-wood", "Lacquered Wood", "lacquered-wood", "",
         "Flowing warm timber beneath a polished lacquer coating.",
         {wizardControl("lacquer", "Lacquer depth", 0, 100, 90, 1,
              {{P::clearCoatHigh, 0, 0.25, 1.0}, {P::coatingLow, 0, 0.35, 0.98}}),
          wizardControl("lacquer-polish", "Lacquer polish", 0, 100, 88, 1,
              {{P::clearCoatRoughnessHigh, 0, 0.3, 0.025}}),
          wizardControl("grain-detail", "Grain detail", 0, 100, 78, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("relief-depth", "Grain depth (mm)", 0, 4, 1, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.004}})}},
        {"wet-stone", "Wet Stone", "wet-stone", "",
         "Hand-cut dark stone with water pooled in joints and cavities.",
         {wizardControl("wetness", "Puddle wetness", 0, 100, 90, 1,
              {{P::clearCoatLow, 0, 0.05, 1.0}, {P::coatingLow, 0, 0.05, 1.0}}),
          wizardControl("water-polish", "Water smoothness", 0, 100, 92, 1,
              {{P::clearCoatRoughnessLow, 0, 0.3, 0.015}}),
          wizardControl("stone-rounding", "Stone rounding", 0.18, 0.7, 0.46, 0.01,
              {{P::regionBevelWidth, 1, 0.18, 0.7}}),
          wizardControl("relief-depth", "Stone depth (mm)", 2, 40, 22, 1,
              {{P::reliefDepthMetres, 0, 0.002, 0.04}})}},
        {"machinery-panels", "Machinery Panels", "machinery-panels", "",
         "Painted industrial panels with exposed directionally brushed steel.",
         {wizardControl("paint-wear", "Paint wear", 0, 100, 28, 1,
              {{P::layerOpacity, 1, 0, 0.5}, {P::coatingHigh, 0, 1.0, 0.35}}),
          wizardControl("bare-metal", "Exposed metal", 0, 100, 86, 1,
              {{P::metalnessLow, 0, 0.35, 1.0}}),
          wizardControl("brushing", "Brush anisotropy", 0, 100, 86, 1,
              {{P::anisotropyStrength, 0, 0, 1}}),
          wizardControl("brush-angle", "Brush angle", 0, 180, 90, 1,
              {{P::anisotropyRotationDegrees, 0, 0, 180}})}},
        {"illuminated-scifi", "Illuminated Science-Fiction Surface", "illuminated-scifi", "",
         "Dark plated hull with cyan emissive energy conduits.",
         {wizardControl("emission", "Emission", 0, 100, 100, 1,
              {{P::emissiveIntensity, 0, 0, 1}}),
          wizardControl("conduit-width", "Conduit width", 0.015, 0.12, 0.055, 0.005,
              {{P::latticeWidth, 1, 0.015, 0.12}}),
          wizardControl("coating", "Panel clear coat", 0, 100, 44, 1,
              {{P::clearCoatHigh, 0, 0, 0.8}}),
          wizardControl("relief-depth", "Panel depth (mm)", 0, 12, 5, 0.25,
              {{P::reliefDepthMetres, 0, 0, 0.012}})}},
        {"polished-marble", "Polished Marble", "polished-marble", "",
         "Flowing mineral veins with restrained physical relief and independently polished roughness.",
         {wizardControl("vein-detail", "Vein detail", 0, 100, 70, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("vein-flow", "Vein flow", 0, 100, 88, 1,
              {{P::surfaceDistortion, 0, 0.15, 0.95}}),
          wizardControl("polish", "Polish", 0, 100, 86, 1,
              {{P::surfaceValue, 3, 0.55, 0.06}}),
          wizardControl("relief-depth", "Relief depth (mm)", 0, 4, 1.5, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.004}})}},
        {"wet-mortar", "Wet Mortar", "wet-mortar", "",
         "Damp masonry with glossy mortar and separately roughened stone faces.",
         {wizardControl("wetness", "Mortar wetness", 0, 100, 84, 1,
              {{P::surfaceValue, 3, 0.65, 0.08}}),
          wizardControl("stone-roughness", "Stone roughness", 0, 100, 64, 1,
              {{P::layerOpacity, 4, 0.1, 0.9}}),
          wizardControl("relief-depth", "Relief depth (mm)", 1, 24, 12, 0.5,
              {{P::reliefDepthMetres, 0, 0.001, 0.024}})}},
        {"engraved-metal", "Engraved Metal", "engraved-metal", "",
         "Brushed metal with a seamless cut lattice and independent reflective response.",
         {wizardControl("brushing", "Brushing", 0, 100, 60, 1,
              {{P::layerOpacity, 2, 0, 0.22}, {P::layerOpacity, 4, 0, 0.16}}),
          wizardControl("polish", "Polish", 0, 100, 78, 1,
              {{P::surfaceValue, 3, 0.65, 0.08}}),
          wizardControl("engraving-depth", "Engraving depth (mm)", 0, 6, 2, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.006}})}},
        {"varnished-wood", "Varnished Wood", "varnished-wood", "",
         "Warm flowing grain beneath a polished varnish layer.",
         {wizardControl("grain-detail", "Grain detail", 0, 100, 70, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("grain-flow", "Grain flow", 0, 100, 58, 1,
              {{P::surfaceDistortion, 0, 0.1, 0.9}}),
          wizardControl("varnish", "Varnish gloss", 0, 100, 82, 1,
              {{P::surfaceValue, 3, 0.7, 0.07}}),
          wizardControl("relief-depth", "Relief depth (mm)", 0, 4, 1.2, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.004}})}},
        {"painted-metal", "Painted Metal", "painted-metal", "",
         "Brushed painted metal with controlled smoothing and a graphic palette.",
         {wizardControl("brush-detail", "Brush detail", 0, 100, 73, 1,
              {{P::surfaceDetail, 0, 0.4, 1.0}}),
          wizardControl("brush-wander", "Brush wander", 0, 100, 31, 1,
              {{P::surfaceDistortion, 0, 0.05, 0.8}}),
          wizardControl("finish-smoothing", "Finish smoothing", 0, 100, 68, 1,
              {{P::layerOpacity, 1, 0, 1}}),
          wizardControl("surface-relief", "Relief depth (mm)", 0, 5, 1.4, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.005}})}},
        {"weathered-metal", "Weathered Metal", "weathered-metal", "",
         "Scratched and pitted metal with independently adjustable rust and dirt.",
         {wizardControl("scratch-detail", "Scratch detail", 0, 100, 83, 1,
              {{P::surfaceDetail, 0, 0.4, 1.0}}),
          wizardControl("pitting", "Pitting", 0, 100, 36, 1,
              {{P::layerOpacity, 2, 0, 0.55}}),
          wizardControl("rust-coverage", "Rust coverage", 0, 100, 69, 1,
              {{P::layerOpacity, 3, 0, 0.9}}),
          wizardControl("dirt-coverage", "Dirt coverage", 0, 100, 32, 1,
              {{P::layerOpacity, 6, 0, 0.5}})}},
        {"knotty-wood", "Knotty Wood", "knotty-wood", "",
         "Flowing timber grain with pores, warm heartwood, and broken knots.",
         {wizardControl("grain-detail", "Grain detail", 0, 100, 55, 1,
              {{P::surfaceDetail, 1, 0.2, 1.0}}),
          wizardControl("grain-wander", "Grain wander", 0, 100, 45, 1,
              {{P::surfaceDistortion, 1, 0.1, 0.9}}),
          wizardControl("open-pores", "Open pores", 0, 100, 34, 1,
              {{P::layerOpacity, 3, 0, 0.35}}),
          wizardControl("knots", "Knots", 0, 100, 33, 1,
              {{P::layerOpacity, 6, 0, 0.4}})}},
        {"graphic-marble", "Graphic Marble", "graphic-marble", "",
         "Flowing marble veins with restrained relief and stylised mineral bands.",
         {wizardControl("vein-detail", "Vein detail", 0, 100, 65, 1,
              {{P::surfaceDetail, 0, 0.2, 1.0}}),
          wizardControl("vein-flow", "Vein flow", 0, 100, 89, 1,
              {{P::surfaceDistortion, 0, 0.1, 0.95}}),
          wizardControl("graphic-banding", "Graphic banding", 0, 100, 78, 1,
              {{P::layerOpacity, 1, 0, 1}}),
          wizardControl("outlines", "Vein outlines", 0, 100, 72, 1,
              {{P::layerOpacity, 2, 0, 1}})}},
        {"scattered-debris", "Scattered Debris", "scattered-debris", "",
         "A tileable population of chips with stable spacing and varied shapes.",
         {wizardControl("debris-density", "Debris density", 0, 100, 48, 1,
              {{P::scatterDensity, 0, 0.2, 1.0}}),
          wizardControl("debris-spacing", "Debris spacing", 0, 0.04, 0.014, 0.001,
              {{P::scatterMinimumDistance, 0, 0, 0.04}}),
          wizardControl("surface-relief", "Relief depth (mm)", 0, 8, 3.2, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.008}})}},
        {"mossy-pebbles", "Mossy Pebbles", "mossy-pebbles", "",
         "Rounded pebbles with mineral variation, deep joints, and damp moss.",
         {wizardControl("stone-columns", "Stone columns", 3, 14, 8, 1,
              {{P::worleyColumns, 0, 3, 14}}),
          wizardControl("stone-rows", "Stone rows", 3, 14, 7, 1,
              {{P::worleyRows, 0, 3, 14}}),
          wizardControl("joint-width", "Joint width", 0.1, 0.5, 0.29, 0.01,
              {{P::worleyEdgeWidth, 0, 0.1, 0.5}}),
          wizardControl("moss-coverage", "Moss coverage", 0, 100, 50, 1,
              {{P::layerOpacity, 5, 0, 1}})}},
        {"ember", "Ember", "ember", "",
         "An abstract flowing field of dark violet and hot orange material.",
         {wizardControl("hot-coverage", "Hot coverage", 0, 100, 31, 1,
              {{P::layerOpacity, 1, 0, 0.8}}),
          wizardControl("flame-contrast", "Flame contrast", 0, 100, 100, 1,
              {{P::layerOpacity, 2, 0, 1}}),
          wizardControl("surface-relief", "Relief depth (mm)", 0, 5, 1.5, 0.1,
              {{P::reliefDepthMetres, 0, 0, 0.005}})}},
    };
    return templates;
}

WizardControlSection sectionForControl(const TemplateControl& control)
{
    const auto key = control.key;
    if (key.find("wear") != std::string_view::npos ||
        key.find("moss") != std::string_view::npos ||
        key.find("lichen") != std::string_view::npos ||
        key.find("scratch") != std::string_view::npos ||
        key.find("pitting") != std::string_view::npos ||
        key.find("rust") != std::string_view::npos ||
        key.find("dirt") != std::string_view::npos ||
        key.find("knots") != std::string_view::npos ||
        key.find("crooked") != std::string_view::npos ||
        key.find("uneven") != std::string_view::npos ||
        key.find("character") != std::string_view::npos) {
        return WizardControlSection::wear;
    }
    if (key.find("count") != std::string_view::npos ||
        key.find("columns") != std::string_view::npos ||
        key.find("rows") != std::string_view::npos ||
        key.find("density") != std::string_view::npos ||
        key.find("size") != std::string_view::npos ||
        key.find("width") != std::string_view::npos ||
        key.find("spacing") != std::string_view::npos ||
        key.find("overlap") != std::string_view::npos ||
        key.find("gap") != std::string_view::npos ||
        key.find("spread") != std::string_view::npos ||
        key.find("length") != std::string_view::npos) {
        return WizardControlSection::construction;
    }
    return WizardControlSection::surface;
}

double unitValue(std::uint64_t hash)
{
    constexpr double divisor = static_cast<double>(std::uint64_t{1} << 53);
    return static_cast<double>(hash >> 11) / divisor;
}

double steppedValue(const TemplateControl& control, double value)
{
    const double clamped = std::clamp(value, control.minimumValue, control.maximumValue);
    if (control.step <= 0.0) {
        return clamped;
    }
    const double steps = std::round((clamped - control.minimumValue) / control.step);
    return std::clamp(
        control.minimumValue + steps * control.step,
        control.minimumValue,
        control.maximumValue);
}

std::uint8_t adjustedChannel(std::uint8_t channel, double brightness, double warmth)
{
    return static_cast<std::uint8_t>(std::clamp(
        std::llround(static_cast<double>(channel) + brightness + warmth),
        0LL,
        255LL));
}

Rgba8 adjustedColour(Rgba8 colour, double brightness, double warmth)
{
    colour.red = adjustedChannel(colour.red, brightness, warmth);
    colour.green = adjustedChannel(colour.green, brightness, 0.0);
    colour.blue = adjustedChannel(colour.blue, brightness, -warmth);
    return colour;
}

std::optional<std::string> validateSession(
    const MaterialWizardSession& session,
    const ReferenceMaterialTemplate& descriptor)
{
    if (session.templateIdentifier != descriptor.identifier) {
        return "wizard session and template identifier do not match";
    }
    if (session.controls.size() != descriptor.controls.size()) {
        return "wizard session does not contain every template control";
    }
    if (!std::isfinite(session.physicalSize.widthMetres) ||
        !std::isfinite(session.physicalSize.heightMetres) ||
        session.physicalSize.widthMetres < PhysicalLimits::minimumMetres ||
        session.physicalSize.widthMetres > PhysicalLimits::maximumMetres ||
        session.physicalSize.heightMetres < PhysicalLimits::minimumMetres ||
        session.physicalSize.heightMetres > PhysicalLimits::maximumMetres) {
        return "wizard physical width and height are outside the supported metre range";
    }
    for (std::size_t index = 0; index < session.controls.size(); ++index) {
        const auto& state = session.controls[index];
        if (state.templateControlIndex != index) {
            return "wizard control ordering does not match the selected template";
        }
        const auto& control = descriptor.controls[index];
        if (!std::isfinite(state.value) || state.value < control.minimumValue ||
            state.value > control.maximumValue) {
            return "wizard control value is outside its declared range";
        }
    }
    return std::nullopt;
}

} // namespace

std::span<const WizardTemplateOption> wizardTemplateOptions()
{
    return templateOptions;
}

const ReferenceMaterialTemplate* findWizardMaterialTemplate(
    std::string_view identifier)
{
    if (const auto* descriptor = findReferenceMaterialTemplate(identifier)) {
        return descriptor;
    }
    const auto& templates = additionalWizardTemplates();
    const auto found = std::find_if(
        templates.begin(),
        templates.end(),
        [identifier](const auto& candidate) {
            return candidate.identifier == identifier;
        });
    return found == templates.end() ? nullptr : &*found;
}

std::vector<const ReferenceMaterialTemplate*> wizardTemplatesForFamily(
    WizardMaterialFamily family)
{
    std::vector<const ReferenceMaterialTemplate*> result;
    for (const auto& option : templateOptions) {
        if (option.family != family) {
            continue;
        }
        if (const auto* descriptor = findWizardMaterialTemplate(option.templateIdentifier)) {
            result.push_back(descriptor);
        }
    }
    return result;
}

MaterialWizardSessionResult makeMaterialWizardSession(
    const ReferenceMaterialTemplate& descriptor,
    const MaterialRecipe& recipe,
    std::uint64_t seed)
{
    auto material = instantiateMaterial(recipe, seed);
    if (const auto error = validateMaterial(material)) {
        return MaterialWizardError{"wizard recipe is not a valid material: " + *error};
    }
    MaterialWizardSession session;
    session.templateIdentifier = descriptor.identifier;
    session.recipe = recipe;
    session.baseSeed = seed;
    session.physicalSize = recipe.physicalSize;
    session.lowColour = recipe.lowColour;
    session.highColour = recipe.highColour;
    session.controls.reserve(descriptor.controls.size());
    for (std::size_t index = 0; index < descriptor.controls.size(); ++index) {
        const auto& control = descriptor.controls[index];
        session.controls.push_back({index, sectionForControl(control), control.defaultValue, false});
    }
    return session;
}

MaterialWizardMaterialResult makeMaterialFromWizard(
    const MaterialWizardSession& session,
    const ReferenceMaterialTemplate& descriptor)
{
    if (const auto error = validateSession(session, descriptor)) {
        return MaterialWizardError{*error};
    }
    auto material = instantiateMaterial(session.recipe, session.baseSeed);
    material.physicalSize = session.physicalSize;
    material.lowColour = session.lowColour;
    material.highColour = session.highColour;
    for (std::size_t index = 0; index < session.controls.size(); ++index) {
        if (const auto error = applyTemplateControl(
                material, descriptor.controls[index], session.controls[index].value)) {
            return MaterialWizardError{*error};
        }
    }
    material.metadata.reset();
    if (const auto error = validateMaterial(material)) {
        return MaterialWizardError{"wizard produced an invalid material: " + *error};
    }
    return material;
}

MaterialWizardAlternativesResult generateMaterialWizardAlternatives(
    const MaterialWizardSession& session,
    const ReferenceMaterialTemplate& descriptor,
    std::size_t count)
{
    if (count == 0 || count > 16) {
        return MaterialWizardError{"wizard alternative count must be between 1 and 16"};
    }
    if (const auto error = validateSession(session, descriptor)) {
        return MaterialWizardError{*error};
    }

    std::vector<MaterialWizardAlternative> result;
    result.reserve(count);
    for (std::size_t alternativeIndex = 0; alternativeIndex < count; ++alternativeIndex) {
        const auto stableIdentifier = mixBits(
            session.baseSeed ^ 0x706170657277697aULL ^
            static_cast<std::uint64_t>(alternativeIndex + 1));
        auto candidate = session;
        if (!session.seedLocked) {
            candidate.baseSeed = mixBits(stableIdentifier ^ 0x73656564ULL);
        }
        if (!session.physicalSizeLocked) {
            const double factor = 0.75 + 0.5 * unitValue(mixBits(stableIdentifier ^ 0x7363616c65ULL));
            candidate.physicalSize.widthMetres = std::clamp(
                candidate.physicalSize.widthMetres * factor,
                PhysicalLimits::minimumMetres,
                PhysicalLimits::maximumMetres);
            candidate.physicalSize.heightMetres = std::clamp(
                candidate.physicalSize.heightMetres * factor,
                PhysicalLimits::minimumMetres,
                PhysicalLimits::maximumMetres);
        }
        if (!session.coloursLocked) {
            const double brightness =
                (unitValue(mixBits(stableIdentifier ^ 0x627269676874ULL)) - 0.5) * 36.0;
            const double warmth =
                (unitValue(mixBits(stableIdentifier ^ 0x7761726d7468ULL)) - 0.5) * 28.0;
            candidate.lowColour = adjustedColour(session.lowColour, brightness, warmth);
            candidate.highColour = adjustedColour(session.highColour, brightness, warmth);
        }
        std::vector<double> controlValues;
        controlValues.reserve(candidate.controls.size());
        for (std::size_t controlIndex = 0; controlIndex < candidate.controls.size(); ++controlIndex) {
            auto& state = candidate.controls[controlIndex];
            const auto& control = descriptor.controls[controlIndex];
            if (!state.locked) {
                const double span = control.maximumValue - control.minimumValue;
                const double offset =
                    (unitValue(mixBits(stableIdentifier ^
                         (0x636f6e74726f6cULL + static_cast<std::uint64_t>(controlIndex)))) - 0.5) *
                    span * 0.5;
                state.value = steppedValue(control, state.value + offset);
            }
            controlValues.push_back(state.value);
        }
        auto material = makeMaterialFromWizard(candidate, descriptor);
        if (const auto* error = std::get_if<MaterialWizardError>(&material)) {
            return *error;
        }
        result.push_back({
            stableIdentifier,
            std::move(controlValues),
            std::get<Material>(std::move(material)),
        });
    }
    return result;
}

} // namespace paperweight
