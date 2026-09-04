# Region-Attached Detail and Damage

Paperweight v0.0.29 adds one reusable way to place secondary construction and
damage inside an existing procedural region. A slab can own an inlay, a plank
can own its nails, and a shaped panel can own its corner chips without baking
their positions into texture-space pixels.

## Stable region frame

`RegionSample` now describes:

- an exact 64-bit region key;
- normalised local `U` and `V` coordinates;
- centre and boundary distance;
- optional parent identity; and
- the orientation of the local `U` axis.

The local coordinates are evaluated by the source generator. Rotated shapes,
scatter stamps, leaves, and alternating textile tiles therefore carry their
details with them. The orientation value is metadata for callers; evaluation
already occurs in the region-local frame.

## Named anchors

`resolveRegionAnchor` exposes four semantic locations:

- **Centre** places a detail near the stable region centre.
- **Edge** chooses one edge and a position along it.
- **Corner** chooses one of the four local corners.
- **Cavity** selects a boundary-biased location and gates coverage by the
  region's cavity distance.

Anchor choice, jitter, and selection use only the material seed, region key,
operation seed offset, and attachment index. Pixel traversal order, output
resolution, and worker scheduling never enter placement.

## Attachment operation

The Region Attachment layer offers six profiles:

- **Fastener**: an elliptical rivet, nail, or bolt head.
- **Inlay**: a diamond-shaped insert.
- **Glyph**: cross, chevron, triangle, or rune strokes.
- **Chip**: a seeded irregular missing region.
- **Crack**: a bent, optionally branching path between two named anchors.
- **Damage**: a coherent crack and chipped endpoint.

Count is per source region. Selection controls the stable fraction of regions
that receive the operation. Size, aspect, inset, rotation, jitter, line width,
length, branching, and softness remain normalised region-space controls.

The operation can expose a mask or distance field for further graph processing,
or author a complete material detail. Material mode applies one coverage value
to colour, height, roughness, metalness, occlusion, and emissive branches. A
damaged location therefore stays the same damaged location in every exported
map.

## Editor and examples

Add **Region Attachment** from the normal layer menu. The inspector exposes the
profile, output field, named anchors, glyph, placement controls, channel values,
colour, and deterministic seed offset. It remains an ordinary layer and can be
enabled, disabled, reordered, masked, or routed like any other operation.

Four editable examples demonstrate composition rather than asset-specific
generators:

- `attached-paving.pmat` combines slabs, selected inlays, and structural damage.
- `arch-stone-panel.pmat` anchors corner chips and cracks to a dressed stone.
- `damaged-crate.pmat` places fasteners and damage independently on each plank.
- `detailed-target-panel.pmat` builds bolts, a centre inlay, and a glyph from the
  same attachment vocabulary.

## Determinism contract

Region attachments use no mutable random generator and retain no pixel-sized
placement state. The release tests cover anchor repeatability, mathematical
seams, exact PMAT round trips, graph compilation, one/four-worker byte identity,
golden outputs for all four examples, and ARM64/x86-64 conformance.

The operation requires `.pmat` format 21. Versions 1 through 20 contain no
region-attachment layers and retain every historical output byte.
