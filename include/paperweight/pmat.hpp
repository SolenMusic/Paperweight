#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include <paperweight/material.hpp>

namespace paperweight {

inline constexpr std::uint32_t minimumSupportedPmatVersion = 1;
inline constexpr std::uint32_t currentPmatVersion = 14;

struct ParseDiagnostic {
    std::size_t line;
    std::size_t column;
    std::string message;
};

struct SerialisationError {
    std::string message;
};

using ParseResult = std::variant<Material, ParseDiagnostic>;
using SerialisationResult = std::variant<std::string, SerialisationError>;

[[nodiscard]] ParseResult parsePmat(std::string_view text);
[[nodiscard]] SerialisationResult serialisePmat(const Material& material);

} // namespace paperweight
