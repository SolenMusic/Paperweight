#include <paperweight/material_library.hpp>

#include <paperweight/pmat.hpp>

#include <algorithm>
#include <map>
#include <numeric>
#include <utility>

namespace paperweight {

std::span<const MaterialLibraryEntry> MaterialLibraryIndex::entries() const noexcept
{
    return entries_;
}

std::span<const MaterialLibraryDiagnostic> MaterialLibraryIndex::diagnostics() const noexcept
{
    return diagnostics_;
}

const MaterialLibraryEntry* MaterialLibraryIndex::findByUid(std::string_view uid) const noexcept
{
    const MaterialLibraryEntry* match = nullptr;
    for (const auto& entry : entries_) {
        if (!entry.material.metadata || entry.material.metadata->uid != uid) {
            continue;
        }
        if (!entry.libraryReady || match != nullptr) {
            return nullptr;
        }
        match = &entry;
    }
    return match;
}

MaterialLibraryIndex indexMaterialLibrary(std::span<const MaterialLibrarySource> sources)
{
    MaterialLibraryIndex result;
    std::vector<std::size_t> order(sources.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        if (sources[left].path != sources[right].path) {
            return sources[left].path < sources[right].path;
        }
        return sources[left].text < sources[right].text;
    });

    for (const auto sourceIndex : order) {
        const auto& source = sources[sourceIndex];
        const auto parsed = parsePmat(source.text);
        if (const auto* diagnostic = std::get_if<ParseDiagnostic>(&parsed)) {
            result.diagnostics_.push_back({
                MaterialLibraryDiagnosticCode::invalidPmat,
                source.path,
                diagnostic->line,
                diagnostic->column,
                diagnostic->message,
            });
            continue;
        }
        result.entries_.push_back({source.path, std::get<Material>(parsed), true});
    }

    std::map<std::string, std::vector<std::size_t>, std::less<>> entriesByUid;
    for (std::size_t index = 0; index < result.entries_.size(); ++index) {
        auto& entry = result.entries_[index];
        if (!entry.material.metadata || entry.material.metadata->uid.empty()) {
            entry.libraryReady = false;
            result.diagnostics_.push_back({
                MaterialLibraryDiagnosticCode::missingUid,
                entry.path,
                0,
                0,
                "material has no stable UID",
            });
        } else {
            entriesByUid[entry.material.metadata->uid].push_back(index);
        }
        if (!entry.material.metadata || entry.material.metadata->name.empty()) {
            entry.libraryReady = false;
            result.diagnostics_.push_back({
                MaterialLibraryDiagnosticCode::missingName,
                entry.path,
                0,
                0,
                "material has no friendly name",
            });
        }
    }

    for (const auto& [uid, indices] : entriesByUid) {
        if (indices.size() < 2) {
            continue;
        }
        for (const auto index : indices) {
            auto& entry = result.entries_[index];
            entry.libraryReady = false;
            result.diagnostics_.push_back({
                MaterialLibraryDiagnosticCode::duplicateUid,
                entry.path,
                0,
                0,
                "material UID '" + uid + "' is also used by another library file",
            });
        }
    }

    std::stable_sort(
        result.diagnostics_.begin(),
        result.diagnostics_.end(),
        [](const auto& left, const auto& right) {
            if (left.path != right.path) {
                return left.path < right.path;
            }
            if (left.code != right.code) {
                return left.code < right.code;
            }
            if (left.line != right.line) {
                return left.line < right.line;
            }
            return left.column < right.column;
        });
    return result;
}

} // namespace paperweight
