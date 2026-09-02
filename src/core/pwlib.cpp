#include <paperweight/pwlib.hpp>

#include <paperweight/pmat.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <utility>

namespace paperweight {
namespace {

constexpr std::array<std::uint8_t, 8> magic{
    'P', 'W', 'L', 'I', 'B', '\r', '\n', 0x1a,
};
constexpr std::uint32_t headerSize = 64;
constexpr std::uint32_t entryRecordSize = 88;
constexpr std::size_t checksumOffset = 48;
constexpr std::size_t checksumSize = 8;
constexpr std::uint32_t maximumEntries = 65'535;
constexpr std::uint64_t maximumEntryBytes = 16ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

struct EncodedEntry {
    std::string uid;
    std::string name;
    std::vector<std::uint8_t> raw;
    std::vector<std::uint8_t> stored;
    PwlibStorageMode storageMode{PwlibStorageMode::raw};
    std::uint64_t checksum{};
};

PwlibError error(
    PwlibErrorCode code,
    std::string message,
    std::size_t entryIndex = pwlibNoEntry)
{
    return {code, entryIndex, std::move(message)};
}

std::uint64_t checksumBytes(std::span<const std::uint8_t> bytes)
{
    std::uint64_t result = fnvOffset;
    for (const auto byte : bytes) {
        result ^= byte;
        result *= fnvPrime;
    }
    return result;
}

std::uint64_t wholeLibraryChecksum(std::span<const std::uint8_t> bytes)
{
    std::uint64_t result = fnvOffset;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto byte = index >= checksumOffset && index < checksumOffset + checksumSize
            ? std::uint8_t{0}
            : bytes[index];
        result ^= byte;
        result *= fnvPrime;
    }
    return result;
}

void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void appendU64(std::vector<std::uint8_t>& output, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void patchU64(std::vector<std::uint8_t>& output, std::size_t offset, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8) {
        output[offset + shift / 8] = static_cast<std::uint8_t>((value >> shift) & 0xffU);
    }
}

std::optional<std::uint32_t> readU32(
    std::span<const std::uint8_t> bytes,
    std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        return std::nullopt;
    }
    std::uint32_t result{};
    for (unsigned index = 0; index < 4; ++index) {
        result |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8U);
    }
    return result;
}

std::optional<std::uint64_t> readU64(
    std::span<const std::uint8_t> bytes,
    std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 8) {
        return std::nullopt;
    }
    std::uint64_t result{};
    for (unsigned index = 0; index < 8; ++index) {
        result |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8U);
    }
    return result;
}

bool checkedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t& result)
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

bool rangeInside(std::uint64_t offset, std::uint64_t size, std::size_t available)
{
    std::uint64_t end{};
    return checkedAdd(offset, size, end) && end <= available;
}

std::vector<std::uint8_t> encodeRle(std::span<const std::uint8_t> source)
{
    std::vector<std::uint8_t> output;
    output.reserve(source.size());
    std::size_t cursor{};
    while (cursor < source.size()) {
        std::size_t runLength = 1;
        while (cursor + runLength < source.size() && runLength < 130 &&
               source[cursor + runLength] == source[cursor]) {
            ++runLength;
        }
        if (runLength >= 3) {
            output.push_back(static_cast<std::uint8_t>(0x80U | (runLength - 3)));
            output.push_back(source[cursor]);
            cursor += runLength;
            continue;
        }

        const auto literalStart = cursor;
        cursor += runLength;
        while (cursor < source.size() && cursor - literalStart < 128) {
            runLength = 1;
            while (cursor + runLength < source.size() && runLength < 130 &&
                   source[cursor + runLength] == source[cursor]) {
                ++runLength;
            }
            if (runLength >= 3) {
                break;
            }
            cursor += std::min(runLength, std::size_t{128} - (cursor - literalStart));
        }
        const auto literalLength = cursor - literalStart;
        output.push_back(static_cast<std::uint8_t>(literalLength - 1));
        output.insert(
            output.end(),
            source.begin() + static_cast<std::ptrdiff_t>(literalStart),
            source.begin() + static_cast<std::ptrdiff_t>(cursor));
    }
    return output;
}

