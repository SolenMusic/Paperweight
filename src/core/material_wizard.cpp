#include <paperweight/material_wizard.hpp>

#include <paperweight/hash.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace paperweight {
namespace {

constexpr std::array<WizardTemplateOption, 22> templateOptions{{
    {WizardMaterialFamily::masonry, "castle-flagstone"},
    {WizardMaterialFamily::masonry, "castle-stone"},
    {WizardMaterialFamily::masonry, "cel-castle-stone"},
    {WizardMaterialFamily::masonry, "castle-roof"},
    {WizardMaterialFamily::stone, "cel-forest-rock"},
    {WizardMaterialFamily::stone, "castle-flagstone"},
    {WizardMaterialFamily::stone, "graphic-marble"},
    {WizardMaterialFamily::stone, "mossy-pebbles"},
    {WizardMaterialFamily::wood, "cel-forest-crate"},
    {WizardMaterialFamily::wood, "knotty-wood"},
    {WizardMaterialFamily::wood, "cel-forest-bark"},
    {WizardMaterialFamily::metal, "painted-metal"},
    {WizardMaterialFamily::metal, "weathered-metal"},
    {WizardMaterialFamily::organic, "cel-forest-bark"},
    {WizardMaterialFamily::organic, "castle-foliage"},
    {WizardMaterialFamily::foliage, "castle-foliage"},
    {WizardMaterialFamily::gravelDebris, "cel-courtyard-gravel"},
    {WizardMaterialFamily::gravelDebris, "scattered-debris"},
    {WizardMaterialFamily::gravelDebris, "mossy-pebbles"},
    {WizardMaterialFamily::abstract, "graphic-marble"},
    {WizardMaterialFamily::abstract, "ember"},
    {WizardMaterialFamily::abstract, "castle-window"},
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
        {"painted-metal", "Painted Metal", "painted-metal", "",
         "Brushed painted metal with controlled smoothing and a graphic palette.",
         {wizardControl("brush-detail", "Brush detail", 0, 100, 73, 1,
              {{P::surfaceDetail, 0, 0.4, 1.0}}),
          wizardControl("brush-wander", "Brush wander", 0, 100, 31, 1,
              {{P::surfaceDistortion, 0, 0.05, 0.8}}),
          wizardControl("finish-smoothing", "Finish smoothing", 0, 100, 68, 1,
              {{P::layerOpacity, 1, 0, 1}}),
          wizardControl("surface-relief", "Surface relief", 0, 5, 1.4, 0.1,
              {{P::normalStrength, 0, 0, 5}})}},
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
          wizardControl("surface-relief", "Surface relief", 0, 8, 3.2, 0.1,
              {{P::normalStrength, 0, 0, 8}})}},
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
          wizardControl("surface-relief", "Surface relief", 0, 5, 1.5, 0.1,
              {{P::normalStrength, 0, 0, 5}})}},
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
