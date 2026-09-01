# Roadmap

This is the canonical Paperweight product roadmap. GitHub milestones mirror
these version boundaries; detailed implementation issues are expanded as a
version becomes active.

## v0.0.1 - Procedural Foundation

- Portable C++ core and image buffer.
- Deterministic hashing/randomness, periodic 2D noise, and FBM.
- Basic two-colour material with guaranteed seamless generation.
- Native AppKit editor with seed, noise, and two colour controls.
- Live 1x1 and 3x3 previews.
- `.pmat` load/save, PNG export, and basic tests.

Success means the application can create, save, reopen, and export seamless
noise-based materials, while the same `.pmat` definition generates identical
output through the standalone C++ library.

## v0.0.2 - Material Outputs

Introduce height, normal, and roughness maps with native controls for previewing
each output. Normals derive from height, and all maps derive from the same
procedural source.

## v0.0.3 - Layers

Introduce a layer stack with noise, solid colour, add, multiply, blend, levels,
and threshold. Every layer can be enabled independently. Begin separating
procedural operations into reusable evaluation objects.

## v0.0.4 - Masks and Warping

Add masks plus coordinate scale, offset, rotation, and warp/distortion.

## v0.0.5 - Structural Generators

Add brick and tile grids, Voronoi/Worley and random cells, lines, rectangles,
and circles. Good procedural brick walls and cobblestones should become
practical here.

## v0.0.6 - Material Graph

Compile the simple layer model into explicit generator, mask, output, and
processing nodes, with composites represented as processing operations.
Validate identifiers, connections, output routes, and acyclic topology before
evaluation. Portable callers may provide a direct branched graph, while the
editor continues exposing its simpler layer-oriented view. A visual node canvas
and graph-specific file syntax are later work.

## v0.0.7 - Physical Scale

Add material-space dimensions and caller-selected physical coverage so
materials retain consistent world scale across output resolutions. One material
repeat has an explicit metre width and height; requested coverage contains whole
repeats to preserve mathematical seamlessness. Brick width, height, and mortar
may be authored directly in metres.

## v0.0.8 - Advanced Surface Tools

Add a compact reusable vocabulary rather than one bespoke generator per named
material: periodic ridged noise, bands, rings, scatter, and streak patterns,
plus invert, soften, expand, contract, edge, slope, cavity, and peaks filters.
Filters evaluate their graph input over a seamless material-space
neighbourhood. Cracks, erosion, pitting, scratches, dirt, moss, rust, wood grain
and knots, marble veins, pebbles, and terrain erosion become editable showcase
recipes - and later seedless templates - composed from these operations.

## v0.0.9 - Performance

Profile before optimising, then improve the CPU path under a byte-identical
output requirement. v0.0.9 adds reproducible command-line and native-app
benchmarks, cross-architecture golden tests, a resolved evaluator execution
plan, and bounded deterministic multithreaded row generation. The native tool
compares one and multiple workers across every bundled recipe from 64 x 64 to
1024 x 1024, displays results live, and exports machine-labelled CSV. Profiling
did not justify strict SIMD or a potentially enormous neighbourhood-sample
cache in this release; both remain evidence-led future work. Optional Vulkan
and MoltenVK research is tracked separately; GPU choices must not dictate the
core material model or add dependencies to a CPU-only library build.

## v0.0.10 - 3D Material Preview

Add a native MetalKit 3D preview with plane, sphere, cube, and cylinder inspection
shapes. It combines colour, height, normal, and roughness maps, while allowing
each map to be isolated. Camera orbit and zoom, lighting presets and controls,
manual light phase, and configurable automatic animation are included. Map
generation stays cancellable and off the UI thread; only the newest complete
four-map set is published. Metal is presentation-only and remains outside the
portable core. A compatible Metal device enables 3D; the full 2D editor remains
available otherwise.

## v0.0.11 - Stylised Materials

Add portable posterisation, smooth and stepped colour ramps, deterministic
palette reduction, edge-aware smoothing, and periodic ink contours. Processing
can target colour, scalar surface data, or both; colour-only filters preserve
height, normal, and roughness outputs exactly. The AppKit editor exposes native
controls for each operation and includes editable toon-dungeon, painted-metal,
and graphic-marble showcases. The MetalKit preview gains optional display-only
cel lighting with discrete diffuse bands, a hard highlight threshold, and rim
light. The preview effect never enters `.pmat` data or exported maps.

## v0.1.0 - Game Library

Stabilise the embedding API and add material-definition versioning, generator
algorithm versioning, texture caches and hashes, runtime resolution selection,
and asynchronous generation. Games should be able to distribute compact
definitions and generate hardware-appropriate maps at installation, first
launch, load time, or during development-time asset processing.

## Unscheduled future work

- Working-folder material libraries with stable IDs, friendly names, and
  seedless templates built from ordinary recipe properties.
- A measured packed-library export for games, while readable `.pmat` files
  remain the source of truth.
- Optional Git assistance layered over a working folder.
- An Emscripten/WebAssembly portability proof.

These remain planned but intentionally do not displace the numbered roadmap
above.
