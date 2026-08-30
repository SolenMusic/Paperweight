# `.pmat` format version 2

Paperweight material files are UTF-8 text. They are intended to be readable,
diffable, and small enough to embed alongside game assets.

## Canonical form

```text
# Paperweight procedural material
pmat.version = 2
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
layers.count = 1
layer.0.enabled = true
layer.0.operation = noise
layer.0.composite = blend
layer.0.opacity = 1
layer.0.noise.seed_offset = 0
```

The serialiser writes the global keys first and layers in ascending order, with
Unix line endings and one trailing newline. Layer zero is the bottom of the
stack. It uses a locale-independent decimal representation that round-trips
every material value exactly.

## Syntax

- Each non-empty line contains one `key = value` assignment.
- Spaces and tabs around a key, equals sign, or value are ignored.
- A `#` starts a comment that continues to the end of the line.
- LF and CRLF line endings are accepted.
- Keys may appear in any order when reading.
- Each key may appear only once and unknown keys are errors.
- Input must be valid UTF-8 when opened by the Mac app.
- `layers.count` may be from 0 to 32. Every declared layer and each parameter
  required by its operation must be present.

Malformed input produces a diagnostic with a one-based line and column plus a
plain-language reason. Parsing either produces one complete valid material or
no material; it never returns a partially accepted definition.

## Global fields

| Key | Meaning | Accepted value |
| --- | --- | --- |
| `pmat.version` | File-format version | `2` |
| `material.type` | Generator model | `fbm` |
| `material.seed` | Deterministic seed | Unsigned 64-bit integer |
| `colour.low` | Low colour and threshold endpoint | `0xRRGGBBAA` hexadecimal |
| `colour.high` | High colour and threshold endpoint | `0xRRGGBBAA` hexadecimal |
| `noise.frequency` | Base lattice frequency for noise operations | Integer from 1 to 64 |
| `noise.octaves` | FBM octave count | Integer from 1 to 8 |
| `noise.lacunarity` | Frequency multiplier per octave | Integer from 1 to 4 |
| `noise.gain` | Amplitude multiplier per octave | Decimal from 0.1 to 0.9 |
| `normal.strength` | Tangent-space normal slope multiplier | Decimal from 0 to 16 |
| `roughness.low` | Roughness at scalar value zero | Decimal from 0 to 1 |
| `roughness.high` | Roughness at scalar value one | Decimal from 0 to 1 |
| `layers.count` | Number of ordered layers | Integer from 0 to 32 |

The combination of frequency, octaves, and lacunarity must keep every lattice
period at or below 4096. The roughness endpoints may be reversed if an inverse
relationship is wanted.

## Layer fields

Every layer `N` has these common keys:

| Key | Meaning | Accepted value |
| --- | --- | --- |
| `layer.N.enabled` | Whether evaluation includes this layer | `true` or `false` |
| `layer.N.operation` | Reusable evaluation operation | `noise`, `solid_colour`, `levels`, or `threshold` |
| `layer.N.composite` | How the result combines with accumulated input | `blend`, `add`, or `multiply` |
| `layer.N.opacity` | Composite amount | Decimal from 0 to 1 |

The operation selects exactly one parameter group:

| Operation | Required key | Accepted value |
| --- | --- | --- |
| `noise` | `layer.N.noise.seed_offset` | Unsigned 64-bit integer |
| `solid_colour` | `layer.N.solid.colour` | `0xRRGGBBAA` hexadecimal |
| `levels` | `layer.N.levels.input_low` | Decimal from 0 to 1 |
| `levels` | `layer.N.levels.input_high` | Decimal from 0 to 1 and greater than input low |
| `levels` | `layer.N.levels.gamma` | Decimal from 0.1 to 4 |
| `threshold` | `layer.N.threshold.value` | Decimal from 0 to 1 |

Noise seed offset zero reproduces the original material seed exactly. Other
offsets are deterministically mixed with the material seed, allowing multiple
independent noise layers without storing random state. Solid colour uses Rec.
709 luminance for its scalar value. Levels and threshold transform the
accumulated input; threshold selects `colour.low` or `colour.high`.

Blend interpolates between the accumulated and source values. Add sums the
opacity-scaled source and clamps it to the normalised range. Multiply scales the
accumulated value towards a full multiplication according to opacity. The same
formula is applied to scalar, red, green, blue, and alpha channels.

## Material outputs

Every v0.0.3 output derives from the same final layered sample at the same pixel
centre:

- Colour encodes the final RGBA channels.
- Height writes the final scalar to R, G, and B as linear UNORM8, with alpha 255.
- Roughness interpolates between `roughness.low` and `roughness.high` using the
  final scalar, then writes linear greyscale UNORM8 with alpha 255.
- Normal uses wrapped central differences of the final scalar field. The
  tangent-space vector `(-dH/du * strength, -dH/dv * strength, 1)` is
  normalised, maps XYZ from `[-1, 1]` to RGB `[0, 255]`, and writes alpha 255.

All procedural noise remains periodic, and scalar neighbours used by normal
generation wrap mathematically across both tile axes.

## Compatibility policy

The `.pmat` format version and Paperweight application version are separate.
Paperweight v0.0.3 reads versions 1 and 2 and writes version 2. A reader rejects
unsupported versions and unknown fields so that it cannot quietly reinterpret
a future material.

Version-1 files have an implicit base-noise evaluation. Paperweight preserves
that behaviour, including historical files that omit later colour, normal, or
roughness keys. The Mac editor presents an explicit base noise layer after
opening such a file; this is byte-identical and saving performs the migration
to version 2.

The portable entry points are `paperweight::parsePmat` and
`paperweight::serialisePmat` in `include/paperweight/pmat.hpp`.