std::variant<std::vector<std::uint8_t>, PwlibError> decodeRle(
    std::span<const std::uint8_t> source,
    std::uint64_t expectedSize,
    std::size_t entryIndex)
{
    if (expectedSize > maximumEntryBytes ||
        expectedSize > std::numeric_limits<std::size_t>::max()) {
        return error(PwlibErrorCode::invalidEntry,
                     "packed entry expands beyond the supported size limit", entryIndex);
    }
    std::vector<std::uint8_t> output;
    output.reserve(static_cast<std::size_t>(expectedSize));
    std::size_t cursor{};
    while (cursor < source.size()) {
        const auto control = source[cursor++];
        if ((control & 0x80U) != 0) {
            if (cursor >= source.size()) {
                return error(PwlibErrorCode::invalidEntry,
                             "RLE run is missing its value byte", entryIndex);
            }
            const auto length = static_cast<std::size_t>(control & 0x7fU) + 3;
            if (output.size() > expectedSize || length > expectedSize - output.size()) {
                return error(PwlibErrorCode::invalidEntry,
                             "RLE run exceeds the declared entry size", entryIndex);
            }
            output.insert(output.end(), length, source[cursor++]);
        } else {
            const auto length = static_cast<std::size_t>(control) + 1;
            if (length > source.size() - cursor || output.size() > expectedSize ||
                length > expectedSize - output.size()) {
                return error(PwlibErrorCode::invalidEntry,
                             "RLE literal exceeds its input or declared entry size", entryIndex);
            }
            output.insert(
                output.end(),
                source.begin() + static_cast<std::ptrdiff_t>(cursor),
                source.begin() + static_cast<std::ptrdiff_t>(cursor + length));
            cursor += length;
        }
    }
    if (output.size() != expectedSize) {
        return error(PwlibErrorCode::invalidEntry,
                     "RLE payload does not expand to its declared entry size", entryIndex);
    }
    return output;
}

std::variant<std::vector<std::uint8_t>, PwlibError> rawPayload(
    std::span<const std::uint8_t> bytes,
    const PwlibEntry& entry,
    std::uint64_t payloadOffset,
    std::size_t entryIndex)
{
    if (!rangeInside(payloadOffset, entry.storedSize, bytes.size())) {
        return error(PwlibErrorCode::truncatedData,
                     "packed entry payload is outside the library", entryIndex);
    }
    const auto source = bytes.subspan(
        static_cast<std::size_t>(payloadOffset),
        static_cast<std::size_t>(entry.storedSize));
    if (entry.storageMode == PwlibStorageMode::rle) {
        return decodeRle(source, entry.uncompressedSize, entryIndex);
    }
    return std::vector<std::uint8_t>(source.begin(), source.end());
}

} // namespace

