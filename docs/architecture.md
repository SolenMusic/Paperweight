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
noise, and normalised periodic FBM. Reusable evaluation objects produce both
normalised RGBA colour and a scalar value. The layer authoring model compiles
into a directed acyclic graph before pixels are evaluated, so colour, height,
tangent-space normal, roughness, metalness, coating, occlusion, clear coat,
clear-coat roughness, and emissive colour can share a procedural surface or follow
independent direct-graph branches. Texture samples are taken at pixel centres
over one mathematical period. Normal-map finite differences wrap both axes; no
output copies or repairs image edges.

The operation variants are noise, solid colour, levels, threshold, brick grid,
tile grid, course layout, Worley cells, random cells, lines, rectangles,
circles, analytic shape primitives, shape Boolean processors, integer-winding
lattices, surface patterns, surface filters, stylisation processors, and region fields.
Every `MaterialLayer` also owns an enabled flag, opacity, and blend, add, or
multiply composite mode. Levels and threshold process the accumulated input;
noise, solid colour, and structural operations generate new samples. These
operation values remain the authoring vocabulary used by the graph compiler.

In v0.0.4, a layer also owns its coordinate transform and optional mask. The
evaluator rotates by 0, 90, 180, or 270 degrees, applies positive integer X/Y
scale, and then applies continuous offsets. An optional two-channel periodic
FBM field displaces those coordinates before the layer operation runs. The
mask is a separate deterministic periodic FBM field with its own seed,
low/high remapping, and inversion; its value multiplies the layer opacity.

The integer-scale and quarter-turn restrictions are part of the seamlessness
contract, not UI shortcuts. Moving either input coordinate by one tile always
moves transformed coordinates by whole periods. Periodic warp and mask fields
therefore remain periodic under the same shift. Arbitrary-angle toroidal
resampling may be explored later, but v0.0.4 never offers a transform that can
quietly break a tile edge.

In v0.0.5, the structural operations share small portable coordinate and
coverage primitives. Repeated coordinates always wrap into the unit tile.
Brick and tile grids expose mortar or grout coverage; lines, rectangles, and
circles expose anti-aliased shape coverage; random cells use stable integer
hashing; and Worley cells compare the nearest two feature points across a
toroidally wrapped neighbourhood. Seed domains keep structurally random
operations independent of noise, warp, and masks. Each operation returns the
same paired scalar and colour sample used by the existing layer pipeline, so it
automatically composes with transforms, masks, and every material output.

In v0.0.6, `MaterialGraph` becomes the authoritative evaluation model. Every
node has a stable non-zero identifier and one explicit category:

- generator nodes own a source operation and coordinate transform;
- processing nodes apply levels or threshold to one input, or composite source
  and background inputs with optional mask input;
- mask nodes evaluate the existing transformed, remapped periodic mask field;
- output nodes route each of the ten material outputs to a value branch.

Graph validation enforces the 512-node limit, unique identifiers, existing and
category-compatible input references, valid operation parameters, acyclic
dependencies, and exactly one node for each material output. Node storage order
is irrelevant and disconnected working nodes are allowed, which is important
for a future interactive editor.

The layer compiler emits a transparent base, one generator or unary processor
per enabled layer, optional mask nodes, explicit composite processors, and ten
output nodes. Historical empty stacks retain their implicit base-noise source.
Generated nodes record their source-layer index so diagnostics and later
incremental caching can map evaluation work back to authoring state. Existing
v0.0.5 golden pixels are unchanged.

In v0.0.7, every `Material` declares the physical width and height of its unit
repeat. `GenerationRequest::physicalCoverage` optionally selects a larger
world-space rectangle. Coverage must contain a whole number of material repeats
on both axes, making the output periodic without stretching, clipping, or seam
repair. Omitting coverage evaluates exactly one repeat and preserves all legacy
callers and pixels. Pixel-centre coordinates scale by the repeat count, while
normal-map derivatives divide by coverage in metres, so both feature placement
and surface slope remain stable when resolution changes.

Brick generators may additionally replace relative column, row, and mortar
parameters with physical width, height, and mortar width. The dimensions must
divide the material repeat into 1 to 64 whole bricks. Mortar distance and edge
softness are evaluated in metre space, giving one real horizontal/vertical
width even on non-square repeats.

