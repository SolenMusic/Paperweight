#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <paperweight/material.hpp>
#include <paperweight/material_library.hpp>

namespace paperweight {

inline constexpr std::uint32_t currentPwlibVersion = 1;
inline constexpr std::size_t pwlibNoEntry = std::numeric_limits<std::size_t>::max();

enum class PwlibStorageMode : std::uint8_t {
    raw = 0,
    rle = 1,
};

struct PwlibEntry {
    std::string_view uid;
    std::string_view name;
    PwlibStorageMode storageMode{PwlibStorageMode::raw};
    std::uint64_t storedSize{};
    std::uint64_t uncompressedSize{};
    std::uint64_t checksum{};
};

enum class PwlibErrorCode {
    invalidSourceLibrary,
    emptyLibrary,
    serialisationFailure,
    invalidMagic,
    incompatibleVersion,
    truncatedData,
    invalidStructure,
    checksumMismatch,
    duplicateUid,
    invalidEntry,
    entryNotFound,
    allocationFailure,
};

struct PwlibError {
    PwlibErrorCode code{PwlibErrorCode::invalidStructure};
    std::size_t entryIndex{pwlibNoEntry};
    std::string message;
};

struct PwlibPackOptions {
    // Automatic RLE remains deterministic and is selected per entry only when
    // its encoded payload is strictly smaller than the canonical PMAT text.
    bool allowRle{true};
};

using PwlibPackResult = std::variant<std::vector<std::uint8_t>, PwlibError>;
using PwlibMaterialResult = std::variant<Material, PwlibError>;

class PackedMaterialLibrary {
public:
    // The caller owns the byte span and must keep it alive for this view's
    // entire lifetime. Entries and their UID/name string views point into it.
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::span<const PwlibEntry> entries() const noexcept { return entries_; }
    [[nodiscard]] std::uint32_t formatVersion() const noexcept { return formatVersion_; }
    [[nodiscard]] std::uint64_t checksum() const noexcept { return checksum_; }

    [[nodiscard]] const PwlibEntry* findByUid(std::string_view uid) const noexcept;
    [[nodiscard]] PwlibMaterialResult materialByUid(std::string_view uid) const;
    [[nodiscard]] PwlibMaterialResult instantiateByUid(
        std::string_view uid,
        std::uint64_t seed) const;

private:
    friend std::variant<PackedMaterialLibrary, PwlibError> readPwlib(
        std::span<const std::uint8_t> bytes);

    [[nodiscard]] std::variant<std::vector<std::uint8_t>, PwlibError> decodeEntry(
        std::size_t index) const;

    std::span<const std::uint8_t> bytes_;
    std::vector<PwlibEntry> entries_;
    std::vector<std::uint64_t> payloadOffsets_;
    std::uint32_t formatVersion_{};
    std::uint64_t checksum_{};
};

using PwlibReadResult = std::variant<PackedMaterialLibrary, PwlibError>;

[[nodiscard]] PwlibPackResult packPwlib(
    std::span<const MaterialLibrarySource> sources,
    const PwlibPackOptions& options = {});

[[nodiscard]] PwlibReadResult readPwlib(std::span<const std::uint8_t> bytes);

} // namespace paperweight
