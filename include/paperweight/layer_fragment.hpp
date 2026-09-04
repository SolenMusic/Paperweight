#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <paperweight/layer.hpp>
#include <paperweight/pmat.hpp>

namespace paperweight {

inline constexpr std::uint32_t minimumLayerFragmentVersion = 1;
inline constexpr std::uint32_t currentLayerFragmentVersion = 2;

// A fragment carries a self-contained layer subtree. Paste remaps every
// identity, while the destination material remains the source of deterministic
// variation (notably the material seed).
struct LayerFragment {
    std::vector<MaterialLayer> layers;
    std::vector<MaterialLayerGroup> groups;
    std::vector<MaterialLayerHierarchy> hierarchy;

    friend bool operator==(const LayerFragment&, const LayerFragment&) = default;
};

using LayerFragmentParseResult = std::variant<LayerFragment, ParseDiagnostic>;
using LayerFragmentSerialisationResult = std::variant<std::string, SerialisationError>;

[[nodiscard]] LayerFragmentParseResult parseLayerFragment(std::string_view text);
[[nodiscard]] LayerFragmentSerialisationResult serialiseLayerFragment(
    std::span<const MaterialLayer> layers);
[[nodiscard]] LayerFragmentSerialisationResult serialiseLayerFragment(
    const LayerFragment& fragment);

} // namespace paperweight