In v0.0.8, five periodic `SurfacePatternOperation` modes provide ridged noise,
bands, rings, scatter, and streaks through one shared parameter vocabulary.
They are generator nodes and use domain-separated hashing plus periodic noise,
so seeds remain deterministic and a unit-coordinate shift remains an exact
repeat. Named surfaces such as cracked stone, wood, or marble are recipes built
from these reusable operations rather than permanent special cases in the core.

`SurfaceFilterOperation` adds invert, soften, expand, contract, edge, slope,
cavity, and peaks as graph processors. Neighbourhood-aware modes evaluate their
input node at a wrapped 3x3 set of material-space positions. Recursive neighbour
evaluation deliberately bypasses the root-coordinate memoisation cache, while
ordinary graph sharing at the requested sample remains cached. A radius is
therefore independent of output pixels, giving stable features at matching
physical positions across resolutions.

In v0.0.11, posterise, colour ramp, palette, edge-aware soften, and ink contour
remain ordinary unary processing nodes. Posterise and surface filters declare
whether they affect colour, scalar surface data, or both. Colour ramps and
palettes intentionally preserve scalar input, while ink measures a periodic
neighbourhood but writes RGB only. This makes stylisation orthogonal to material
shape: callers can change an art direction without silently changing collision-
like relief cues in height and normal maps. Palette comparison is performed on
UNORM8 channels with integer squared distance and stable source-order ties.

In v0.0.12, structural generators also emit `RegionSample` metadata alongside
their existing scalar/RGBA result. It contains a validity bit, exact 64-bit key,
normalised local U/V, distance to the region centre, and distance to its
boundary. The key is derived only from a generator domain and wrapped integer
cell ownership; it never passes through a floating-point channel. Worley
ownership records the nearest wrapped feature point, including across tile
seams. Unary processors retain the input region, neighbourhood processors keep
the centre region, and a composite with non-zero effective opacity adopts a
valid structural source region or otherwise preserves its background region.

`RegionFieldOperation` is an ordinary unary graph processor. It selects local
U, local V, centre distance, boundary distance, or a domain-separated random
channel derived from material seed, region key, seed offset, and channel number.
The normalised result can target RGB, scalar data, or both, can be inverted and
remapped, and can feed a composite mask in a direct graph. An input without an
active region deterministically supplies field value zero. This makes region
variation composable with existing blend, add, multiply, colour-ramp, height,
normal, and roughness paths rather than creating a second evaluator.

In v0.0.13, `CourseLayoutOperation` provides one shared partitioning engine for
masonry, large slabs, and overlapping slates. It first creates normalised,
seeded course heights and then creates normalised block widths within the
selected course. The slab profile may vary the block count per course; masonry
and slate profiles retain the requested count. Alternate rows apply a controlled
block-relative stagger. Crooked horizontal and vertical boundaries use bounded
periodic value noise, so opposite tile edges agree exactly and neighbouring
partitions cannot cross.

One evaluation returns related block-face, mortar, course-interior, and overlap
masks plus stable region metadata. Block identity includes profile, wrapped
course index, and wrapped block index. An additional exact parent key identifies
the course, allowing `course_random` to vary rows without giving every block a
different result. Physical mode derives whole counts from material width,
height, block width, and course height, while gap width and slate overlap depth
remain metre measurements. Stone and slate therefore remain templates over one
portable operation rather than becoming permanent castle-specific code paths.

In v0.0.14, `RegionSurfaceOperation` is a unary graph processor over that stable
region metadata. Boundary distance supplies a resolution-independent bevel
coordinate; rounded, chamfered, and hand-cut profiles turn it into height.
Region-keyed plane gradients form deterministic facets, while independent
region-keyed centre displacement and direction supply peaks and slopes. Periodic
value-noise fields disturb only the bevel neighbourhood for chips, wear, and
erosion, so opposite repeat edges still agree exactly.

One evaluation computes constructed height together with cavity, outer-edge,
exposed-face, facet, and wear fields. Selecting a field turns it into an ordinary
scalar or colour value for later graph composition. Faceted normal treatment is
explicitly output-aware: it strengthens the seeded planes only while the normal
branch samples its derivative source. The colour, height, and roughness branches
therefore remain byte-identical when that checkbox alone changes.

