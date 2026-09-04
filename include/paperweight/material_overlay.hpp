#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <paperweight/layer_fragment.hpp>

namespace paperweight {

inline constexpr std::uint32_t currentMaterialOverlayVersion = 1;

struct MaterialOverlay {
    std::string identifier;
    std::string name;
    std::string description;
    LayerFragment fragment;

    friend bool operator==(const MaterialOverlay&, const MaterialOverlay&) = default;
};

using MaterialOverlayParseResult = std::variant<MaterialOverlay, ParseDiagnostic>;
using MaterialOverlaySerialisationResult = std::variant<std::string, SerialisationError>;

[[nodiscard]] MaterialOverlayParseResult parseMaterialOverlay(std::string_view text);
[[nodiscard]] MaterialOverlaySerialisationResult serialiseMaterialOverlay(
    const MaterialOverlay& overlay);

// Seedless reusable source presets. The destination material's seed is always
// used after insertion; these definitions carry no hidden material state.
[[nodiscard]] const std::vector<MaterialOverlay>& builtInMaterialOverlays();
[[nodiscard]] const MaterialOverlay* findBuiltInMaterialOverlay(
    std::string_view identifier);

} // namespace paperweight
