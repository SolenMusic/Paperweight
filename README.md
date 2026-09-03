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

v0.0.24 is the current Metals and Reflections release. The portable CPU core now
generates a fifth deterministic metalness map, and `.pmat` version 17 stores
metalness endpoints, per-layer metalness routing, and dielectric index of
refraction. Metalness and IOR do not reinterpret the authored colour image, so
all exported maps remain deterministic across worker counts and architectures.

The presentation-only MetalKit preview now uses a metallic/roughness lighting
model with dielectric F0 derived from IOR, coloured metallic reflections, and a
procedural studio environment. Chrome Studio, Brushed Metal, Ceramic, Wet
Surface, and Neutral inspection presets make reflective response visible without
placing environment images or GPU results in the portable material definition.
Polished Chrome, Brushed Steel, Polished Copper, Polished Brass, Painted Steel,
and Corroded Metal are bundled editable templates with Material Design Wizard
controls.

The v0.0.23 Surface Channel Authoring work remains available. Layers can target
colour, height (and its derived normal), roughness, and now metalness
independently. A material
may declare physical relief depth in metres, making normal-map slopes independent
of export resolution while correctly accounting for real-world repeat size;
normal strength remains an
optional artistic multiplier. Existing levels, threshold, posterise, surface
filters, transforms, and masks now operate on whichever output branches a layer
targets. Minimum, maximum, and centred detail composites add non-destructive
surface combination tools, while a constant Surface Value layer provides clear
height and roughness baselines.

The editor and Material Design Wizard expose the new controls directly. Polished
Marble, Wet Mortar, Engraved Metal, and Varnished Wood demonstrate independently
authored polish, wet joints, cut relief, brushing, and clear coat. Existing
version-15 materials retain their byte-exact output through a dedicated legacy
graph path; older material files migrate explicitly to readable version-17 `.pmat`.

The v0.0.22 Library Workspace remains intact. Paperweight opens on the
full material library instead of manufacturing an unnamed editor document.
Every opened material has an independent editing session and appears as a
native macOS tab alongside the library. Tabs can be dragged out or moved with
**Window > Move Tab to New Window** for a multi-display workflow, then merged
again without copying or losing document state.

Each editor also has a collapsible, searchable library navigator. Double-click
another material to open it, use **Library Overview** to return to the complete
browser, or toggle the navigator with **View > Show/Hide Library Navigator**.
Opening an already-open `.pmat` selects its existing editor. Closing an editor
removes that live document identity, so library rename and application teardown
cannot be confused by a stale hidden window.

The v0.0.21 Portable Material Library Packs work remains available. **Tools >
Material Library > Export Pack** compiles an entire working folder or selected
materials into one deterministic `.pwlib` deployment blob. The portable C++20
reader lists entries directly from caller-owned memory, retrieves by canonical
UID, and instantiates an ordinary material with the game's chosen seed. The
source `.pmat` files remain the editable truth.

Finder recognises both formats. Opening a `.pmat` selects or creates an editable
material tab. Opening a `.pwlib` presents a read-only Pack Inspector with
entry names, UIDs, storage modes, sizes, checksums, and chosen-seed
instantiation into an ordinary new editor document.

Each pack has a versioned little-endian directory, friendly names, entry and
whole-library checksums, and canonical PMAT payloads. Bounded RLE is selected
per entry only when it is smaller than raw storage. The `paperweight_pack`
command-line tool accepts folders or individual materials and can emit a C/C++
byte-array header for direct executable embedding.

The v0.0.20 Material Design Wizard remains available through **File > New
Material**. It opens a four-step native workflow for choosing a material family
and seedless starting recipe, setting the real-world repeat size, adjusting friendly
construction, surface, colour, and wear controls, and comparing deterministic
seeded alternatives. Any choice can be locked before variation. The wizard has
live 2D and five-map 3D preview and finishes either in the complete layer editor
or directly in the remembered working-folder library.

Family choices describe the actual material rather than incidental details:
Metal includes Painted Metal, Weathered Metal, Engraved Metal, chrome, steel,
copper, brass, painted steel, and corroded metal, while proven wood, marble,
pebble, debris, and abstract showcases supplement the ten v0.0.18 reference
templates where appropriate.

Wizard results are normal editable `Material` values and human-readable
version-17 `.pmat` files. The wizard adds no evaluator, hidden recipe state, or
new serialisation format. The v0.0.19 working-folder browser, metadata,
diagnostics, safe file operations, and selectable 64x64 through 1024x1024
preview resolution remain available.

