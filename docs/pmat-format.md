# `.pmat` format version 13

Paperweight material files are UTF-8 text. They are intended to be readable,
diffable, and small enough to embed alongside game assets.

## Canonical form

```text
# Paperweight procedural material
pmat.version = 13
material.type = fbm
material.seed = 18431
material.width = 1m
material.height = 1m
colour.low = 0x000000FF
colour.high = 0xFFFFFFFF
noise.frequency = 4
noise.octaves = 5
noise.lacunarity = 2
noise.gain = 0.5
normal.strength = 1
roughness.low = 0.25
roughness.high = 0.85
layers.count = 1
layer.0.enabled = true
layer.0.operation = noise
layer.0.composite = blend
layer.0.opacity = 1
layer.0.noise.seed_offset = 0
layer.0.transform.scale_x = 1
layer.0.transform.scale_y = 1
layer.0.transform.offset_x = 0
layer.0.transform.offset_y = 0
layer.0.transform.rotation = 0
layer.0.warp.enabled = false
layer.0.warp.strength = 0
layer.0.warp.frequency = 1
layer.0.warp.seed_offset = 0
layer.0.mask.enabled = false
layer.0.mask.inverted = false
layer.0.mask.seed_offset = 0
layer.0.mask.input_low = 0
layer.0.mask.input_high = 1
```

The serialiser writes the global keys first and layers in ascending order, with
Unix line endings and one trailing newline. Layer zero is the bottom of the
stack. It uses a locale-independent decimal representation that round-trips
every material value exactly.

## Syntax

- Each non-empty line contains one `key = value` assignment.
- Spaces and tabs around a key, equals sign, or value are ignored.
- A `#` starts a comment that continues to the end of the line.
- LF and CRLF line endings are accepted.
- Keys may appear in any order when reading.
- Each key may appear only once and unknown keys are errors.
- Input must be valid UTF-8 when opened by the Mac app.
- `layers.count` may be from 0 to 32. Every declared layer and each parameter
  required by its operation must be present.

Malformed input produces a diagnostic with a one-based line and column plus a
plain-language reason. Parsing either produces one complete valid material or
no material; it never returns a partially accepted definition.

## Global fields

| Key | Meaning | Accepted value |
| --- | --- | --- |
| `pmat.version` | File-format version | `13` |
| `material.type` | Generator model | `fbm` |
| `material.seed` | Deterministic seed | Unsigned 64-bit integer |
| `material.width` | Width of one seamless material repeat | Metre value from `0.000001m` to `1000000m` |
| `material.height` | Height of one seamless material repeat | Metre value from `0.000001m` to `1000000m` |
| `colour.low` | Low colour and threshold endpoint | `0xRRGGBBAA` hexadecimal |
| `colour.high` | High colour and threshold endpoint | `0xRRGGBBAA` hexadecimal |
| `noise.frequency` | Base lattice frequency for noise operations | Integer from 1 to 64 |
| `noise.octaves` | FBM octave count | Integer from 1 to 8 |
| `noise.lacunarity` | Frequency multiplier per octave | Integer from 1 to 4 |
| `noise.gain` | Amplitude multiplier per octave | Decimal from 0.1 to 0.9 |
| `normal.strength` | Tangent-space normal slope multiplier | Decimal from 0 to 16 |
| `roughness.low` | Roughness at scalar value zero | Decimal from 0 to 1 |
| `roughness.high` | Roughness at scalar value one | Decimal from 0 to 1 |
| `layers.count` | Number of ordered layers | Integer from 0 to 32 |

The combination of frequency, octaves, and lacunarity must keep every lattice
period at or below 4096. The roughness endpoints may be reversed if an inverse
relationship is wanted.

`material.width` and `material.height` describe one complete mathematical
repeat, not the pixel dimensions of an export. A caller may set
`GenerationRequest::physicalCoverage` to the same size or to a whole-number
multiple on either axis. Fractional repeats are rejected because cropping or
stretching them would break the seamlessness contract. Omitting coverage means
exactly one material repeat. Changing only output resolution changes sampling
density, not material-space feature placement.

## Layer fields

