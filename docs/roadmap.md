# Roadmap

## v0.0.1 - Foundations and native preview

Delivered a portable deterministic core and a small native macOS editor with
live controls and 1x1/3x3 seamless previews.

## v0.0.2 - Material files and export

Add the versioned `.pmat` parser and canonical serialiser, native open/save and
unsaved-change protection, PNG export, cross-platform CI, and the corresponding
format and usage documentation.

The GitHub milestone and issues are the authoritative implementation backlog.

## Near term after v0.0.2

- Harden the public API and `.pmat` compatibility policy.
- Add performance benchmarks and profiling targets.
- Establish an Emscripten/WebAssembly build and a minimal JavaScript bridge.
- Add further general-purpose procedural operators.
- Explore multiple output channels before committing to a PBR workflow.

## Explicitly deferred

Metal and Vulkan rendering, node graphs, PBR authoring, specialised brick or Voronoi generators, and direct Blastard/game integration are not part of v0.0.2.
