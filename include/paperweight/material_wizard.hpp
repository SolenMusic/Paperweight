#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <paperweight/material_template.hpp>

namespace paperweight {

enum class WizardMaterialFamily : std::uint8_t {
    masonry,
    stone,
    wood,
    metal,
    organic,
    foliage,
    gravelDebris,
    abstract,
};

struct WizardFamilyDescriptor {
    WizardMaterialFamily family;
    std::string_view key;
    std::string_view displayName;
    std::string_view description;
};

inline constexpr std::array<WizardFamilyDescriptor, 8> wizardMaterialFamilies{{
    {WizardMaterialFamily::masonry, "masonry", "Masonry", "Constructed walls, floors, blocks, and courses."},
    {WizardMaterialFamily::stone, "stone", "Stone", "Natural, cut, rounded, or deliberately faceted stone."},
    {WizardMaterialFamily::wood, "wood", "Wood", "Planks, grain, bark-like surfaces, and timber wear."},
    {WizardMaterialFamily::metal, "metal", "Metal", "Constructed metallic surfaces and repeated details."},
    {WizardMaterialFamily::organic, "organic", "Organic", "Irregular living or weather-grown structures."},
    {WizardMaterialFamily::foliage, "foliage", "Foliage", "Overlapping leaves and clustered plant cover."},
    {WizardMaterialFamily::gravelDebris, "gravel-debris", "Gravel or Debris", "Scattered stones, chips, and ground cover."},
    {WizardMaterialFamily::abstract, "abstract", "Abstract", "Graphic patterns and stylised repeating structures."},
}};

struct WizardTemplateOption {
    WizardMaterialFamily family;
    std::string_view templateIdentifier;
};

[[nodiscard]] std::span<const WizardTemplateOption> wizardTemplateOptions();
[[nodiscard]] std::vector<const ReferenceMaterialTemplate*> wizardTemplatesForFamily(
    WizardMaterialFamily family);

enum class WizardControlSection : std::uint8_t {
    construction,
    surface,
    wear,
};

struct WizardControlState {
    std::size_t templateControlIndex{};
    WizardControlSection section{WizardControlSection::surface};
    double value{};
    bool locked{};

    friend bool operator==(const WizardControlState&, const WizardControlState&) = default;
};

struct MaterialWizardSession {
    std::string templateIdentifier;
    MaterialRecipe recipe;
    std::uint64_t baseSeed{18431};
    PhysicalSize physicalSize;
    Rgba8 lowColour;
    Rgba8 highColour;
    bool seedLocked{};
    bool physicalSizeLocked{true};
    bool coloursLocked{true};
    std::vector<WizardControlState> controls;

    friend bool operator==(const MaterialWizardSession&, const MaterialWizardSession&) = default;
};

struct MaterialWizardAlternative {
    std::uint64_t stableIdentifier{};
    std::vector<double> controlValues;
    Material material;

    friend bool operator==(const MaterialWizardAlternative&, const MaterialWizardAlternative&) = default;
};

struct MaterialWizardError {
    std::string message;

    friend bool operator==(const MaterialWizardError&, const MaterialWizardError&) = default;
};

using MaterialWizardSessionResult = std::variant<MaterialWizardSession, MaterialWizardError>;
using MaterialWizardMaterialResult = std::variant<Material, MaterialWizardError>;
using MaterialWizardAlternativesResult =
    std::variant<std::vector<MaterialWizardAlternative>, MaterialWizardError>;

[[nodiscard]] MaterialWizardSessionResult makeMaterialWizardSession(
    const ReferenceMaterialTemplate& descriptor,
    const MaterialRecipe& recipe,
    std::uint64_t seed);

[[nodiscard]] MaterialWizardMaterialResult makeMaterialFromWizard(
    const MaterialWizardSession& session,
    const ReferenceMaterialTemplate& descriptor);

[[nodiscard]] MaterialWizardAlternativesResult generateMaterialWizardAlternatives(
    const MaterialWizardSession& session,
    const ReferenceMaterialTemplate& descriptor,
    std::size_t count);

} // namespace paperweight
