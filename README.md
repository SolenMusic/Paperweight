# Paperweight

*A deliberately lightweight procedural material generator. Yes, the name is intentional.*

Paperweight is planned as a deterministic, seamless procedural material generator with:

- a portable C++20 core library;
- a native macOS AppKit editor, using Objective-C++ only as platform glue;
- human-readable `.pmat` material definitions;
- mathematically seamless tiling;
- seeded, reproducible output;
- a design that can later compile to WebAssembly and embed in games such as Blastard.

## Status

v0.0.3 is the current Layers release. An ordered stack now combines reusable
noise, solid-colour, levels, and threshold operations with blend, add, and
multiply compositing. Every layer has its own enabled state and opacity. Colour,
height, tangent-space normal, and roughness continue to derive from the same
deterministic, seamless evaluation result.

The staged work is tracked in [GitHub Issues](https://github.com/SolenMusic/Paperweight/issues).

## Architectural boundary

```text
Portable C++20 core
  image buffers, deterministic primitives, periodic noise,
  reusable layer evaluation, generator API, .pmat parsing/serialisation
             |
             +-- Native builds and game embedding
             +-- Future WebAssembly build
             |
Native macOS AppKit frontend
  Objective-C++ glue only; no material-generation logic
```

See [docs/architecture.md](docs/architecture.md) and [docs/roadmap.md](docs/roadmap.md).

## Current scope

The current release adds a native layer editor with add, remove, reorder,
enable/disable, composite-mode, opacity, and operation-specific controls. The
portable evaluator applies the same stack for all four material outputs, and
format-version-2 `.pmat` files preserve it exactly. Format-version-1 files still
open and retain their output; saving migrates them to the current format.

Masks and warping, structural generators, a material graph, physical scale,
advanced surface tools, performance work, and the stable game library follow
in the documented roadmap.

## Repository layout

```text
include/paperweight/  Public portable C++ API
src/core/             Portable C++ implementation
app/macos/            Native AppKit frontend and Objective-C++ glue
tests/                Automated core and format tests
examples/materials/   Example .pmat definitions
docs/                 Architecture and roadmap
```

## Principles

- Determinism is a feature, not an accident.
- Seamlessness must be mathematical, not repaired after generation.
- The core must remain independent of Apple frameworks and UI concerns.
- Dependencies must earn their weight. Paperweight should not become one.

## Build and test

Paperweight requires CMake 3.24 or newer and a C++20 compiler. On macOS, the
minimum deployment target is macOS 11 Big Sur; both Apple Silicon and Intel are
supported.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The native application is produced at:

```text
build/app/macos/Paperweight.app
```

Open it in Finder or run:

```sh
open build/app/macos/Paperweight.app
```

In the app, use File > Open to load a `.pmat` definition, File > Save or Save As
to store one, build a stack in the Layer Stack panel, choose a material output,
and use File > Export PNG to write that 512x512 RGBA8 tile. The stack is shown
bottom-to-top, matching evaluation order.
See [the `.pmat` format reference](docs/pmat-format.md) for the text format.

For a universal Intel and Apple Silicon build:

```sh
cmake -S . -B build-universal \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal
ctest --test-dir build-universal --output-on-failure
```

The project currently uses a tiny in-repository test harness and has no
third-party dependencies. See [CONTRIBUTING.md](CONTRIBUTING.md) for repository
conventions.

## Licence

Paperweight is available under the [MIT License](LICENSE).
