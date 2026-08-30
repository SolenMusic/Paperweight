# `.pmat` format version 1

Paperweight material files are UTF-8 text. They are intended to be readable,
diffable, and small enough to embed alongside game assets.

## Canonical form

```text
# Paperweight procedural material
pmat.version = 1
material.type = fbm
material.seed = 18431
noise.frequency = 4
noise.octaves = 5
noise.lacunarity = 2
noise.gain = 0.5
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
- Every key is required and may appear only once.
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
| `noise.frequency` | Base lattice frequency | Integer from 1 to 64 |
| `noise.octaves` | FBM octave count | Integer from 1 to 8 |
| `noise.lacunarity` | Frequency multiplier per octave | Integer from 1 to 4 |
| `noise.gain` | Amplitude multiplier per octave | Decimal from 0.1 to 0.9 |

The combination of frequency, octaves, and lacunarity must keep every lattice
period at or below 4096. This is the same validation used by the generator.

## Compatibility policy

The `.pmat` format version and Paperweight application version are separate.
Paperweight v0.0.2 supports format version 1. A reader rejects unsupported
versions and unknown fields so that it cannot quietly reinterpret a future
material. A future compatible extension can define a new version and an
explicit migration path.

The portable entry points are `paperweight::parsePmat` and
`paperweight::serialisePmat` in `include/paperweight/pmat.hpp`.
