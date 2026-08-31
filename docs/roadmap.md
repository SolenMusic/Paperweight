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

Potential additions include cracks, erosion, edge wear, pitting, scratches,
dirt, moss, rust, wood grain and knots, marble veins, pebbles, and terrain
erosion.

## v0.0.9 - Performance

Profile before optimising. Candidate improvements include multithreaded CPU
generation, SIMD, tile-based evaluation, cached intermediate nodes, incremental
preview regeneration, and later GPU investigation. GPU choices must not dictate
the core material model.

## v0.1.0 - Game Library

Stabilise the embedding API and add material-definition versioning, generator
algorithm versioning, texture caches and hashes, runtime resolution selection,
and asynchronous generation. Games should be able to distribute compact
definitions and generate hardware-appropriate maps at installation, first
launch, load time, or during development-time asset processing.

## Unscheduled future work

An Emscripten/WebAssembly portability proof remains planned but intentionally
does not displace the numbered roadmap above.