The staged work is tracked in [GitHub Issues](https://github.com/SolenMusic/Paperweight/issues).

## Architectural boundary

```text
Portable C++20 core
  image buffers, deterministic primitives, periodic noise,
  structural, course-layout, sculpting, shape, lattice, scatter, organic, and surface generators,
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

See [docs/architecture.md](docs/architecture.md),
[docs/metals-and-reflections.md](docs/metals-and-reflections.md),
[docs/organic-structures.md](docs/organic-structures.md),
[docs/reference-materials.md](docs/reference-materials.md),
[docs/material-wizard.md](docs/material-wizard.md),
[docs/material-library.md](docs/material-library.md),
[docs/pwlib-format.md](docs/pwlib-format.md), and
[docs/roadmap.md](docs/roadmap.md).
Performance methodology and reproducible baseline measurements live in
[docs/performance.md](docs/performance.md).

## Current scope

The native editor retains its approachable layer interface. **File > New
Material** and the library's **New Material** button open the guided wizard;
**Edit Material** hands the finished ordinary recipe to that complete editor.
Each edit compiles the stack into a fresh graph and renders only the requested output
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
fasteners, masonry corner variation, cel courtyard gravel, scattered debris,
an initial foliage population, cel forest bark, and layered castle foliage.
The metal collection adds polished chrome, brushed steel, copper, brass,
scratched painted steel, and corroded metal.
File > New from Reference Template exposes the ten Blastard targets as seedless,
caller-instantiated recipes with compact material-specific controls. The older
Showcase menu remains available for direct access to the wider example set.
Region Stones demonstrates repeatable
per-cell variation, while Course Random gives every row one stable value.

Format-version-1 through version-17 `.pmat` files open. Versions 1 through 16
retain their historical output intact; saving migrates them to canonical format
version 17. Version 16 adds output routing and optional physical relief without
changing the legacy default. Version 17 adds metalness routing and dielectric
optics; older files migrate to zero metalness and IOR 1.5. Exact block
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
tools/                Portable-library command-line packer
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

In the app, use File > New Material to create a physically scaled material with
the guided workflow, File > Open to load a `.pmat` definition or inspect a
`.pwlib` pack, or File > New from Showcase to start from an advanced editable
recipe. Both file types also open directly from Finder. File > Save or Save As
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
Add Organic Cells for stretched bark-like plates, Organic Cracks for periodic
trunks and seeded branch hierarchies, Leaf Clusters for analytic foliage, and
Organic Accumulation for moss, lichen, or colour growth driven by cavities,
boundaries, height, or the authored scalar. Species presets fill a complete
editable leaf recipe but deliberately preserve the material seed.
Add Instance Scatter to place an analytic shape as a torus-safe population.
The native controls cover candidate density and jitter, spacing and overlap,
population creation and selection, scale/aspect/rotation ranges, colour pairs,
height and roughness ranges, stamp kind and size, and density/exclusion masks.
See [the `.pmat` format reference](docs/pmat-format.md) for the text format.

Use File > New from Reference Template to instantiate one of the ten reference
families with the seed currently shown in the editor. Its friendly controls
appear above the ordinary material settings; the full layer inspector remains
available for detailed work. Open Reference Image displays a PNG or BMP beside
the generated result without embedding that image or its path in the material.
Enable Optional baked presentation to generate a separate colour preview from
the unlit colour and normal/height maps. Azimuth, elevation, diffuse bands,
highlight threshold, and ambient contribution are adjustable, and exporting
while enabled writes the clearly named baked result. See
[the reference-material guide](docs/reference-materials.md).

Choose 3D under Preview mode to inspect all five material maps together. Select
a plane, sphere, cube, or cylinder; drag the object to orbit and scroll to zoom.
The map switches isolate colour, displacement, normal detail, roughness, and
metalness.
Height and Normal adjust their display strength without modifying the material
definition or exported maps. Lighting presets, manual phase, and Play Light make
surface response visible from several directions. Loading a material chooses a
bounded starting displacement from its authored normal strength; the Height
control remains a free visual override. Enable Toon lighting for a display-only
cel-shaded inspection; its bands, highlight threshold, and rim light never alter
saved or exported material data.

The ordinary preview path uses metallic/roughness PBR. Base colour contributes
diffuse reflection only where metalness is zero and tints specular reflection
where metalness is one; intermediate values support transitions such as worn
paint and corrosion. Dielectric F0 is calculated from the saved IOR. Procedural
studio environments, their rotation and intensity, direct-light controls, and
all GPU arithmetic remain inspection state only.

Use Tools > Performance Benchmark (Command-Option-B) to run a visible benchmark
suite at 64, 128, 256, 512, and 1024 pixels. Choose Colour, Height, Normal,
Roughness, or Metalness before starting. The separate window shows timings,
throughput,
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
five material outputs deliberately select different sources. Layer-oriented
callers can continue using `GenerationRequest` exactly as before; the core
compiles their material once per request. Set `physicalCoverage` when a caller
needs more than the material's single default repeat.

`paperweight_pack` compiles library-ready PMAT sources for game deployment:

```sh
build/tools/paperweight_pack -o game-materials.pwlib path/to/material-library
```

Add `--cpp-header paperweight_materials.h --symbol paperweight_materials` for a
C/C++ byte array. `examples/embed_pwlib.cpp` shows how Blastard or another game
can enumerate that memory, choose a UID and seed, and call the unchanged
generator API. The pack format is still provisional; v0.1.0 will stabilise the
lessons learned from an actual game integration.

The Material Library table previews each source PMAT size and the storage mode
and payload size it would occupy in the current pack. Its summary shows the
source total, complete PWLIB size including directory overhead, stored and
canonical payload totals, and RLE entry count before export.

## Licence

Paperweight is available under the [MIT License](LICENSE).
