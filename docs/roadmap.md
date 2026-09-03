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

## v0.0.12 - Regions and Attributes

Implemented: give every structural region a stable integer identity, region-local
coordinates, boundary and centre distances, and independent seeded random
channels. Allow these fields to drive colour, scalar height, roughness,
parameters, and masks without changing region geometry. Identity must remain
independent of pixels, resolution, worker scheduling, architecture, and
container traversal. The native editor exposes the operation and includes a
Region Stones showcase. `.pmat` version 9 persists the field selection while
the exact integer identity stays evaluator metadata. This is the shared
foundation for all later layouts and instances. See
[roadmap issue #128](https://github.com/SolenMusic/Paperweight/issues/128).

## v0.0.13 - Masonry and Courses

Implemented: add variable-height courses, variable-width blocks, controlled staggering,
rectangular slab subdivision, crooked seamless boundaries, and overlapping
shingle/slate courses. Layouts expose reusable block, mortar, course, and
overlap fields and retain physical dimensions. Stable child and parent-course
keys allow block-level or whole-course variation without moving geometry. The
native editor includes complete relative/metre controls and four editable
castle showcases. This establishes the targeted 80-95 percent structural
foundation; v0.0.14 adds the bevels and facets needed for their final surface
character. See [roadmap issue #129](https://github.com/SolenMusic/Paperweight/issues/129).

## v0.0.14 - Bevels, Facets and Wear

Implemented: construct rounded, chamfered, and hand-cut height profiles from
stable region distance. Seeded planar facets, displaced centre peaks,
directional slopes, chipping, edge wear, and erosion are one reusable graph
processor rather than named rock or castle generators. The processor can emit
constructed height or cavity, outer-edge, exposed-face, facet, and wear masks.
An optional normal-only planar treatment leaves colour, height, and roughness
unchanged. The native editor exposes every control and includes cel-rock,
flagstone, masonry, and slate showcases. See
[roadmap issue #130](https://github.com/SolenMusic/Paperweight/issues/130).

## v0.0.15 - Shapes and Lattices

Implemented: add analytic rounded rectangles, ellipses, capsules, diamonds, and
convex polygons, together with fill, inset, outline, border, union,
intersection, and subtraction operations. Bounded repeated instances rotate
freely; global line and diamond lattices use signed integer winding directions
rather than pretending that every arbitrary rotation remains seamless. Stable
regions, graph nodes, canonical `.pmat` version 12 persistence, native controls,
and byte-exact single/multi-worker tests cover the complete vocabulary. Four
editable showcases target the diamond castle window, detailed crate, reusable
fasteners, and masonry corner variation. See
[roadmap issue #131](https://github.com/SolenMusic/Paperweight/issues/131).

## v0.0.16 - Deterministic Instance Scattering

Implemented: add mathematically tileable placement with minimum distance,
forbidden, controlled, or unrestricted overlap, multiple weighted populations,
stable occlusion order, and per-instance scale, aspect, rotation, colour,
height, and roughness. Candidate selection and rejection depend only on stable
hashes and candidate-centre masks, never pixels, workers, or container order.
Shape primitives are reusable stamps for gravel, leaves, nails, chips, and
debris. Canonical `.pmat` version 13, graph generators, native controls, a
bounded per-cell lookup, and byte-exact tests cover the complete contract.
Editable courtyard gravel, debris, and foliage-foundation showcases establish
the targets for later organic structures. See
[roadmap issue #132](https://github.com/SolenMusic/Paperweight/issues/132).

## v0.0.17 - Organic Structures

Implemented: anisotropic bark regions, branching crack networks, parametric
ovate, lanceolate, cordate, and lobed leaf silhouettes, midrib and vein masks,
stable overlapping clusters, four species presets, and moss or lichen
accumulation driven by cavities, boundaries, height, or authored masks. Named
bark and foliage results remain editable `.pmat` templates rather than special
evaluation paths. Canonical format version 14, graph integration, native
controls, benchmarks, golden maps, and cross-worker tests cover the complete
path. This targets cel bark at 90-98 percent and dense castle foliage at 80-95
percent.
See [roadmap issue #133](https://github.com/SolenMusic/Paperweight/issues/133).

## v0.0.18 - Reference Materials and Stylised Baking

Implemented: converge all branches into seedless, caller-instantiated templates
for the ten supplied Blastard reference materials, each backed by the ordinary
layer evaluator and a small set of typed high-level controls. Add side-by-side comparison and an
optional portable colour-output lighting bake with explicit direction, diffuse
bands, highlight threshold, and ambient contribution. Unlit maps
remain the default for dynamic game lighting. Every template receives golden
colour, height, normal, roughness, and baked outputs plus true 1x1/3x3 repeat,
worker, and cross-architecture acceptance. See
[roadmap issue #134](https://github.com/SolenMusic/Paperweight/issues/134) and
the [cross-version tracker #135](https://github.com/SolenMusic/Paperweight/issues/135).

The intended dependency shape is:

```text
v0.0.12  Regions and Attributes
    |-- v0.0.13  Masonry and Courses
    |       `-- v0.0.14  Bevels, Facets and Wear
    `-- v0.0.15  Shapes and Lattices
            `-- v0.0.16  Instance Scattering
                    `-- v0.0.17  Organic Structures

                 all feed into
v0.0.18  Reference Materials and Stylised Baking
```

These releases retain deterministic seeded output, mathematical seamlessness,
resolution-independent physical scale, byte-identical supported CPU output,
and portable dependency-free C++20 semantics. Detailed Codex-sized child issues
are expanded only when a milestone becomes active, allowing implementation
evidence to shape the local backlog without creating a second roadmap.

## v0.0.19 - Working Folder and Material Library

Implemented: choose and remember a working folder; recursively index readable
`.pmat` source files; browse generated thumbnails; search names, tags,
categories, descriptions, UIDs, and paths; and filter by category. Version-15
material metadata adds an optional stable UID, friendly name, description,
category, and tags without changing evaluation. Missing identity, duplicate
UIDs, malformed files, and missing friendly names are visible diagnostics and
do not prevent valid siblings from loading. Safe create, duplicate, friendly
rename, in-root move, open, reveal, explicit refresh, and external-change
refresh operations avoid silent overwrite. The editor also offers remembered
64, 128, 256, 512, and 1024 square preview resolutions across 2D, baked, and 3D
inspection. See [roadmap issue #201](https://github.com/SolenMusic/Paperweight/issues/201).

## v0.0.20 - Material Design Wizard

Implemented: add a native **New Material** workflow covering masonry, stone,
wood, metal, organic, foliage, gravel or debris, and abstract families. Authors
choose a compatible v0.0.18 seedless reference template or a proven bundled
recipe promoted to the same seedless model, select or enter the physical repeat
size, and adjust friendly construction, surface, colour, and wear controls
while watching live 2D or four-map 3D preview. Metal starts from actual Painted
Metal or Weathered Metal recipes rather than incidental metal details inside an
unrelated material. Seed, size, colour, and individual template properties may
be locked while generating four stable, deterministic alternatives for
comparison.

The chosen result can enter the complete layer editor or be given identity and
saved directly into the remembered working-folder library. Both routes produce
an ordinary editable version-15 `.pmat`; family, locks, comparison state, and
preview settings are deliberately transient. The portable wizard model maps
onto the existing recipe and typed template-control APIs, so there is no second
material engine or inaccessible state. See
[roadmap issue #211](https://github.com/SolenMusic/Paperweight/issues/211).

## v0.0.21 - Portable Library Packs

Implemented: compile an entire working folder or selected materials into one
deterministic `.pwlib` game blob. Version 1 has a strict little-endian header,
fixed entry table, canonical UIDs and friendly names, per-entry checksums, and a
whole-library checksum. Canonical PMAT payloads use bounded PackBits-style RLE
only when it is strictly smaller; raw storage remains available.

The dependency-free C++ reader operates on a caller-owned memory span, lists
entries, retrieves by UID, and instantiates a normal material with a chosen
seed. The command-line packer accepts folders or selected files and can emit a
C/C++ byte-array header. The native material-library window exports all or the
current multi-selection and previews per-entry and total source/packed sizes.
Finder opens `.pmat` in the editor and `.pwlib` in a read-only Pack Inspector;
an inspected entry can be instantiated with a chosen seed. Corruption,
truncation, incompatible versions,
duplicate identities, and directory/payload mismatch are rejected. Source and
pack generation are checked across every material output against a fixed pack
checksum on native and Intel test slices. Human-readable `.pmat` files remain
authoritative and suitable for Git. See [pwlib-format.md](pwlib-format.md).
The staged implementation is recorded in
[roadmap issue #224](https://github.com/SolenMusic/Paperweight/issues/224).

## v0.0.22 - Library Workspace

Implemented: launch into the full material library; give every open material an
independent editor session with its own state and cancellable preview work; and
place library and material editors in one native macOS tab group. Opening the
same canonical file selects the existing editor, while different materials may
be edited independently. Native tab detaching and merging preserve a useful
multi-monitor workflow.

Every editor includes a collapsible, searchable view of the remembered working
folder with direct material opening and a route back to the full library. Save,
Export, reference-image actions, dirty-document prompts, library rename guards,
Finder opening, and editor teardown all follow the currently selected material
rather than an obsolete singleton window. Closing the library does not destroy
open editors, and closing an editor cannot leave a stale open-document identity.
This release changes only the AppKit workspace: portable material evaluation,
PMAT/PWLIB formats, and generated bytes are unchanged. See
[roadmap issue #241](https://github.com/SolenMusic/Paperweight/issues/241).

Still out of scope here: visual node-graph editing, generators that bypass the
reusable operation vocabulary, texture caches, asynchronous generation,
runtime resolution policy, Git assistance, and GPU generation. The pack format
is expressly provisional until a Blastard integration has exercised it.

## v0.0.23 - Surface Channel Authoring

Implemented: route every layer independently to colour, height/normal, and
roughness while preserving the historical shared graph for legacy materials.
Add optional physical relief depth in metres and derive resolution-independent
normals from relief divided by material-space distance; retain normal strength
as a deliberate artistic multiplier.

Levels, threshold, inversion, smoothing, posterisation/terracing, masks, and
existing wear sources can now be applied to the chosen surface branch. Constant
Surface Value layers establish height or roughness baselines, while minimum,
maximum, and centred detail composites provide deterministic surface blending.
The editor and wizard expose these choices directly. Polished Marble, Wet
Mortar, Engraved Metal, and Varnished Wood are the acceptance recipes. PMAT
format 16 stores the new authoring state readably; checked-in version-15 golden
images remain byte-identical.

Tracked by [roadmap issue #246](https://github.com/SolenMusic/Paperweight/issues/246).

## v0.1.0 - Game Library

Stabilise the embedding API and add material-definition versioning, generator
algorithm versioning, texture caches and hashes, runtime resolution selection,
and asynchronous generation. Games should be able to distribute compact
definitions and generate hardware-appropriate maps at installation, first
launch, load time, or during development-time asset processing.

## Unscheduled future work

- Optional Git assistance layered over a working folder.
- An Emscripten/WebAssembly portability proof.
- Per-axis seam policy (`X`, `Y`, both, or none) for directional materials.
- Material-authored placement permissions for X/Y mirroring and quarter-turn
  rotation, including correct tangent-space normal transforms and physical-size
  handling. See
  [future issue #233](https://github.com/SolenMusic/Paperweight/issues/233).

These remain planned but intentionally do not displace the numbered roadmap
above.
