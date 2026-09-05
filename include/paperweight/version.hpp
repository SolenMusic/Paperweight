#pragma once

#include <cstdint>
#include <string_view>

namespace paperweight {

struct Version {
    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;

    friend constexpr bool operator==(const Version&, const Version&) = default;
};

inline constexpr Version currentVersion{0, 0, 33};

[[nodiscard]] std::string_view versionString() noexcept;

} // namespace paperweight
