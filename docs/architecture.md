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

### macOS frontend

Located under `app/macos/`.

Constraints:

- Native AppKit user interface.
- Objective-C++ is restricted to the bridge between AppKit and the C++ core.
- The frontend owns windows, controls, file panels, and presentation.
- It contains no procedural-generation algorithms.

### Material format

`.pmat` is a human-readable, versioned text format. Parsing and serialisation live in the portable core. Round trips should be stable and errors should identify useful source locations.

## Initial data flow

```text
.pmat text -> parser -> material model -> generator -> image buffer
                    ^                         |
                    |                         v
                serialiser              preview / PNG
```

## Deferred architecture

WebAssembly bindings, game-engine adapters, GPU backends, node graphs, PBR channel sets, and specialised material generators are deliberately outside v0.0.1.
