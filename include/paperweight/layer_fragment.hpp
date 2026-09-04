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

inline constexpr std::uint32_t currentLayerFragmentVersion = 1;

// A fragment carries only layers. The PMAT carrier metadata used by the text
// encoding is ignored on parse; paste creates fresh editor identities and the
// destination material remains the source of deterministic variation.
struct LayerFragment {
    std::vector<MaterialLayer> layers;

    friend bool operator==(const LayerFragment&, const LayerFragment&) = default;
};

using LayerFragmentParseResult = std::variant<LayerFragment, ParseDiagnostic>;
using LayerFragmentSerialisationResult = std::variant<std::string, SerialisationError>;

[[nodiscard]] LayerFragmentParseResult parseLayerFragment(std::string_view text);
[[nodiscard]] LayerFragmentSerialisationResult serialiseLayerFragment(
    std::span<const MaterialLayer> layers);

} // namespace paperweight