Every layer `N` has these common keys:

| Key | Meaning | Accepted value |
| --- | --- | --- |
| `layer.N.enabled` | Whether evaluation includes this layer | `true` or `false` |
| `layer.N.operation` | Reusable evaluation operation | See operation table below |
| `layer.N.composite` | How the result combines with accumulated input | `blend`, `add`, or `multiply` |
| `layer.N.opacity` | Composite amount | Decimal from 0 to 1 |

The operation selects exactly one parameter group:

| Operation | Required key | Accepted value |
| --- | --- | --- |
| `noise` | `layer.N.noise.seed_offset` | Unsigned 64-bit integer |
| `solid_colour` | `layer.N.solid.colour` | `0xRRGGBBAA` hexadecimal |
| `levels` | `layer.N.levels.input_low` | Decimal from 0 to 1 |
| `levels` | `layer.N.levels.input_high` | Decimal from 0 to 1 and greater than input low |
| `levels` | `layer.N.levels.gamma` | Decimal from 0.1 to 4 |
| `threshold` | `layer.N.threshold.value` | Decimal from 0 to 1 |
| `brick_grid` | `layer.N.brick.sizing` | `relative` or `physical` |
| `brick_grid` (relative) | `layer.N.brick.columns`, `rows` | Integers from 1 to 64 |
| `brick_grid` (relative) | `layer.N.brick.mortar` | Decimal from 0 to 0.95 |
| `brick_grid` (relative) | `layer.N.brick.mortar_space` | `cell` or `texture` |
| `brick_grid` (physical) | `layer.N.brick.width`, `height` | Positive metre values that divide the material repeat into 1 to 64 bricks |
| `brick_grid` (physical) | `layer.N.brick.mortar_width` | Non-negative metre value smaller than brick width and height |
| `brick_grid` | `layer.N.brick.stagger` | Decimal from 0 to 1 |
| `brick_grid` | `layer.N.brick.softness` | Decimal from 0 to 0.25 |
| `tile_grid` | `layer.N.tile.columns`, `rows` | Integers from 1 to 64 |
| `tile_grid` | `layer.N.tile.grout` | Decimal from 0 to 0.95 |
| `tile_grid` | `layer.N.tile.softness` | Decimal from 0 to 0.25 |
| `course_layout` | `layer.N.course.profile` | `masonry`, `slabs`, or `slates` |
| `course_layout` | `layer.N.course.field` | `blocks`, `mortar`, `course`, or `overlap` |
| `course_layout` | `layer.N.course.sizing` | `relative` or `physical` |
| `course_layout` | `layer.N.course.blocks`, `courses` | Base counts from 1 to 64 |
| `course_layout` | `layer.N.course.block_variation` | Width variation from 0 to 1 |
| `course_layout` | `layer.N.course.height_variation` | Course-height variation from 0 to 1 |
| `course_layout` | `layer.N.course.stagger` | Alternate-course offset from 0 to 1 block |
| `course_layout` | `layer.N.course.crookedness` | Seamless boundary displacement from 0 to 1 |
| `course_layout` | `layer.N.course.gap` | Relative gap amount from 0 to 0.95 |
| `course_layout` | `layer.N.course.softness` | Edge transition from 0 to 0.25 |
| `course_layout` | `layer.N.course.overlap` | Relative slate overlap from 0 to 0.95 |
| `course_layout` | `layer.N.course.seed_offset` | Unsigned 64-bit integer |
| `course_layout` (physical) | `layer.N.course.block_width`, `course_height` | Positive metre dimensions resolving to 1 to 64 blocks/courses |
| `course_layout` (physical) | `layer.N.course.gap_width` | Non-negative metre width smaller than a block and course |
| `course_layout` (physical) | `layer.N.course.overlap_depth` | Non-negative metre depth smaller than one course |
| `worley_cells` | `layer.N.worley.columns`, `rows` | Integers from 1 to 64 |
| `worley_cells` | `layer.N.worley.jitter` | Decimal from 0 to 1 |
| `worley_cells` | `layer.N.worley.edge_width` | Decimal from 0.01 to 2 |
| `worley_cells` | `layer.N.worley.seed_offset` | Unsigned 64-bit integer |
| `random_cells` | `layer.N.random_cells.columns`, `rows` | Integers from 1 to 64 |
| `random_cells` | `layer.N.random_cells.seed_offset` | Unsigned 64-bit integer |
| `lines` | `layer.N.lines.direction` | `vertical` or `horizontal` |
| `lines` | `layer.N.lines.count` | Integer from 1 to 64 |
| `lines` | `layer.N.lines.width` | Decimal from 0 to 1 |
| `lines` | `layer.N.lines.softness` | Decimal from 0 to 0.25 |
| `rectangles` | `layer.N.rectangles.columns`, `rows` | Integers from 1 to 64 |
| `rectangles` | `layer.N.rectangles.width`, `height` | Decimals from 0 to 1 |
| `rectangles` | `layer.N.rectangles.softness` | Decimal from 0 to 0.25 |
| `circles` | `layer.N.circles.columns`, `rows` | Integers from 1 to 64 |
| `circles` | `layer.N.circles.radius` | Decimal from 0 to 0.5 |
| `circles` | `layer.N.circles.softness` | Decimal from 0 to 0.25 |
| `shape` or `shape_boolean` | `layer.N.shape.kind` | `rounded_rectangle`, `ellipse`, `capsule`, `diamond`, or `convex_polygon` |
| `shape` or `shape_boolean` | `layer.N.shape.field` | `fill`, `inset`, `outline`, or `border` |
| `shape` or `shape_boolean` | `layer.N.shape.columns`, `rows` | Integers from 1 to 64 |
| `shape` or `shape_boolean` | `layer.N.shape.width`, `height` | Cell-relative decimals from 0.001 to 1 |
| `shape` or `shape_boolean` | `layer.N.shape.corner_radius`, `inset`, `border_width` | Cell-relative decimals from 0 to 0.5 |
| `shape` or `shape_boolean` | `layer.N.shape.softness` | Cell-relative decimal from 0 to 0.25 |
| `shape` or `shape_boolean` | `layer.N.shape.offset_x`, `offset_y` | Cell-relative decimal from -0.5 to 0.5 |
| `shape` or `shape_boolean` | `layer.N.shape.stagger` | Alternate-row shift from 0 to 1 cell |
| `shape` or `shape_boolean` | `layer.N.shape.rotation` | Local rotation in degrees from -360 to 360 |
| `shape` or `shape_boolean` | `layer.N.shape.seed_offset` | Unsigned 64-bit region-identity salt |
| `shape` or `shape_boolean` | `layer.N.shape.vertices` | Convex polygon vertex count from 3 to 12 |
| `shape` or `shape_boolean` | `layer.N.shape.vertex.K.x`, `.y` | Ordered convex vertex coordinates from -0.5 to 0.5 |
| `shape_boolean` | `layer.N.shape.boolean` | `union`, `intersection`, or `subtraction` |
| `shape_boolean` | `layer.N.shape.target` | `colour`, `scalar`, or `all` |
| `lattice` | `layer.N.lattice.kind` | `lines` or `diamonds` |
| `lattice` | `layer.N.lattice.winding_x`, `winding_y` | Signed integer cycles from -64 to 64, not both zero; diamonds require both non-zero |
| `lattice` | `layer.N.lattice.width` | Decimal from 0.001 to 1 |
| `lattice` | `layer.N.lattice.softness` | Decimal from 0 to 0.25 |
| `lattice` | `layer.N.lattice.phase` | Decimal from 0 to 1 |
| `scatter` | `layer.N.scatter.field` | `material`, `fill`, `instance_random`, `local_u`, `local_v`, or `boundary_distance` |
| `scatter` | `layer.N.scatter.columns`, `rows` | Candidate-grid integers from 1 to 64 |
| `scatter` | `layer.N.scatter.density`, `jitter` | Decimals from 0 to 1 |
| `scatter` | `layer.N.scatter.minimum_distance` | Torus distance from 0 to 0.5 |
| `scatter` | `layer.N.scatter.overlap` | `forbidden`, `controlled`, or `unrestricted` |
| `scatter` | `layer.N.scatter.maximum_overlap` | Controlled-overlap fraction from 0 to 1 |
| `scatter` | `layer.N.scatter.seed_offset` | Unsigned 64-bit integer |
| `scatter` | `layer.N.scatter.populations` | Integer from 1 to 4 |
| `scatter` | `layer.N.scatter.population.K.weight` | Positive relative selection weight |
| `scatter` | `layer.N.scatter.population.K.min_scale`, `max_scale` | Ordered decimals from 0.1 to 4 |
| `scatter` | `layer.N.scatter.population.K.min_aspect`, `max_aspect` | Ordered decimals from 0.25 to 4 |
| `scatter` | `layer.N.scatter.population.K.min_rotation`, `max_rotation` | Ordered degrees from -360 to 360 |
| `scatter` | `layer.N.scatter.population.K.colour_low`, `colour_high` | `0xRRGGBBAA` hexadecimal |
| `scatter` | `layer.N.scatter.population.K.min_height`, `max_height` | Ordered decimals from 0 to 1 |
| `scatter` | `layer.N.scatter.population.K.min_roughness`, `max_roughness` | Ordered decimals from 0 to 1 |
| `scatter` | `layer.N.scatter.density_mask.*`, `exclusion_mask.*` | Explicit `enabled`, `inverted`, `frequency`, `input_low`, `input_high`, and `seed_offset` fields |
| `surface_pattern` | `layer.N.surface.kind` | `ridged_noise`, `bands`, `rings`, `scatter`, or `streaks` |
| `surface_pattern` | `layer.N.surface.scale` | Integer from 1 to 64 |
| `surface_pattern` | `layer.N.surface.width` | Decimal from 0.001 to 1 |
| `surface_pattern` | `layer.N.surface.detail` | Decimal from 0 to 1 |
| `surface_pattern` | `layer.N.surface.distortion` | Decimal from 0 to 1 |
| `surface_pattern` | `layer.N.surface.variation` | Decimal from 0 to 1 |
| `surface_pattern` | `layer.N.surface.seed_offset` | Unsigned 64-bit integer |
| `surface_filter` | `layer.N.filter.kind` | `invert`, `soften`, `expand`, `contract`, `edge`, `slope`, `cavity`, `peaks`, or `edge_aware_soften` |
| `surface_filter` | `layer.N.filter.radius` | Material-space distance from 0 to 0.25 tile units |
| `surface_filter` | `layer.N.filter.strength` | Decimal from 0 to 1 |
| `surface_filter` | `layer.N.filter.sensitivity` | Edge-aware similarity threshold from 0 to 1 |
| `surface_filter` | `layer.N.filter.target` | `colour`, `scalar`, or `all` |
| `posterise` | `layer.N.posterise.bands` | Integer from 2 to 16 |
| `posterise` | `layer.N.posterise.target` | `colour`, `scalar`, or `all` |
| `colour_ramp` | `layer.N.ramp.mode` | `linear` or `stepped` |
| `colour_ramp` | `layer.N.ramp.stops` | Integer from 2 to 8 |
| `colour_ramp` | `layer.N.ramp.stop.K.position` | Strictly increasing decimal from 0 to 1; first is 0 and last is 1 |
| `colour_ramp` | `layer.N.ramp.stop.K.colour` | `0xRRGGBBAA` hexadecimal |
| `palette` | `layer.N.palette.colours` | Integer from 2 to 8 |
| `palette` | `layer.N.palette.entry.K.colour` | `0xRRGGBBAA` hexadecimal |
| `ink_contour` | `layer.N.ink.colour` | `0xRRGGBBAA` hexadecimal |
| `ink_contour` | `layer.N.ink.radius` | Material-space distance from 0 to 0.25 tile units |
| `ink_contour` | `layer.N.ink.threshold` | Edge threshold from 0 to 1 |
| `ink_contour` | `layer.N.ink.softness` | Transition softness from 0 to 0.5 |
| `ink_contour` | `layer.N.ink.strength` | Decimal from 0 to 1 |
| `ink_contour` | `layer.N.ink.inverted` | `true` inks flat regions; `false` inks detected edges |
| `region_field` | `layer.N.region.field` | `random`, `local_u`, `local_v`, `centre_distance`, `boundary_distance`, or `course_random` |
| `region_field` | `layer.N.region.seed_offset` | Unsigned 64-bit integer |
| `region_field` | `layer.N.region.channel` | Independent random channel from 0 to 255 |
| `region_field` | `layer.N.region.output_low` | Decimal from 0 to 1 |
| `region_field` | `layer.N.region.output_high` | Decimal from 0 to 1 |
| `region_field` | `layer.N.region.inverted` | Whether to complement the selected field before remapping |
| `region_field` | `layer.N.region.target` | `colour`, `scalar`, or `all` |
| `region_surface` | `layer.N.sculpt.field` | `height`, `cavity`, `outer_edge`, `exposed_face`, `facet`, or `wear` |
| `region_surface` | `layer.N.sculpt.profile` | `rounded`, `chamfered`, or `hand_cut` |
| `region_surface` | `layer.N.sculpt.bevel_width` | Normalised boundary distance from 0.001 to 1 |
| `region_surface` | `layer.N.sculpt.bevel_height` | Constructed bevel height from 0 to 1 |
| `region_surface` | `layer.N.sculpt.facet_count` | Seeded planar facet count from 3 to 16 |
| `region_surface` | `layer.N.sculpt.facet_strength` | Planar contribution from 0 to 1 |
| `region_surface` | `layer.N.sculpt.centre_peak` | Per-region centre peak from 0 to 1 |
| `region_surface` | `layer.N.sculpt.slope` | Per-region directional slope from 0 to 1 |
| `region_surface` | `layer.N.sculpt.chips` | Edge chipping amount from 0 to 1 |
| `region_surface` | `layer.N.sculpt.chip_scale` | Periodic chip and wear scale from 1 to 64 |
| `region_surface` | `layer.N.sculpt.wear` | Edge wear amount from 0 to 1 |
| `region_surface` | `layer.N.sculpt.erosion` | Edge erosion amount from 0 to 1 |
| `region_surface` | `layer.N.sculpt.seed_offset` | Unsigned 64-bit integer |
| `region_surface` | `layer.N.sculpt.faceted_normals` | Whether normal evaluation strengthens planar facets |
| `region_surface` | `layer.N.sculpt.target` | `colour`, `scalar`, or `all` |

