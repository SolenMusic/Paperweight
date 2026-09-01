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
tangent-space normal, and roughness can share a procedural surface or follow
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
- output nodes route colour, height, normal, or roughness to a value branch.

Graph validation enforces the 512-node limit, unique identifiers, existing and
category-compatible input references, valid operation parameters, acyclic
dependencies, and exactly one node for each material output. Node storage order
is irrelevant and disconnected working nodes are allowed, which is important
for a future interactive editor.

The layer compiler emits a transparent base, one generator or unary processor
per enabled layer, optional mask nodes, explicit composite processors, and four
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

The prepared graph evaluator resolves identifiers once per generation request
and memoises shared nodes once per sample. It walks only the branch selected by
`GenerationRequest::output`. An optional `GenerationRequest::graph` lets a
portable caller bypass layer compilation and provide a validated branched graph
directly. The layer model remains a stable, compact frontend rather than a
second evaluator.

`GenerationRequest::output` selects one portable RGBA8 result. The colour map
interpolates two RGBA endpoints, height and roughness use explicit linear
greyscale encodings, and the normal map encodes a normalised tangent-space XYZ
vector. This is an output contract, not a claim of a complete PBR material
model. Higher precision and additional pixel formats remain possible behind
the image abstraction.

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

The optional 3D preview is an AppKit-hosted MetalKit renderer. It receives four
completed RGBA8 core images and uses them as colour, displacement, tangent-space
normal, and roughness textures on built-in inspection meshes. Shape, camera,
lighting, map switches, and animation are display state only. The Metal shader
is bundled with the application and compiled at runtime, avoiding a separate
shader-toolchain requirement for ordinary CMake builds. A missing compatible
Metal device disables only the 3D mode.

Live previews use a serial background queue and a snapshot of the current
material. Continuous UI changes are briefly coalesced and cooperatively cancel
the active core generation between scanlines. For sufficiently large images,
the synchronous core call divides rows among a bounded native worker pool; each
worker owns its graph evaluator and writes disjoint rows. Seeds and coordinates
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
Paperweight v0.0.15 reads `.pmat` format versions 1 through 12 and writes version 12.
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
Direct-graph text persistence is deferred until a node-authoring workflow can
round-trip it without discarding information.
See [pmat-format.md](pmat-format.md).

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
                           +----------+----------+---------+
                           |          |          |         |
                         colour    height     normal   roughness
                           |          |          |         |
                           +----------+----------+---------+
                                          |
                                    preview / PNG
```

## Deferred architecture

Visual node-canvas authoring, graph-specific text persistence, WebAssembly
bindings, game-engine adapters, complete PBR authoring, arbitrary global texture
rotation, arbitrary instance scattering, and procedural GPU backends remain
outside v0.0.15. MetalKit in v0.0.15 presents
CPU-generated images and optional cel lighting; it is not a
generator backend and its lighting choices are never serialised into `.pmat`.