PwlibPackResult packPwlib(
    std::span<const MaterialLibrarySource> sources,
    const PwlibPackOptions& options)
{
    try {
        if (sources.empty()) {
            return error(PwlibErrorCode::emptyLibrary,
                         "a packed material library must contain at least one material");
        }
        const auto index = indexMaterialLibrary(sources);
        if (!index.diagnostics().empty()) {
            const auto& diagnostic = index.diagnostics().front();
            const auto code = diagnostic.code == MaterialLibraryDiagnosticCode::duplicateUid
                ? PwlibErrorCode::duplicateUid
                : PwlibErrorCode::invalidSourceLibrary;
            return error(code,
                         diagnostic.path + ": " + diagnostic.message);
        }
        if (index.entries().size() > maximumEntries) {
            return error(PwlibErrorCode::invalidSourceLibrary,
                         "source library contains too many materials");
        }

        std::vector<EncodedEntry> entries;
        entries.reserve(index.entries().size());
        for (const auto& sourceEntry : index.entries()) {
            if (!sourceEntry.material.metadata) {
                return error(PwlibErrorCode::invalidSourceLibrary,
                             sourceEntry.path + ": material identity is missing");
            }
            const auto serialised = serialisePmat(sourceEntry.material);
            if (const auto* serialisationError = std::get_if<SerialisationError>(&serialised)) {
                return error(PwlibErrorCode::serialisationFailure,
                             sourceEntry.path + ": " + serialisationError->message);
            }
            const auto& text = std::get<std::string>(serialised);
            if (text.empty() || text.size() > maximumEntryBytes) {
                return error(PwlibErrorCode::invalidSourceLibrary,
                             sourceEntry.path + ": canonical PMAT payload is outside the size limit");
            }
            EncodedEntry entry;
            entry.uid = sourceEntry.material.metadata->uid;
            entry.name = sourceEntry.material.metadata->name;
            entry.raw.assign(text.begin(), text.end());
            entry.checksum = checksumBytes(entry.raw);
            entry.stored = entry.raw;
            if (options.allowRle) {
                auto encoded = encodeRle(entry.raw);
                if (encoded.size() < entry.raw.size()) {
                    entry.storageMode = PwlibStorageMode::rle;
                    entry.stored = std::move(encoded);
                }
            }
            entries.push_back(std::move(entry));
        }
        std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
            return left.uid < right.uid;
        });

        const auto tableBytes = static_cast<std::uint64_t>(entries.size()) * entryRecordSize;
        const std::uint64_t directoryOffset = headerSize;
        std::uint64_t namesOffset{};
        if (!checkedAdd(directoryOffset, tableBytes, namesOffset)) {
            return error(PwlibErrorCode::allocationFailure, "packed library size overflow");
        }
        std::uint64_t dataOffset = namesOffset;
        for (const auto& entry : entries) {
            if (!checkedAdd(dataOffset, entry.name.size(), dataOffset)) {
                return error(PwlibErrorCode::allocationFailure, "packed library size overflow");
            }
        }
        std::uint64_t fileSize = dataOffset;
        for (const auto& entry : entries) {
            if (!checkedAdd(fileSize, entry.stored.size(), fileSize)) {
                return error(PwlibErrorCode::allocationFailure, "packed library size overflow");
            }
        }
        if (fileSize > std::numeric_limits<std::size_t>::max()) {
            return error(PwlibErrorCode::allocationFailure,
                         "packed library does not fit in this address space");
        }

        std::vector<std::uint8_t> output;
        output.reserve(static_cast<std::size_t>(fileSize));
        output.insert(output.end(), magic.begin(), magic.end());
        appendU32(output, currentPwlibVersion);
        appendU32(output, headerSize);
        appendU32(output, static_cast<std::uint32_t>(entries.size()));
        appendU32(output, entryRecordSize);
        appendU64(output, directoryOffset);
        appendU64(output, dataOffset);
        appendU64(output, fileSize);
        appendU64(output, 0);
        appendU64(output, 0);

        std::uint64_t nameCursor = namesOffset;
        std::uint64_t payloadCursor = dataOffset;
        for (const auto& entry : entries) {
            output.insert(output.end(), entry.uid.begin(), entry.uid.end());
            output.push_back(static_cast<std::uint8_t>(entry.storageMode));
            output.insert(output.end(), 3, std::uint8_t{0});
            appendU64(output, nameCursor);
            appendU32(output, static_cast<std::uint32_t>(entry.name.size()));
            appendU32(output, 0);
            appendU64(output, payloadCursor);
            appendU64(output, entry.stored.size());
            appendU64(output, entry.raw.size());
            appendU64(output, entry.checksum);
            nameCursor += entry.name.size();
            payloadCursor += entry.stored.size();
        }
        for (const auto& entry : entries) {
            output.insert(output.end(), entry.name.begin(), entry.name.end());
        }
        for (const auto& entry : entries) {
            output.insert(output.end(), entry.stored.begin(), entry.stored.end());
        }
        if (output.size() != fileSize) {
            return error(PwlibErrorCode::invalidStructure,
                         "internal packed-library layout calculation failed");
        }
        patchU64(output, checksumOffset, wholeLibraryChecksum(output));
        return output;
    } catch (const std::bad_alloc&) {
        return error(PwlibErrorCode::allocationFailure,
                     "memory allocation failed while packing the material library");
    }
}

