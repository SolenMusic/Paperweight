# `.pmat` format version 1

Paperweight material files are UTF-8 text. They are intended to be readable,
diffable, and small enough to embed alongside game assets.

## Canonical form

```text
# Paperweight procedural material
pmat.version = 1
material.type = fbm
material.seed = 18431
colour.low = 0x000000FF
colour.high = 0xFFFFFFFF
noise.frequency = 4
noise.octaves = 5
noise.lacunarity = 2
noise.gain = 0.5
normal.strength = 1
roughness.low = 0.25
roughness.high = 0.85
```

The serialiser always writes this key order with Unix line endings and one
trailing newline. It uses a locale-independent decimal representation that
round-trips the material's values exactly.

## Syntax

- Each non-empty line contains one `key = value` assignment.
- Spaces and tabs around a key, equals sign, or value are ignored.
- A `#` starts a comment that continues to the end of the line.
- LF and CRLF line endings are accepted.
- Keys may appear in any order when reading.
- Every canonical key may appear only once. The two colour keys and the three
  material-output keys are optional only when reading older files; canonical
  output always writes them. All other keys are required.
- Unknown keys are errors in format version 1.
- Input must be valid UTF-8 when opened by the Mac app.

Malformed input produces a diagnostic with a one-based line and column plus a
plain-language reason. Parsing either produces one complete valid material or
no material; it never returns a partially accepted definition.

## Fields

| Key | Meaning | Accepted value |
| --- | --- | --- |
| `pmat.version` | File-format version | `1` |
| `material.type` | Generator model | `fbm` |
| `material.seed` | Deterministic seed | Unsigned 64-bit integer |
| `colour.low` | Colour at scalar value zero | `0xRRGGBBAA` hexadecimal |
| `colour.high` | Colour at scalar value one | `0xRRGGBBAA` hexadecimal |
| `noise.frequency` | Base lattice frequency | Integer from 1 to 64 |
| `noise.octaves` | FBM octave count | Integer from 1 to 8 |
| `noise.lacunarity` | Frequency multiplier per octave | Integer from 1 to 4 |
| `noise.gain` | Amplitude multiplier per octave | Decimal from 0.1 to 0.9 |
| `normal.strength` | Tangent-space normal slope multiplier | Decimal from 0 to 16 |
| `roughness.low` | Roughness at scalar value zero | Decimal from 0 to 1 |
| `roughness.high` | Roughness at scalar value one | Decimal from 0 to 1 |

The combination of frequency, octaves, and lacunarity must keep every lattice
period at or below 4096. This is the same validation used by the generator.
Colour channels are interpolated component-by-component in RGBA8 space. The
roughness endpoints may be reversed if an inverse relationship is wanted.

## Material outputs

Every v0.0.2 output derives from the same normalised periodic FBM scalar at the
same pixel centre:

- Colour interpolates between `colour.low` and `colour.high` in RGBA8.
- Height writes the scalar to R, G, and B as linear UNORM8, with alpha 255.
- Roughness interpolates between `roughness.low` and `roughness.high`, then
  writes linear greyscale UNORM8 with alpha 255.
- Normal uses wrapped central differences of the sampled height field. The
  tangent-space vector `(-dH/du * strength, -dH/dv * strength, 1)` is
  normalised, maps XYZ from `[-1, 1]` to RGB `[0, 255]`, and writes alpha 255.
  A strength of zero therefore encodes the neutral normal `(128, 128, 255)`.

The normal calculation wraps both axes before taking its differences; it does
not repair image edges afterwards. All scalar outputs use linear numeric
encodings. Colour management, HDR data, channel packing, and a complete PBR
workflow remain later work.

## Compatibility policy

The `.pmat` format version and Paperweight application version are separate.
Paperweight v0.0.2 supports format version 1. A reader rejects unsupported
versions and unknown fields so that it cannot quietly reinterpret a future
material. A future compatible extension can define a new version and an
explicit migration path.

Historical previews and v0.0.1 wrote format-version-1 files before some current
keys existed. The reader supplies black-to-white colour defaults, normal
strength 1, and roughness endpoints 0.25 and 0.85 when those fields are absent.
Canonical output always writes the complete current key set.

The portable entry points are `paperweight::parsePmat` and
`paperweight::serialisePmat` in `include/paperweight/pmat.hpp`.
