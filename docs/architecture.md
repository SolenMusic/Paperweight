# Architecture

## Goal

Paperweight separates a reusable procedural-material engine from platform frontends. The engine is portable C++20 and owns all deterministic generation, image data, material definitions, parsing, and serialisation.

## Components

### Portable core

Located under `include/paperweight/` and `src/core/`.

Constraints:

- C++20 only.
- No AppKit, Foundation, Objective-C, Objective-C++, Metal, Vulkan, or platform file APIs.
- No unnecessary third-party dependencies.
- Stable, explicit data ownership.
- Deterministic results for identical definitions, seeds, and dimensions.
- Algorithms designed for native compilation and a later Emscripten/WebAssembly target.
- Seamlessness achieved by periodic algorithms and coordinate wrapping.

The initial image representation is tightly packed, opaque-capable RGBA8. It
is deliberately exposed through an image abstraction rather than baked into
the generator API, leaving room for greyscale, higher precision, floating-point,
and non-colour data formats later.

The generator combines platform-stable integer hashing, periodic 2D value
noise, and normalised periodic FBM. In v0.0.2, colour, height, tangent-space
normal, and roughness outputs all derive from that one scalar source. Texture
samples are taken at pixel centres over one mathematical period. Normal-map
finite differences wrap both axes; no output copies or repairs image edges.

`GenerationRequest::output` selects one portable RGBA8 result. The colour map
interpolates two RGBA endpoints, height and roughness use explicit linear
greyscale encodings, and the normal map encodes a normalised tangent-space XYZ
vector. This is an output contract, not a claim of a complete PBR material
model. Higher precision and additional pixel formats remain possible behind
the image abstraction.

### macOS frontend

Located under `app/macos/`.

Constraints:

- Native AppKit user interface.
- Objective-C++ is restricted to the bridge between AppKit and the C++ core.
- The frontend owns windows, controls, file panels, and presentation.
- It contains no procedural-generation algorithms.

The first preview is rendered through `NSBitmapImageRep`. This conversion lives
in Objective-C++ and consumes the portable core's RGBA8 buffer without exposing
AppKit types to the core. The same bridge encodes PNG data. Native file panels
handle paths, while parsing and serialisation remain in portable C++.

### Material format

`.pmat` is a human-readable, versioned text format. Parsing and serialisation live in the portable core. Round trips should be stable and errors should identify useful source locations.

The format version is deliberately independent of the application version.
Paperweight v0.0.2 reads and writes `.pmat` format version 1. Unknown keys and
unsupported format versions fail explicitly instead of being silently ignored.
See [pmat-format.md](pmat-format.md).

## Initial data flow

```text
.pmat text -> parser -> material model -> periodic FBM source
                    ^                         |
                    |              +----------+----------+---------+
                serialiser         |          |          |         |
                                  colour    height     normal   roughness
                                    |          |          |         |
                                    +----------+----------+---------+
                                                   |
                                             preview / PNG
```

## Deferred architecture

Layers, warping, specialised generators, material graphs, WebAssembly
bindings, game-engine adapters, complete PBR authoring, and GPU backends remain
outside v0.0.2.