Brick and tile values are one inside each unit and zero in mortar or grout.
With relative brick `mortar_space = cell`, mortar is a fraction of each repeated cell,
preserving version-4 behaviour. With `mortar_space = texture`, mortar is a
fraction of the complete tile and therefore has the same horizontal and
vertical width even when column and row counts differ. Stagger shifts alternate
brick rows by a fraction of one brick. Worley values rise towards cell interiors
and approach zero at cell boundaries; `edge_width` controls that transition.
Random cells assign one deterministic value per cell. Shape sizes are fractions
of a repeated cell, and softness controls a smooth coverage transition around
an edge.

The `shape` operation is an analytic repeated generator. Rounded rectangles,
ellipses, capsules, diamonds, and ordered convex polygons share one signed
boundary-distance contract, from which fill, inset, centred outline, and inner
border fields are derived. Rotation is local to each bounded instance, so any
finite angle remains seamless. Convex vertices must be non-degenerate and
consistently ordered around the boundary. `shape_boolean` evaluates the same
shape and combines its coverage with the accumulated input by union,
intersection, or subtraction.

Lattice directions are represented by whole signed winding counts across the
tile rather than arbitrary global angles. This is what makes line and crossed
diamond families repeat exactly on both axes. A freely chosen global rotation
is not generally compatible with a rectangular periodic tile; repeated bounded
shapes remain the appropriate tool when arbitrary local rotation is required.

