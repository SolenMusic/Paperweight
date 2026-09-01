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

v0.0.15 is the current Shape Primitives and Lattices release. Portable analytic
rounded rectangles, ellipses, capsules, diamonds, and convex polygons can be
repeated, inset, outlined, bordered, and combined through mask Boolean
operations. Each bounded instance may rotate freely. Global line and diamond
lattices instead use explicit integer windings, which guarantees that they
remain mathematically seamless. Four editable showcases construct a castle
window, a detailed crate, reusable fasteners, and masonry corner ornament from
the shared vocabulary, while older material definitions retain their historical
output.

The staged work is tracked in [GitHub Issues](https://github.com/SolenMusic/Paperweight/issues).

## Architectural boundary

```text
Portable C++20 core
  image buffers, deterministic primitives, periodic noise,
  structural, course-layout, sculpting, shape, lattice, and surface generators,
  exact region identity and fields,
  neighbourhood filters,
  layer-to-graph compiler, graph validation/evaluation,
  transforms, masks, generator API, .pmat parsing/serialisation
             |
             +-- Native builds and game embedding
             +-- Future WebAssembly build
             |
Native macOS AppKit frontend
  Objective-C++ glue only; no material-generation logic
```

See [docs/architecture.md](docs/architecture.md) and [docs/roadmap.md](docs/roadmap.md).
Performance methodology and reproducible baseline measurements live in
[docs/performance.md](docs/performance.md).

## Current scope

The native editor retains its approachable layer interface. Each
edit compiles the stack into a fresh graph and renders only the requested output
branch; the preview status shows the resulting node count. Structural controls,
transforms, warp, masks, output selection, 1x1/3x3 tiling, open/save, and PNG
export behave as before. Preview work remains cancellable and off AppKit's event
thread, and continuous sliders retain direct mouse tracking. The visible layer
stack matches bottom-to-top evaluation. Compact repeat-size and preview-coverage
controls use metres, while brick layers can switch between relative sizing and
friendly metre fields. Physical bricks and course layouts expose their derived
counts alongside dimensions and calculate a compatible seamless repeat; authors
never need to guess which dimensions divide an existing repeat. File > New from
Showcase loads editable realistic and stylised recipes, including toon dungeon
stone, painted metal, graphic marble, castle flagstone, two castle masonry
styles, castle roof slates, cel forest rock, sculpted flagstone, worn masonry,
chamfered roof slate, a diamond castle window, a detailed crate, decorative
fasteners, and masonry corner variation. These are the practical forerunners of the
planned seedless template library. Region Stones demonstrates repeatable
per-cell variation, while Course Random gives every row one stable value.

Format-version-1 through version-11 `.pmat` files still open with their
historical output intact; saving migrates them to canonical format version 12.
Version 12 adds shape primitives, shape Boolean processors, and seamless
lattices. Exact block
and parent-course keys are evaluator metadata, not pixel channels, so they are
never stored in lossy floating-point form.
The editor does not invent a redundant graph syntax for the existing layer recipe:
direct graph construction is currently a portable C++ API, while visual graph
authoring and graph-specific persistence remain later editor work.

The native 3D material preview is intentionally presentation-only. It uses
MetalKit without introducing Apple APIs into `libpaperweight`; a Mac without a
compatible Metal device retains the complete 2D editor. Optional
Vulkan/MoltenVK research remains separate from ordinary `libpaperweight` builds.

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

In the app, use File > Open to load a `.pmat` definition, File > New from
Showcase to start from an advanced editable recipe, File > Save or Save As
to store one, build a stack in the Layer Stack panel, choose a material output,
and use File > Export PNG to write that 512x512 RGBA8 tile. Select a layer and
use the Layer, Transform, and Mask tabs to shape it. Structural operations show
their own controls in the Layer tab. The stack is shown bottom-to-top, matching
evaluation order. Repeat Size is stored with the material; Coverage controls how
many whole physical repeats the preview generates. Add Region Field immediately
above a brick, tile, Worley, random-cell, line, rectangle, or circle layer to
select seeded random variation, local coordinates, or centre/boundary distance.
Add Course Layout for variable masonry, large slabs, or roof slates; choose
Block Faces, Mortar, Course Interiors, or Overlap as the generated field and
switch Physical Dimensions on when dimensions should be authored in metres.
Add Region Surface immediately above any structural region to construct height
from its boundary. Select a bevel profile, then tune facets, centre peak, slope,
chips, wear, and erosion; the output selector can expose the resulting masks.
Planar normals are opt-in and do not alter the saved colour, height, or roughness.
Add Shape Primitive for repeated rounded rectangles, ellipses, capsules,
diamonds, or an editable regular convex polygon. Fill, inset, outline, and border
fields expose the same analytic boundary. Shape Boolean combines one of those
local shapes with the accumulated mask, while Seam-safe Lattice creates line or
diamond grids from whole-tile winding counts. Local shape rotation is free;
lattice direction remains restricted to repeat-compatible windings.
See [the `.pmat` format reference](docs/pmat-format.md) for the text format.

Choose 3D under Preview mode to inspect all four material maps together. Select
a plane, sphere, cube, or cylinder; drag the object to orbit and scroll to zoom.
The map switches isolate colour, displacement, normal detail, and roughness.
Height and Normal adjust their display strength without modifying the material
definition or exported maps. Lighting presets, manual phase, and Play Light make
surface response visible from several directions. Loading a material chooses a
bounded starting displacement from its authored normal strength; the Height
control remains a free visual override. Enable Toon lighting for a display-only
cel-shaded inspection; its bands, highlight threshold, and rim light never alter
saved or exported material data.

Use Tools > Performance Benchmark (Command-Option-B) to run a visible benchmark
suite at 64, 128, 256, 512, and 1024 pixels. Choose Colour, Height, Normal, or
Roughness before starting. The separate window shows timings, throughput,
speed-up, and byte-identity for each material and size as they finish; it also
supports cancellation and copy/save CSV for comparisons between machines. A
complete run deliberately measures the slow reference path and can take several
minutes.

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

Set `PAPERWEIGHT_ENABLE_THREADS=OFF` for an explicitly serial core build. The
option defaults off under Emscripten and on for native builds. A request's
`workerCount` may force one or more workers; zero selects the automatic policy.

`examples/generate_graph.cpp` demonstrates a direct branched graph in which the
four material outputs deliberately select different sources. Layer-oriented
callers can continue using `GenerationRequest` exactly as before; the core
compiles their material once per request. Set `physicalCoverage` when a caller
needs more than the material's single default repeat.

## Licence

Paperweight is available under the [MIT License](LICENSE).