PwlibReadResult readPwlib(std::span<const std::uint8_t> bytes)
{
    try {
        if (bytes.size() < headerSize) {
            return error(PwlibErrorCode::truncatedData,
                         "packed material library header is truncated");
        }
        if (!std::equal(magic.begin(), magic.end(), bytes.begin())) {
            return error(PwlibErrorCode::invalidMagic,
                         "data is not a Paperweight packed material library");
        }
        const auto version = readU32(bytes, 8).value_or(0);
        if (version != currentPwlibVersion) {
            return error(PwlibErrorCode::incompatibleVersion,
                         "packed library format version " + std::to_string(version) +
                             " is not supported by this build");
        }
        const auto storedHeaderSize = readU32(bytes, 12).value_or(0);
        const auto entryCount = readU32(bytes, 16).value_or(0);
        const auto storedEntrySize = readU32(bytes, 20).value_or(0);
        const auto directoryOffset = readU64(bytes, 24).value_or(0);
        const auto dataOffset = readU64(bytes, 32).value_or(0);
        const auto fileSize = readU64(bytes, 40).value_or(0);
        const auto storedChecksum = readU64(bytes, checksumOffset).value_or(0);
        const auto reserved = readU64(bytes, 56).value_or(1);
        if (storedHeaderSize != headerSize || storedEntrySize != entryRecordSize ||
            directoryOffset != headerSize || reserved != 0) {
            return error(PwlibErrorCode::invalidStructure,
                         "packed library uses an invalid header or entry layout");
        }
        if (entryCount == 0 || entryCount > maximumEntries) {
            return error(PwlibErrorCode::invalidStructure,
                         "packed library has an invalid entry count");
        }
        if (fileSize != bytes.size()) {
            return error(PwlibErrorCode::truncatedData,
                         "packed library byte count does not match its header");
        }
        const auto tableSize = static_cast<std::uint64_t>(entryCount) * entryRecordSize;
        std::uint64_t tableEnd{};
        if (!checkedAdd(directoryOffset, tableSize, tableEnd) || tableEnd > dataOffset ||
            dataOffset > fileSize) {
            return error(PwlibErrorCode::invalidStructure,
                         "packed library directory offsets are invalid");
        }

        PackedMaterialLibrary library;
        library.bytes_ = bytes;
        library.formatVersion_ = version;
        library.checksum_ = storedChecksum;
        library.entries_.reserve(entryCount);
        library.payloadOffsets_.reserve(entryCount);
        std::uint64_t expectedNameOffset = tableEnd;
        std::uint64_t expectedPayloadOffset = dataOffset;
        std::string_view previousUid;
        for (std::uint32_t index = 0; index < entryCount; ++index) {
            const auto recordOffset = static_cast<std::size_t>(
                directoryOffset + static_cast<std::uint64_t>(index) * entryRecordSize);
            const std::string_view uid(
                reinterpret_cast<const char*>(bytes.data() + recordOffset), 36);
            const auto storageByte = bytes[recordOffset + 36];
            if (bytes[recordOffset + 37] != 0 || bytes[recordOffset + 38] != 0 ||
                bytes[recordOffset + 39] != 0 ||
                readU32(bytes, recordOffset + 52).value_or(1) != 0) {
                return error(PwlibErrorCode::invalidStructure,
                             "packed entry has non-zero reserved fields", index);
            }
            if (!isCanonicalMaterialUid(uid)) {
                return error(PwlibErrorCode::invalidEntry,
                             "packed entry UID is not a canonical lowercase UUID", index);
            }
            if (!previousUid.empty() && uid <= previousUid) {
                const auto code = uid == previousUid
                    ? PwlibErrorCode::duplicateUid
                    : PwlibErrorCode::invalidStructure;
                return error(code,
                             uid == previousUid
                                 ? "packed library contains a duplicate material UID"
                                 : "packed library entry table is not sorted by UID",
                             index);
            }
            previousUid = uid;
            if (storageByte > static_cast<std::uint8_t>(PwlibStorageMode::rle)) {
                return error(PwlibErrorCode::invalidEntry,
                             "packed entry uses an unknown storage mode", index);
            }
            const auto nameOffset = readU64(bytes, recordOffset + 40).value_or(0);
            const auto nameSize = readU32(bytes, recordOffset + 48).value_or(0);
            const auto payloadOffset = readU64(bytes, recordOffset + 56).value_or(0);
            const auto storedSize = readU64(bytes, recordOffset + 64).value_or(0);
            const auto rawSize = readU64(bytes, recordOffset + 72).value_or(0);
            const auto entryChecksum = readU64(bytes, recordOffset + 80).value_or(0);
            if (nameSize == 0 || nameSize > MaterialLimits::maximumNameLength ||
                nameOffset != expectedNameOffset ||
                !rangeInside(nameOffset, nameSize, bytes.size()) ||
                nameOffset + nameSize > dataOffset) {
                return error(PwlibErrorCode::invalidStructure,
                             "packed entry name range is invalid", index);
            }
            if (storedSize == 0 || rawSize == 0 || rawSize > maximumEntryBytes ||
                payloadOffset != expectedPayloadOffset ||
                !rangeInside(payloadOffset, storedSize, bytes.size())) {
                return error(PwlibErrorCode::invalidStructure,
                             "packed entry payload range is invalid", index);
            }
            const auto storageMode = static_cast<PwlibStorageMode>(storageByte);
            if ((storageMode == PwlibStorageMode::raw && storedSize != rawSize) ||
                (storageMode == PwlibStorageMode::rle && storedSize >= rawSize)) {
                return error(PwlibErrorCode::invalidEntry,
                             "packed entry storage sizes do not match its mode", index);
            }
            const std::string_view name(
                reinterpret_cast<const char*>(bytes.data() + nameOffset), nameSize);
            library.entries_.push_back({
                uid,
                name,
                storageMode,
                storedSize,
                rawSize,
                entryChecksum,
            });
            library.payloadOffsets_.push_back(payloadOffset);
            expectedNameOffset += nameSize;
            expectedPayloadOffset += storedSize;
        }
        if (expectedNameOffset != dataOffset || expectedPayloadOffset != fileSize) {
            return error(PwlibErrorCode::invalidStructure,
                         "packed library contains gaps or trailing data");
        }

        for (std::size_t index = 0; index < library.entries_.size(); ++index) {
            const auto decoded = library.decodeEntry(index);
            if (const auto* decodeError = std::get_if<PwlibError>(&decoded)) {
                return *decodeError;
            }
            const auto& raw = std::get<std::vector<std::uint8_t>>(decoded);
            if (checksumBytes(raw) != library.entries_[index].checksum) {
                return error(PwlibErrorCode::checksumMismatch,
                             "packed entry checksum does not match its material data", index);
            }
            const std::string_view text(
                reinterpret_cast<const char*>(raw.data()), raw.size());
            const auto parsed = parsePmat(text);
            const auto* material = std::get_if<Material>(&parsed);
            if (material == nullptr || !material->metadata ||
                material->metadata->uid != library.entries_[index].uid ||
                material->metadata->name != library.entries_[index].name) {
                return error(PwlibErrorCode::invalidEntry,
                             "packed entry metadata does not match its canonical PMAT payload",
                             index);
            }
        }
        if (wholeLibraryChecksum(bytes) != storedChecksum) {
            return error(PwlibErrorCode::checksumMismatch,
                         "packed library checksum does not match its contents");
        }
        return library;
    } catch (const std::bad_alloc&) {
        return error(PwlibErrorCode::allocationFailure,
                     "memory allocation failed while reading the packed library");
    }
}