`scatter` uses the complete `shape.*` group as one local stamp, with stamp
columns and rows fixed to one. It first creates at most one candidate per grid
cell. Stable hash channels determine candidate probability, jitter, population,
every attribute, placement priority, and occlusion priority. Candidates are
sorted by exact placement priority before torus-distance rejection; neither
output pixels nor worker scheduling participate. Density and exclusion masks
sample periodic value noise only at candidate centres. Accepted instances use
a bounded cell lookup, while wrapped shortest-distance evaluation makes a stamp
crossing one edge continue from the opposite edge. Overlapping stamps select
the exact highest occlusion priority. `material` output uses each instance's
colour, height, and roughness on the corresponding output branch; the other
fields expose reusable masks and local instance coordinates.

Course layouts partition the complete repeat into courses and then partition
each course into blocks. `masonry` varies widths within regular course counts,
`slabs` may also vary the number of blocks per course, and `slates` exposes an
additional overlap field. All four fields come from the same layout and seed:
`blocks` is the stable face mask, `mortar` is its complement, `course` keeps the
horizontal course interior without vertical joints, and `overlap` marks the
lower overlapping strip of slate faces. Boundary displacement is periodic and
bounded so crookedness cannot tear or reorder neighbouring courses.

Each block is an ordinary stable region. Its parent course has a separate exact
identity; `region.field = course_random` hashes that parent so every block in a
row receives the same deterministic value. Physical sizing derives whole block
and course counts from material repeat dimensions while gap and overlap remain
true metre distances. The stored base counts remain explicit for predictable
round trips and for switching back to relative sizing.