In v0.0.15, analytic shape primitives expose a signed-distance field for
rounded rectangles, ellipses, capsules, diamonds, and ordered convex polygons.
The signed distance is negative inside, positive outside, and independent of
output resolution. Fill, inset, centred outline, and inner-border fields are
derived from that one boundary rather than raster post-processing. Repeated
instances wrap their cell coordinates and stable region identities; arbitrary
rotation is applied in each bounded instance's local frame, never to the global
periodic domain.

Shape Boolean is a unary graph processor that combines the accumulated mask
with another analytic shape through maximum (union), minimum (intersection), or
subtraction. Line and diamond lattices are generators over signed integer
winding pairs. Advancing across either tile axis therefore changes phase by a
whole number of cycles and preserves the exact repeat. A globally rotated
texture with arbitrary slope cannot generally satisfy that constraint, so the
API represents the honest tile-compatible winding rather than a misleading
free-angle control.

In v0.0.16, `ScatterOperation` separates placement from pixel evaluation. A
single deterministic layout pass creates candidate centres on the tile torus,
samples optional density and exclusion masks at those centres, selects weighted
populations, derives every instance attribute from independent stable hash
channels, and performs ordered spacing rejection. Exact integer priorities
govern both placement and later occlusion. The resulting layout contains no
resolution, output, or worker state and is built once per evaluator plan.

Pixel evaluation uses a bounded grid lookup around the sample and wrapped
shortest deltas to find only nearby accepted stamps. Analytic shape distance is
evaluated with the instance's scale, aspect, and rotation. The same selected
instance carries colour, height, roughness, local coordinates, and exact region
identity to every output branch. This makes population placement a reusable
generator rather than a gravel- or foliage-specific algorithm.

In v0.0.17, organic construction remains four reusable operations rather than
material-specific code. `OrganicCellOperation` stretches a periodic cellular
field along a selected grain and exposes plates, boundaries, or stable region
variation. `OrganicCrackOperation` builds periodic trunks and recursively
splitting branches once per evaluator plan; exact hierarchy and segment keys are
independent of pixels and workers. `LeafClusterOperation` constructs ovate,
lanceolate, cordate, or lobed silhouettes analytically, then derives edge,
midrib, vein, colour, height, roughness, and region outputs from the same stable
instance. Cluster occlusion uses exact integer ordering.
`OrganicAccumulationOperation` processes existing scalar and region metadata,
allowing moss, lichen, or colour variation to respond to cavities, boundaries,
low height, or an authored mask without introducing a bark- or foliage-specific
evaluation path.

The prepared graph evaluator resolves identifiers once per generation request
and memoises shared nodes once per sample. Immutable plans, including
deterministic scatter, crack, and leaf layouts, are shared by the request's
worker evaluators; mutable sample caches remain private. It walks only the branch
selected by `GenerationRequest::output`. An optional `GenerationRequest::graph` lets a
portable caller bypass layer compilation and provide a validated branched graph
directly. The layer model remains a stable, compact frontend rather than a
second evaluator.

`GenerationRequest::output` selects one portable RGBA8 result. The colour map
interpolates two RGBA endpoints, height and roughness use explicit linear
greyscale encodings, and the normal map encodes a normalised tangent-space XYZ
vector. This is an output contract, not a claim of a complete PBR material
model. Higher precision and additional pixel formats remain possible behind
the image abstraction.

In v0.0.26, `MaterialSetRequest` selects any subset of the ten outputs and
`generateMaterialSet` returns them in one coordinated `MaterialImageSet`.
Validation, graph preparation, deterministic layouts, worker startup, and
material-space X coordinates are shared. Constant remapped channels skip graph
evaluation. Height and normal share one unquantised scalar field only when their
graph source and normal treatment are provably equivalent. `GenerationRequest`
remains compatible and delegates to this path with one selected output.

In v0.0.27, radial profiles remain part of the same portable analytic shape
evaluator. Annulus, arc, sector, and crescent distances are evaluated in wrapped
material coordinates. Radial repetition transforms the sample into each copy's
local frame using only copy index, count, radius, phase, and orientation, then
selects the strongest deterministic result. It does not create a pixel-derived
placement list or depend on worker scheduling. The same operation therefore
feeds colour, height, roughness, and every other routed branch without adding a
second designed-surface engine.

