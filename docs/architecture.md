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
tile grid, Worley cells, random cells, lines, rectangles, circles, surface
patterns, and surface filters.
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
Paperweight v0.0.9 reads `.pmat` format versions 1 through 7 and writes version 7.
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
bindings, game-engine adapters, complete PBR authoring, arbitrary-angle
rotation, and GPU backends remain outside v0.0.9.