Region Surface consumes the currently active region rather than creating new
geometry. Boundary distance becomes the selected rounded, chamfered, or
hand-cut bevel; region-keyed planes, centre displacement, and direction add
facets, peaks, and slopes. Chipping, wear, and erosion are confined to the edge
neighbourhood and use periodic noise. Selecting a mask field exposes the same
construction data for later colour, roughness, moss, or dirt composition. With
`faceted_normals = true`, only the normal branch receives stronger planar
derivative planes; colour, height, and roughness do not change.

Advanced surface patterns are deterministic generators in the same scalar and
colour pipeline as noise and structural shapes. `scale` controls repetitions or
feature density; the four normalised controls deliberately have shared names so
recipes can be edited consistently while each pattern interprets them in its
own geometric way. All coordinate displacement remains periodic.

Surface filters process the complete accumulated graph input rather than
creating a fresh source. Except for invert, they sample a wrapped 3x3
neighbourhood at `radius`; zero radius therefore becomes a centre-only sample.
Soften averages, expand and contract select extrema, edge measures local range,
slope measures a finite-difference gradient, and cavity/peaks isolate local
depressions or protrusions. `strength` blends the processed result with its
input. Because radius is a material-space distance, matching physical points
produce matching results at different export resolutions.

Edge-aware soften weights nearby samples by their scalar similarity to the
centre, reducing small variation without washing deliberate boundaries across
one another. In version 8, `target` chooses whether a filter changes RGB colour,
the scalar surface field, or both. Earlier files implicitly target both and
retain their historical bytes.