In v0.0.28, textile construction is another portable generator operation rather
than a family of asset-specific recipes. It evaluates woven yarn crossings or
pile tufts directly from wrapped material coordinates, stable integer thread or
tuft identities, and the material seed. Alternating tile orientation transforms
coordinates before evaluation and validates the even tile counts required for a
periodic rotated boundary. No placement depends on output resolution, worker
scheduling, or generated pixels.

In v0.0.29, every region can expose a stable local frame in addition to its
exact key, centre distance, boundary distance, and optional parent key. A
portable attachment processor derives centre, edge, corner, and cavity anchors
from that metadata and evaluates fasteners, inlays, glyphs, chips, and cracks in
local coordinates. Material-mode coverage is shared by colour, height,
roughness, metalness, occlusion, and emissive evaluation, so related damage
cannot drift between maps. Stable hashes of authored values and region identity
replace mutable random state or pixel-order placement.

In v0.0.30, the existing leaf-cluster generator becomes a general hierarchical
organic population evaluator. Each accepted instance chooses a primary,
secondary, or tertiary analytic profile from stable hashes; profile, palette,
scale, cluster colour, leaf colour, height, roughness, and occlusion order never
depend on output pixels or worker scheduling. Blob, rosette, and lichen signed-
distance profiles and a ground-scatter arrangement reuse the existing toroidal
layout and bounded cell lookup. Reusable fill, contour, inner-highlight,
cluster-random, and population fields feed the ordinary graph branches.

Organic accumulation remains a processor, now with noise, colony, and speckle
profiles and independent fill, outline, inner-highlight, and detail fields. It
does not stamp stored images or introduce a second material evaluator.

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

The optional 3D preview is an AppKit-hosted MetalKit renderer. It receives ten
completed RGBA8 core images and uses them as colour, displacement, tangent-space
normal, roughness, metalness, coating, occlusion, clear coat, clear-coat
roughness, and emissive textures on built-in inspection meshes. Its
metallic/roughness shader derives dielectric F0 from IOR, treats base colour as
metallic reflection tint where appropriate, and samples a deterministic
procedural studio environment. Clear coat adds a separate dielectric lobe,
occlusion attenuates indirect light, emissive colour is added after lighting,
and optional anisotropy stretches brushed-metal reflection along the authored
texture-space direction. Shape, camera, environment presets, lighting, map
switches, and animation are display state only. The wizard can inspect the same
maps on a square plane, sphere, cube, or cylinder without regenerating them. The Metal shader
is bundled with the application and compiled at runtime, avoiding a separate
shader-toolchain requirement for ordinary CMake builds. A missing compatible
Metal device disables only the 3D mode.

The reference catalogue keeps seed ownership outside its recipes.
`MaterialRecipe` contains every authored field except `seed`; instantiation
copies it into a normal `Material` only after a caller supplies that value.
High-level template controls are typed mappings onto ordinary material or layer
properties, so the catalogue adds no parallel evaluator and no hidden state.

The v0.0.20 wizard remains on that same boundary. Its portable session contains
a seedless recipe, caller-selected seed and physical size, ordinary material
colours, typed template-control values, and transient locks. Deterministic
alternative generation perturbs only unlocked values using stable integer
hashes, then calls the same template-control and material-validation paths as
every other caller. A selected candidate is a complete ordinary `Material`;
family, page, lock, and comparison state never enter the graph or `.pmat`.

AppKit owns the guided pages, live 2D and Metal presentation, cancellation,
comparison thumbnails, and working-folder save dialogue. Both preview modes use
the existing generator and 3D view, while finishing either hands the material to
the full editor or serialises it through the existing portable writer.

The portable stylised bake consumes completed colour plus normal or height
images and returns an independent image. It remains outside `MaterialOutput`,
the graph, and `.pmat` serialisation. Reference images and bake settings are
editor presentation state. This preserves a sharp boundary between unlit source
maps suitable for runtime shaders and deliberately illustrated exports.

Live previews use a serial background queue and a snapshot of the current
material. The editor and wizard retain completed 3D map sets and use
`affectedMaterialOutputs` to invalidate only conservatively dependent channels;
the old complete presentation remains visible until replacements are ready.
Continuous UI changes are briefly coalesced and cooperatively cancel
the active core generation between scanlines. For sufficiently large images,
the synchronous core call divides rows among a bounded native worker pool; each
worker owns its mutable graph-evaluator cache and writes disjoint rows while
sharing the request's immutable plan. Seeds and coordinates
remain independent of scheduling. A monotonically increasing revision allows
only the newest complete result onto the main thread. AppKit updates, including
the loading indicator and image conversion, remain on the main thread. Callers
may force the serial reference path or disable threading at build time.

