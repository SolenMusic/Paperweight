#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <paperweight/material.hpp>

namespace paperweight {

struct MaterialLibrarySource {
    std::string path;
    std::string text;
};

enum class MaterialLibraryDiagnosticCode {
    invalidPmat,
    missingUid,
    missingName,
    duplicateUid,
};

struct MaterialLibraryDiagnostic {
    MaterialLibraryDiagnosticCode code{MaterialLibraryDiagnosticCode::invalidPmat};
    std::string path;
    std::size_t line{};
    std::size_t column{};
    std::string message;

    friend bool operator==(
        const MaterialLibraryDiagnostic&,
        const MaterialLibraryDiagnostic&) = default;
};

struct MaterialLibraryEntry {
    std::string path;
    Material material;
    bool libraryReady{true};

    friend bool operator==(const MaterialLibraryEntry&, const MaterialLibraryEntry&) = default;
};

class MaterialLibraryIndex {
public:
    [[nodiscard]] std::span<const MaterialLibraryEntry> entries() const noexcept;
    [[nodiscard]] std::span<const MaterialLibraryDiagnostic> diagnostics() const noexcept;
    [[nodiscard]] const MaterialLibraryEntry* findByUid(std::string_view uid) const noexcept;

private:
    friend MaterialLibraryIndex indexMaterialLibrary(
        std::span<const MaterialLibrarySource> sources);

    std::vector<MaterialLibraryEntry> entries_;
    std::vector<MaterialLibraryDiagnostic> diagnostics_;
};

[[nodiscard]] MaterialLibraryIndex indexMaterialLibrary(
    std::span<const MaterialLibrarySource> sources);

} // namespace paperweight