const PwlibEntry* PackedMaterialLibrary::findByUid(std::string_view uid) const noexcept
{
    const auto found = std::lower_bound(
        entries_.begin(), entries_.end(), uid,
        [](const auto& entry, std::string_view candidate) {
            return entry.uid < candidate;
        });
    return found != entries_.end() && found->uid == uid ? &*found : nullptr;
}

std::variant<std::vector<std::uint8_t>, PwlibError> PackedMaterialLibrary::decodeEntry(
    std::size_t index) const
{
    if (index >= entries_.size()) {
        return error(PwlibErrorCode::invalidEntry,
                     "packed entry index is outside the library", index);
    }
    return rawPayload(bytes_, entries_[index], payloadOffsets_[index], index);
}

PwlibMaterialResult PackedMaterialLibrary::materialByUid(std::string_view uid) const
{
    const auto* entry = findByUid(uid);
    if (entry == nullptr) {
        return error(PwlibErrorCode::entryNotFound,
                     "packed material UID was not found");
    }
    const auto index = static_cast<std::size_t>(entry - entries_.data());
    const auto decoded = decodeEntry(index);
    if (const auto* decodeError = std::get_if<PwlibError>(&decoded)) {
        return *decodeError;
    }
    const auto& raw = std::get<std::vector<std::uint8_t>>(decoded);
    const std::string_view text(reinterpret_cast<const char*>(raw.data()), raw.size());
    const auto parsed = parsePmat(text);
    if (const auto* diagnostic = std::get_if<ParseDiagnostic>(&parsed)) {
        return error(PwlibErrorCode::invalidEntry,
                     "packed material cannot be parsed: " + diagnostic->message, index);
    }
    return std::get<Material>(parsed);
}

PwlibMaterialResult PackedMaterialLibrary::instantiateByUid(
    std::string_view uid,
    std::uint64_t seed) const
{
    auto result = materialByUid(uid);
    if (auto* material = std::get_if<Material>(&result)) {
        material->seed = seed;
    }
    return result;
}

} // namespace paperweight