The graph evaluator compiles graph identifiers and processing connections into
an internal index-based execution plan, then memoises shared nodes within one
material sample. It does not retain intermediate images across pixels or
preview generations. Profiling found graph compilation itself takes only a few
microseconds; generation time is dominated by procedural sampling, particularly
neighbourhood filters. Any later image or prefix cache must demonstrate a useful
hit rate and bounded memory without altering the cancellation contract.

### Material format

`.pmat` is a human-readable, versioned text format. Parsing and serialisation live in the portable core. Round trips should be stable and errors should identify useful source locations.

The format version is deliberately independent of the application version.
Paperweight v0.0.31 reads `.pmat` format versions 1 through 22 and writes version 22.
Unknown keys and unsupported format versions fail explicitly instead of being
silently ignored. Version 1 maps to the original implicit FBM source; adding an
explicit base noise layer produces byte-identical output. Version-2 layers map
to identity transforms with warp and masks disabled, also preserving their
historical output exactly. Format version 4 adds the seven structural operation
names and their explicit parameter groups. Format version 5 adds explicit cell-
or texture-space brick mortar; older bricks migrate to cell space without pixel
changes. Format version 6 adds metre-suffixed material repeat dimensions and the
optional physical brick parameter group. Versions 1 through 5 acquire a 1m by
1m repeat, so default one-repeat generation remains byte-identical. The layer
recipe still compiles to a graph only in memory. Format version 7 adds the
surface-pattern and surface-filter groups; versions 1 through 6 remain unchanged.
Format version 8 adds posterise, colour-ramp, palette, ink-contour, edge-aware
softening, and processing-target fields. Colour-only processors retain the
scalar branch used by height, normal, and roughness.
Format version 9 adds the Region Field parameter group. Versions 1 through 8
retain their historical generated bytes and migrate explicitly when saved.
Format version 10 adds course-layout parameters and the parent-course random
field. Versions 1 through 9 retain their historical generated bytes.
Format version 11 adds the Region Surface parameter group. Versions 1 through
10 retain their historical generated bytes.
Format version 12 adds analytic shape, shape-Boolean, convex-vertex, and lattice
parameter groups. Versions 1 through 11 retain their historical generated
bytes.
Format version 13 adds scatter placement, population, attribute, stamp, and
candidate-centre mask fields. Versions 1 through 12 retain their historical
generated bytes.
Format version 14 adds organic cells, branching cracks, parametric leaf
clusters, and organic accumulation. Versions 1 through 13 retain their
historical generated bytes.
Format version 15 adds optional material identity and descriptive metadata.
These fields remain outside the graph and generator. The portable material
library index accepts source path/text pairs rather than filesystem APIs,
keeping folder discovery, file coordination, thumbnails, and user defaults in
the AppKit frontend. Index ordering and duplicate-UID diagnostics are stable
regardless of filesystem enumeration order.
Format version 16 adds per-layer colour, height/normal, and roughness routing,
physical relief depth, constant surface values, and minimum, maximum, and detail
composition. Legacy all-output materials keep the original shared compiled
graph, and absent relief retains the historical normal calculation, so version-15
pixel output remains byte-identical.
Format version 17 adds an independently routed metalness scalar, global
metalness remap endpoints, and dielectric IOR. Old files acquire zero metalness
and IOR 1.5. These additions do not alter the prior four output branches.
Format version 18 adds independently routed coating, occlusion, clear-coat,
clear-coat roughness, and emissive branches plus their global remaps, emissive
intensity, and brushed-reflection anisotropy controls. Neutral migration values
preserve the five version-17 output maps byte-for-byte.
Format version 19 adds annulus, arc, annular-sector, and crescent profiles plus
deterministic radial copy placement. Versions 1 through 18 acquire one fixed copy
at radius zero, preserving every historical shape evaluation.
Format version 20 adds an opt-in textile generator with weave, yarn, pile,
fibre, defect, colour, and tile-orientation parameters. Versions 1 through 19
contain no textile layer and retain every historical output byte.
Format version 21 adds region-attached detail and damage with named anchors,
local geometry, stable per-region selection, and coherent material-channel
values. Versions 1 through 20 contain no attachment layer and retain every
historical output byte.
Format version 22 adds hierarchical organic populations, blob/rosette/lichen
profiles, ground scatter, separate cluster/instance colour variation, reusable
contour/highlight fields, and richer accumulation profiles. Version-21 and older
leaf clusters and accumulation layers receive neutral defaults matching their
previous evaluation exactly.
Paperweight v0.0.21 adds a separate derived `.pwlib` format around canonical
PMAT payloads. Its encoder and memory-backed reader remain in the
portable core; filesystem discovery and save panels remain outside it. A strict
little-endian directory exposes UID and friendly-name lookup without evaluating
or parsing pixels. Raw and bounded RLE storage feed the same PMAT parser and
ordinary `Material` type, so packing introduces no second evaluator. Fixed
layout rules and entry/library FNV-1a checksums make corruption and architecture
drift observable. See [pwlib-format.md](pwlib-format.md).
Direct-graph text persistence is deferred until a node-authoring workflow can
round-trip it without discarding information.
See [pmat-format.md](pmat-format.md).