Posterise snaps selected channels to evenly spaced bands. Colour ramps map the
current scalar field to two through eight ordered colours without changing that
scalar field; stepped mode holds each stop colour while linear mode interpolates.
Palette selects the nearest authored colour using deterministic UNORM8 integer
distance and source order for ties. Ink contours detect wrapped neighbourhood
contrast and blend an authored ink colour into RGB only. Consequently, colour
ramps, palettes, and ink can stylise a material while preserving height, normal,
and roughness output exactly.

Brick, tile, Worley, random-cell, line, rectangle, and circle generators attach
a stable structural region to their result. Its key remains a 64-bit integer in
the evaluator; it is never stored in RGB, scalar, or floating-point metadata.
The region also carries local U/V coordinates and normalised centre and boundary
distances. Unary processing retains this metadata. A non-zero composite adopts
a valid structural source region and otherwise keeps the background region, allowing a
Region Field layer above colour or filter layers to address the same cells.

`region_field` converts one active region field into an ordinary graph value.
`random` hashes the material seed, exact region key, seed offset, and numbered
channel, so channel changes produce independent deterministic values without
moving boundaries. Other field kinds ignore the random parameters but retain
them in canonical text for an unambiguous operation shape. The selected value is
optionally inverted, mapped between `output_low` and `output_high`, and written
to colour, scalar, or both. A missing active region supplies field value zero.
Scalar targeting can drive height, normal, and roughness in a layer recipe;
portable direct graphs may route separate Region Field nodes to individual
outputs or use one as a composite mask.

