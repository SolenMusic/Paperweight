#include <paperweight/layer_fragment.hpp>

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace paperweight {
namespace {

constexpr std::string_view fragmentBanner = "# Paperweight layer fragment";
constexpr std::string_view versionPrefix = "paperweight.layer_fragment.version = ";

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

} // namespace

LayerFragmentParseResult parseLayerFragment(std::string_view text)
{
    const auto [banner, afterBanner] = takeLine(text);
    if (banner != fragmentBanner) {
        return ParseDiagnostic{1, 1, "expected a Paperweight layer fragment"};
    }

    const auto [versionLine, pmatText] = takeLine(afterBanner);
    if (!versionLine.starts_with(versionPrefix)) {
        return ParseDiagnostic{2, 1, "missing layer fragment version"};
    }
    const auto versionText = versionLine.substr(versionPrefix.size());
    std::uint32_t version{};
    const auto result = std::from_chars(
        versionText.data(), versionText.data() + versionText.size(), version);
    if (result.ec != std::errc{} || result.ptr != versionText.data() + versionText.size()) {
        return ParseDiagnostic{2, versionPrefix.size() + 1, "invalid layer fragment version"};
    }
    if (version != currentLayerFragmentVersion) {
        return ParseDiagnostic{2, versionPrefix.size() + 1, "unsupported layer fragment version"};
    }
    if (pmatText.empty()) {
        return ParseDiagnostic{3, 1, "layer fragment has no PMAT payload"};
    }

    auto parsed = parsePmat(pmatText);
    if (const auto* diagnostic = std::get_if<ParseDiagnostic>(&parsed)) {
        auto adjusted = *diagnostic;
        adjusted.line += 2;
        return adjusted;
    }
    auto material = std::get<Material>(std::move(parsed));
    if (material.layers.empty()) {
        return ParseDiagnostic{3, 1, "layer fragment contains no layers"};
    }
    return LayerFragment{std::move(material.layers)};
}

LayerFragmentSerialisationResult serialiseLayerFragment(
    std::span<const MaterialLayer> layers)
{
    if (layers.empty()) {
        return SerialisationError{"cannot copy an empty layer selection"};
    }
    if (layers.size() > LayerLimits::maximumLayers) {
        return SerialisationError{"layer fragment exceeds the maximum layer count"};
    }

    Material carrier;
    carrier.layers.assign(layers.begin(), layers.end());
    const auto encoded = serialisePmat(carrier);
    if (const auto* error = std::get_if<SerialisationError>(&encoded)) {
        return *error;
    }

    std::string output;
    const auto& pmat = std::get<std::string>(encoded);
    output.reserve(fragmentBanner.size() + versionPrefix.size() + pmat.size() + 32);
    output += fragmentBanner;
    output += '\n';
    output += versionPrefix;
    output += std::to_string(currentLayerFragmentVersion);
    output += '\n';
    output += pmat;
    return output;
}

} // namespace paperweight