## Native workspace ownership

The v0.0.22 AppKit process has one application coordinator and one editor
controller per open material. The coordinator owns global menus, the full
library, the wizard, benchmarks, pack inspection, document-path identity, and
the collection of live editors. Each editor owns all mutable material and
preview state. This preserves the existing proven editor implementation while
removing its former singleton lifetime assumption.

The library window and editor windows share an AppKit tabbing identifier. The
selected tab may be detached into an ordinary independent window without
changing its controller or material state. A lightweight library navigator is
instantiated per editor because AppKit views cannot safely belong to several
windows at once. It performs filesystem discovery and PMAT metadata parsing for
presentation only; generation and material semantics remain in the portable
core. No workspace or window state is added to `.pmat` or `.pwlib`.

In v0.0.31 each editor owns a native multi-select layer table plus transient UUID
identities parallel to its `Material::layers` vector. Those UUIDs preserve UI
selection across reordering and are refreshed for pasted or duplicated layers;
they are editor state and never enter `.pmat`, `.pwlib`, graph evaluation, or
generated bytes. The visible table reverses the vector so its top-to-bottom
presentation continues to mean top-to-bottom compositing.

Cross-document transfer uses the portable `layer_fragment` codec. Its versioned
text envelope carries a lossless ordered layer subset through a dedicated
pasteboard type, while an ordinary text representation remains available for
inspection. Parsing yields layers only: carrier-level PMAT metadata is ignored,
so paste cannot replace the destination seed, name, UID, physical repeat, or
surface settings. The AppKit controller assigns fresh transient identities and
inserts the fragment immediately above the selected evaluation layer.

Structural stack changes capture layers, identities, and selection in a bounded
per-editor `NSUndoManager`. Parameter edits clear that structural history until
general material undo exists, preventing an older structural snapshot from
silently discarding newer parameter work. Every accepted structural change uses
the ordinary affected-output analysis, preview revision, coalescing, and
cooperative cancellation path.

## Initial data flow

```text
.pmat text -> parser -> material + ordered layers
                    ^               |
                    |       layer-to-graph compiler
                serialiser           |
                                      v
                       validated material graph (DAG)
                      /          |          |         \
               generators   processors    masks    outputs
                      \          |          /         |
                       +------ memoised evaluation ----+
                                      |
                 +-----+------+-----+------+-----+------+-----+------+-----+------+
                 |     |      |     |      |     |      |     |      |     |
              colour height normal rough metal coating AO clear coat coat-R emissive
                 |     |      |     |      |     |      |     |      |     |
                 +-----+------+-----+------+-----+------+-----+------+-----+------+
                                          |
                                    preview / PNG
```

## Deferred architecture

Visual node-canvas authoring, graph-specific text persistence, WebAssembly
bindings, game-engine adapters, arbitrary global texture
rotation, bitmap-backed botanical stamps, and procedural GPU backends remain
outside v0.0.31. MetalKit presents CPU-generated images with metallic/roughness,
coating, occlusion, clear-coat, emissive, and anisotropic shading, or with the
optional cel-lighting mode; it is not a
generator backend and its lighting choices are never serialised into `.pmat`.
The plane, sphere, cube, cylinder, and animated Wavy Flag are likewise
presentation meshes only.