Physical brick sizing replaces `columns`, `rows`, `mortar`, and
`mortar_space` with explicit dimensions. For example:

```text
material.width = 1.92m
material.height = 0.45m
layer.0.brick.sizing = physical
layer.0.brick.width = 0.24m
layer.0.brick.height = 0.075m
layer.0.brick.mortar_width = 0.01m
```

This produces eight columns and six rows. Mortar is evaluated as a true 10mm
distance on both axes rather than as a percentage of differently shaped cells.
The native editor presents those derived column and row counts explicitly. When
an author changes the brick size or count, it recalculates `material.width` and
`material.height` automatically so the saved definition remains seamless.

Every layer in versions 3 through 13 also has this coordinate-transform group:

| Key | Meaning | Accepted value |
| --- | --- | --- |
| `layer.N.transform.scale_x` | Horizontal repetitions per tile | Integer from 1 to 16 |
| `layer.N.transform.scale_y` | Vertical repetitions per tile | Integer from 1 to 16 |
| `layer.N.transform.offset_x` | Horizontal periodic offset in tile units | Decimal from -1024 to 1024 |
| `layer.N.transform.offset_y` | Vertical periodic offset in tile units | Decimal from -1024 to 1024 |
| `layer.N.transform.rotation` | Clockwise quarter-turn | `0`, `90`, `180`, or `270` |
| `layer.N.warp.enabled` | Whether periodic distortion is applied | `true` or `false` |
| `layer.N.warp.strength` | Maximum coordinate displacement | Decimal from 0 to 1 |
| `layer.N.warp.frequency` | Periodic warp-field frequency | Integer from 1 to 16 |
| `layer.N.warp.seed_offset` | Independent deterministic warp seed | Unsigned 64-bit integer |

Scale and quarter-turn rotation deliberately preserve the unit tile period.
Offsets are continuous and wrap naturally. When enabled, warp uses two
independent periodic FBM channels to displace the transformed coordinates.
Disabling warp, or setting its strength to zero, is exactly the identity path.

Every layer in versions 3 through 13 also declares its optional mask:

| Key | Meaning | Accepted value |
| --- | --- | --- |
| `layer.N.mask.enabled` | Whether the mask affects this layer | `true` or `false` |
| `layer.N.mask.inverted` | Whether to replace mask value `m` with `1 - m` | `true` or `false` |
| `layer.N.mask.seed_offset` | Independent deterministic mask seed | Unsigned 64-bit integer |
| `layer.N.mask.input_low` | Noise value mapped to mask zero | Decimal from 0 to 1 |
| `layer.N.mask.input_high` | Noise value mapped to mask one | Decimal from 0 to 1 and greater than input low |

The mask samples an independent periodic FBM field in the layer's transformed
coordinates. Its remapped value multiplies the layer opacity, allowing smooth,
threshold-like, or inverted spatial control without changing the operation.
Disabled masks evaluate to exactly one. Transform, warp, and mask fields remain
required in versions 3 through 13 even when their optional features are disabled; this
keeps canonical files explicit and round trips unambiguous.

Noise seed offset zero reproduces the original material seed exactly. Other
offsets are deterministically mixed with the material seed, allowing multiple
independent noise layers without storing random state. Solid colour uses Rec.
709 luminance for its scalar value. Levels and threshold transform the
accumulated input; threshold selects `colour.low` or `colour.high`.

Blend interpolates between the accumulated and source values. Add sums the
opacity-scaled source and clamps it to the normalised range. Multiply scales the
accumulated value towards a full multiplication according to opacity. The same
formula is applied to scalar, red, green, blue, and alpha channels.

## Graph compilation

Paperweight v0.0.16 retains the layer syntax, now at version 13, as the compact,
human-editable authoring projection. Before generation, the portable core
compiles it into a directed acyclic material graph:

