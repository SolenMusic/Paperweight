# Portable Material Library Packs

Paperweight v0.0.21 can compile a readable folder of `.pmat` source files into
one deterministic `.pwlib` deployment artefact. The folder remains the source
of truth. A pack is deliberately closer to an executable than a project file:
rebuild it whenever its sources or this provisional format change.

The encoder and reader live in the portable C++20 core. They use no filesystem,
Apple, threading, or third-party APIs. The command-line tool and AppKit library
window merely gather source text and write the returned bytes.

## Portable API

Include `paperweight/pwlib.hpp` and give the reader a byte span:

```cpp
auto opened = paperweight::readPwlib(bytes);
auto* library = std::get_if<paperweight::PackedMaterialLibrary>(&opened);
if (library == nullptr) {
    // Inspect the PwlibError alternative.
}

for (const auto& entry : library->entries()) {
    use(entry.uid, entry.name);
}

auto result = library->instantiateByUid(uid, chosenSeed);
auto* material = std::get_if<paperweight::Material>(&result);
```

`PackedMaterialLibrary` is a view. Its UID and name views point directly into
the caller-owned byte span, so those bytes must outlive the library object.
Payloads are decoded and parsed only into ordinary `Material` values. The
caller then uses the existing `generate` API without a game-specific adapter.

`materialByUid` retrieves the authored material and its authored seed.
`instantiateByUid` retrieves the same material but replaces the seed with the
caller's choice. Identity and descriptive metadata remain available; they do
not participate in generation.

## Binary layout

Version 1 is little-endian and has no implicit padding. All offsets are absolute
from the first byte. A decoder rejects gaps, overlaps, trailing data, non-zero
reserved fields, unsorted identities, and ranges outside the supplied span.

### Header: 64 bytes

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | `PWLIB\r\n` followed by byte `0x1a` |
| 8 | 4 | format version, currently `1` |
| 12 | 4 | header size, `64` |
| 16 | 4 | entry count |
| 20 | 4 | entry-record size, `88` |
| 24 | 8 | entry-table offset, `64` |
| 32 | 8 | payload-data offset |
| 40 | 8 | complete file size |
| 48 | 8 | whole-library checksum |
| 56 | 8 | reserved, zero |

### Entry record: 88 bytes

Entries are sorted by canonical lowercase UUID text. Fixed UUID text avoids a
platform-dependent UUID representation and permits lookup without decoding a
material.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 36 | canonical material UID, without a terminator |
| 36 | 1 | storage mode: `0` raw, `1` RLE |
| 37 | 3 | reserved, zero |
| 40 | 8 | friendly-name offset |
| 48 | 4 | friendly-name byte count |
| 52 | 4 | reserved, zero |
| 56 | 8 | stored-payload offset |
| 64 | 8 | stored-payload byte count |
| 72 | 8 | uncompressed PMAT byte count |
| 80 | 8 | checksum of uncompressed canonical PMAT bytes |

All friendly UTF-8 names follow the complete entry table in entry order, with
no terminators or gaps. Stored payloads then follow in the same order. A payload
is the canonical current-version PMAT serialisation produced by `serialisePmat`, not
the source file's incidental whitespace. Equivalent parsed materials therefore
produce identical pack entries.

## RLE storage

The packer first calculates the deterministic RLE representation and uses it
only when it is strictly smaller than the raw PMAT payload. `--raw` disables
that choice for inspection or comparison.

RLE is a bounded PackBits-style byte stream:

- Control bytes `0x00` through `0x7f` introduce 1 through 128 literal bytes.
- Control bytes `0x80` through `0xff` introduce a repeated byte; the run length
  is the low seven bits plus 3, giving runs of 3 through 130 bytes.

Short runs remain literals. Encoding is greedy and canonical; decoding checks
the declared uncompressed size before every write. RLE is not expected to win
for every textual recipe. Raw storage is a normal and intentional outcome.

## Integrity and rejection

Every entry stores a 64-bit FNV-1a checksum of its uncompressed canonical PMAT
bytes. The header stores another FNV-1a checksum over the complete `.pwlib`,
treating the checksum field itself as eight zero bytes. These are corruption
checks, not cryptographic signatures.

Opening a pack validates its structure, storage modes, canonical and unique
UIDs, entry checksums, parseable PMAT payloads, directory-to-payload identity,
and whole-library checksum. Version mismatches, truncation, decompression
overrun, duplicate identities, and corrupt bytes return a bounded `PwlibError`.
An entry may expand to at most 16 MiB and a pack may contain at most 65,535
materials in version 1.

## Command-line packer

Builds include `paperweight_pack` by default:

```sh
paperweight_pack -o game-materials.pwlib path/to/material-library
paperweight_pack -o selected.pwlib stone.pmat metal.pmat
paperweight_pack --raw -o inspectable.pwlib path/to/material-library
```

Inputs may be individual `.pmat` files or folders searched recursively. Every
selected material must parse, have a canonical UID and friendly name, and be
unique within the export. A bad sibling prevents the derived artefact from
being written rather than silently disappearing from a game build.

Add `--cpp-header` to emit a C/C++ byte array beside the ordinary pack:

```sh
paperweight_pack -o game-materials.pwlib \
  --cpp-header paperweight_materials.h \
  --symbol paperweight_materials \
  path/to/material-library
```

The generated header uses only `unsigned char`, `size_t`, and `<stddef.h>`, so
it can be included from either C or C++. `examples/embed_pwlib.cpp` demonstrates
enumeration, UID lookup, caller-selected seed instantiation, and generation
directly from that array.

## Native export

Open **Tools > Material Library**, then choose **Export Pack**. With no table
selection it exports the complete working folder. With one or more selected
rows it asks whether to export those materials or the entire folder. The result
dialog reports the entry count, byte size, and how many entries benefited from
RLE.

The Material Library browser previews the source size and prospective packed
payload size for every valid material. Its summary reports the complete pack
size, including header, entry table, names, and payloads, alongside the source,
stored-payload, and canonical-payload totals.

## Opening packs on macOS

The native application registers `.pwlib` as a viewable Paperweight pack, so a
pack can be opened from Finder or through **File > Open**. The read-only Pack
Inspector shows each entry's friendly name, UID, storage mode, stored and
canonical sizes, and checksum. Selecting an entry and supplying a seed
instantiates it as an ordinary unsaved material in the full editor. The pack
itself is never edited and its source folder remains authoritative.

Finder also opens `.pmat` files directly in the normal editable document
window. The usual unsaved-changes confirmation applies in both routes.

`.pwlib` version 1 is intentionally not promised as Paperweight's permanent
game format. The Blastard prototype should teach us what must become stable for
v0.1.0: algorithm versioning, caching, asynchronous work, and runtime resolution
policy remain separate concerns.
