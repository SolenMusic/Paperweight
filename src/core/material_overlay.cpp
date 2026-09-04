#include <paperweight/material_overlay.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <system_error>
#include <utility>

namespace paperweight {
namespace {

constexpr std::string_view overlayBanner = "# Paperweight reusable material overlay";
constexpr std::string_view versionPrefix = "paperweight.overlay.version = ";
constexpr std::string_view idPrefix = "overlay.id = ";
constexpr std::string_view namePrefix = "overlay.name = ";
constexpr std::string_view descriptionPrefix = "overlay.description = ";

std::pair<std::string_view, std::string_view> takeLine(std::string_view text)
{
    const auto newline = text.find('\n');
    if (newline == std::string_view::npos) {
        return {text, {}};
    }
    auto line = text.substr(0, newline);
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    return {line, text.substr(newline + 1)};
}

bool validText(std::string_view value, std::size_t maximum, bool identifier)
{
    if (value.empty() || value.size() > maximum ||
        std::isspace(static_cast<unsigned char>(value.front())) != 0 ||
        std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [identifier](char valueCharacter) {
        const auto character = static_cast<unsigned char>(valueCharacter);
        if (identifier) {
            return std::isalnum(character) != 0 || valueCharacter == '-' ||
                valueCharacter == '_' || valueCharacter == '.';
        }
        return character >= 0x20 && character != 0x7f && valueCharacter != '#' &&
            valueCharacter != '=';
    });
}

MaterialOverlay makeOrganicOverlay(bool lichen)
{
    MaterialOverlay overlay;
    overlay.identifier = lichen ? "polished-lichen" : "polished-moss";
    overlay.name = lichen ? "Polished Lichen" : "Polished Moss";
    overlay.description = lichen
        ? "Layered pale lichen colonies with fine speckled breakup."
        : "Lush moss colonies with cavity-like breakup and damp variation.";

    const std::string groupId = lichen ? "overlay.lichen" : "overlay.moss";
    overlay.fragment.groups.push_back(MaterialLayerGroup{
        groupId,
        {},
        overlay.name,
        true,
        lichen ? 0.82 : 0.9,
        CompositeMode::blend,
        {},
        LayerMask{
            true,
            false,
            lichen ? 823U : 811U,
            lichen ? 0.48 : 0.4,
            lichen ? 0.72 : 0.78,
        },
        LayerOutputRouting{
            true, true, true, false, true, true, false, false, false},
    });

    auto foundation = makeNoiseLayer();
    foundation.outputs = overlay.fragment.groups.front().outputs;
    std::get<NoiseOperation>(foundation.operation).seedOffset = lichen ? 59U : 41U;

    auto growth = makeOrganicAccumulationLayer();
    growth.outputs = overlay.fragment.groups.front().outputs;
    growth.opacity = lichen ? 0.86 : 0.92;
    auto& operation = std::get<OrganicAccumulationOperation>(growth.operation);
    operation.kind = lichen ? OrganicAccumulationKind::lichen
                            : OrganicAccumulationKind::moss;
    operation.source = OrganicAccumulationSource::lowHeight;
    operation.scale = lichen ? 12U : 9U;
    operation.coverage = lichen ? 0.44 : 0.6;
    operation.softness = lichen ? 0.035 : 0.055;
    operation.moistureBias = lichen ? 0.42 : 0.72;
    operation.breakup = lichen ? 0.62 : 0.38;
    operation.variation = lichen ? 0.52 : 0.4;
    operation.lowColour = lichen ? Rgba8{102, 123, 90, 255}
                                 : Rgba8{41, 70, 41, 255};
    operation.highColour = lichen ? Rgba8{199, 209, 156, 255}
                                  : Rgba8{146, 180, 92, 255};
    operation.seedOffset = lichen ? 419U : 401U;
    operation.target = ProcessingTarget::colourAndScalar;
    operation.profile = lichen ? OrganicAccumulationProfile::speckles
                               : OrganicAccumulationProfile::colonies;
    operation.field = OrganicAccumulationField::material;
    operation.outlineWidth = lichen ? 0.055 : 0.09;
    operation.innerHighlightWidth = lichen ? 0.08 : 0.11;
    operation.innerHighlightInset = lichen ? 0.045 : 0.07;

    overlay.fragment.layers = {foundation, growth};
    overlay.fragment.hierarchy = {
        {groupId + ".foundation", groupId},
        {groupId + ".growth", groupId},
    };
    return overlay;
}

} // namespace

MaterialOverlayParseResult parseMaterialOverlay(std::string_view text)
{
    const auto [banner, afterBanner] = takeLine(text);
    if (banner != overlayBanner) {
        return ParseDiagnostic{1, 1, "expected a Paperweight reusable material overlay"};
    }
    const auto [versionLine, afterVersion] = takeLine(afterBanner);
    if (!versionLine.starts_with(versionPrefix)) {
        return ParseDiagnostic{2, 1, "missing material overlay version"};
    }
    std::uint32_t version{};
    const auto versionText = versionLine.substr(versionPrefix.size());
    const auto versionResult = std::from_chars(
        versionText.data(), versionText.data() + versionText.size(), version);
    if (versionResult.ec != std::errc{} ||
        versionResult.ptr != versionText.data() + versionText.size() ||
        version != currentMaterialOverlayVersion) {
        return ParseDiagnostic{2, versionPrefix.size() + 1, "unsupported material overlay version"};
    }
    const auto [idLine, afterId] = takeLine(afterVersion);
    const auto [nameLine, afterName] = takeLine(afterId);
    const auto [descriptionLine, fragmentText] = takeLine(afterName);
    if (!idLine.starts_with(idPrefix) || !nameLine.starts_with(namePrefix) ||
        !descriptionLine.starts_with(descriptionPrefix)) {
        return ParseDiagnostic{3, 1, "material overlay metadata is incomplete or out of order"};
    }

    MaterialOverlay overlay;
    overlay.identifier = std::string(idLine.substr(idPrefix.size()));
    overlay.name = std::string(nameLine.substr(namePrefix.size()));
    overlay.description = std::string(descriptionLine.substr(descriptionPrefix.size()));
    if (!validText(overlay.identifier, 96, true)) {
        return ParseDiagnostic{3, idPrefix.size() + 1, "invalid material overlay identifier"};
    }
    if (!validText(overlay.name, 128, false)) {
        return ParseDiagnostic{4, namePrefix.size() + 1, "invalid material overlay name"};
    }
    if (!validText(overlay.description, 512, false)) {
        return ParseDiagnostic{5, descriptionPrefix.size() + 1, "invalid material overlay description"};
    }
    auto parsed = parseLayerFragment(fragmentText);
    if (const auto* diagnostic = std::get_if<ParseDiagnostic>(&parsed)) {
        auto adjusted = *diagnostic;
        adjusted.line += 5;
        return adjusted;
    }
    overlay.fragment = std::get<LayerFragment>(std::move(parsed));
    if (overlay.fragment.groups.empty()) {
        return ParseDiagnostic{6, 1, "a reusable overlay must contain a compositing group"};
    }
    return overlay;
}

MaterialOverlaySerialisationResult serialiseMaterialOverlay(
    const MaterialOverlay& overlay)
{
    if (!validText(overlay.identifier, 96, true)) {
        return SerialisationError{"invalid material overlay identifier"};
    }
    if (!validText(overlay.name, 128, false)) {
        return SerialisationError{"invalid material overlay name"};
    }
    if (!validText(overlay.description, 512, false)) {
        return SerialisationError{"invalid material overlay description"};
    }
    if (overlay.fragment.groups.empty()) {
        return SerialisationError{"a reusable overlay must contain a compositing group"};
    }
    const auto fragment = serialiseLayerFragment(overlay.fragment);
    if (const auto* error = std::get_if<SerialisationError>(&fragment)) {
        return *error;
    }

    std::string output;
    const auto& fragmentText = std::get<std::string>(fragment);
    output.reserve(fragmentText.size() + overlay.name.size() + overlay.description.size() + 180);
    output += overlayBanner;
    output += '\n';
    output += versionPrefix;
    output += std::to_string(currentMaterialOverlayVersion);
    output += '\n';
    output += idPrefix;
    output += overlay.identifier;
    output += '\n';
    output += namePrefix;
    output += overlay.name;
    output += '\n';
    output += descriptionPrefix;
    output += overlay.description;
    output += '\n';
    output += fragmentText;
    return output;
}

const std::vector<MaterialOverlay>& builtInMaterialOverlays()
{
    static const std::vector<MaterialOverlay> overlays{
        makeOrganicOverlay(false),
        makeOrganicOverlay(true),
    };
    return overlays;
}

const MaterialOverlay* findBuiltInMaterialOverlay(std::string_view identifier)
{
    const auto& overlays = builtInMaterialOverlays();
    const auto match = std::find_if(overlays.begin(), overlays.end(), [identifier](const auto& value) {
        return value.identifier == identifier;
    });
    return match == overlays.end() ? nullptr : &*match;
}

} // namespace paperweight