- source operations become generator nodes;
- levels, threshold, shape Boolean, surface filters, posterise, colour ramps,
  palettes, ink contours, region fields, and region surfaces become unary
  processing nodes;
- enabled procedural masks become mask nodes;
- layer blend, add, or multiply behaviour becomes composite processing nodes;
- colour, height, normal, and roughness receive explicit output nodes.

Disabled layers compile as exact no-ops. Node metadata records the source layer
for future diagnostics and incremental evaluation. All four output nodes point
to the final layer result for a `.pmat` material, preserving historical output.
Portable C++ callers may instead provide a direct graph with independent output
branches through `GenerationRequest::graph`.

Graph-specific text syntax is intentionally deferred until Paperweight has a
graph authoring workflow that can round-trip it honestly. Format version 7 adds
advanced surface recipes; it does not serialise the internal graph representation.
Format version 8 adds stylised processors without serialising preview lighting.
Format version 9 adds region fields without serialising the internal 64-bit key.
Format version 10 adds course layouts and parent-course random fields without
serialising either exact region key.
Format version 11 adds constructed region surfaces and normal-only facet
treatment without serialising derived masks or planes.
Format version 12 adds analytic shape generators, shape Boolean processors, and
integer-winding lattices without introducing graph-specific persistence.
Format version 13 adds deterministic scatter generators, weighted populations,
per-instance material attributes, and candidate-centre masks.

## Material outputs

Every layer-authored output derives from the same final graph sample at
the same pixel centre:

- Colour encodes the final RGBA channels.
- Height writes the final scalar to R, G, and B as linear UNORM8, with alpha 255.
- Roughness interpolates between `roughness.low` and `roughness.high` using the
  final scalar, then writes linear greyscale UNORM8 with alpha 255.
- Normal uses wrapped central differences of the final scalar field. Derivatives
  are measured per metre of requested coverage. The tangent-space vector
  `(-dH/dx * strength, -dH/dy * strength, 1)` is
  normalised, maps XYZ from `[-1, 1]` to RGB `[0, 255]`, and writes alpha 255.

All procedural noise remains periodic, and scalar neighbours used by normal
generation wrap mathematically across both tile axes.

## Compatibility policy

The `.pmat` format version and Paperweight application version are separate.
Paperweight v0.0.16 reads versions 1 through 13 and writes version 13. A reader
rejects unsupported versions and unknown fields so that it cannot quietly
reinterpret a future material.

Version-1 files have an implicit base-noise evaluation. Paperweight preserves
that behaviour, including historical files that omit later colour, normal, or
roughness keys. The Mac editor presents an explicit base noise layer after
opening such a file; this is byte-identical. Version-2 layers acquire identity
transforms with warp and masks disabled, which also preserves every generated
pixel. Version 3 remains the exact Masks and Warping representation. Structural
operations require version 4. Version 5 adds `brick.mortar_space`; version-4
bricks migrate to `cell` and retain their exact pixels. Version 6 adds the
physical repeat and brick fields. Versions 1 through 5 migrate to a 1m by 1m
repeat and retain their exact default-coverage pixels. Version 7 adds advanced
surface patterns and filters without changing older evaluations. Version 8 adds
posterise, colour ramp, palette, ink contour, edge-aware soften, and explicit
filter targets. Versions 1 through 7 retain byte-identical default evaluations.
Version 9 adds region fields; versions 1 through 8 retain byte-identical default
evaluations. Version 10 adds course layouts and `course_random`; versions 1
through 9 retain byte-identical default evaluations. Version 11 adds region
surface sculpting; versions 1 through 10 retain byte-identical default
evaluations. Version 12 adds analytic shapes, mask Boolean operations, and
integer-winding lattices; versions 1 through 11 retain byte-identical default
evaluations. Version 13 adds deterministic scatter operations; versions 1
through 12 retain byte-identical default evaluations. Saving any older format
performs the explicit migration to version 13.

The portable entry points are `paperweight::parsePmat` and
`paperweight::serialisePmat` in `include/paperweight/pmat.hpp`.
The graph model, compiler, and validator are declared in
`include/paperweight/graph.hpp`.
