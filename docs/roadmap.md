# Roadmap

## v0.0.1 - Foundations and native editor

Deliver a portable deterministic core and a small native macOS editor capable of loading, editing, previewing, saving, and exporting a simple seamless procedural material.

The GitHub milestone and issues are the authoritative implementation backlog.

## Near term after v0.0.1

- Harden the public API and `.pmat` compatibility policy.
- Add performance benchmarks and profiling targets.
- Establish an Emscripten/WebAssembly build and a minimal JavaScript bridge.
- Add further general-purpose procedural operators.
- Explore multiple output channels before committing to a PBR workflow.

## Explicitly deferred

Metal and Vulkan rendering, node graphs, PBR authoring, specialised brick or Voronoi generators, and direct Blastard/game integration are not part of v0.0.1.
