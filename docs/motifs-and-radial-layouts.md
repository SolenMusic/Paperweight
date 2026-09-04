# Motifs, profiles and radial layouts

Paperweight v0.0.27 adds a small analytic vocabulary for designed surfaces. It
is intended for targets, dials, jump pads, arches, medallions, decorative trim,
fastener arrangements, and similar objects that need deliberate geometry rather
than natural noise.

## Profile primitives

Four reusable signed-distance profiles join the existing rounded rectangle,
ellipse, capsule, diamond, and convex polygon:

- **Annulus** produces a complete ring between an outer and inner radius.
- **Arc** clips that ring to an authored start angle and sweep.
- **Sector** fills the matching angular wedge between its inner and outer radii.
- **Crescent** subtracts an offset inner circle from an outer ellipse.

For annuli, arcs, and sectors, the smaller of width and height defines the outer
circular diameter. `inner_radius` controls the annular cut-out, while `arc_start`
and `arc_sweep` bound arcs and sectors. Crescents retain the authored width and
height and use `crescent_offset` to move a proportionate subtractive ellipse. The established fill, inset,
outline, border, transform, mask, Boolean, composite, and per-output routing
systems continue to apply.

## Radial copies

Any analytic shape can be repeated around the centre of its existing grid cell:

```text
layer.0.shape.radial_copies = 8
layer.0.shape.radial_radius = 0.32
layer.0.shape.radial_phase = 22.5
layer.0.shape.radial_orientation = outward
```

`fixed` retains the shape's authored rotation. `outward` points its local axis
away from the centre. `tangent` rotates it ninety degrees further so it follows
the circle. The radial layout combines with the ordinary local rotation, making
small per-design adjustments possible without defining another primitive.

Copy placement is evaluated directly from continuous material coordinates. It
does not inspect output pixels, create thread-dependent candidates, or use GPU
math. Region identity includes the selected radial copy, so later region and
surface operations remain stable.

## Seam and compatibility contract

The evaluator wraps coordinates on the same texture torus as every other
Paperweight generator. A motif touching an edge therefore continues from the
opposite edge exactly. Worker scheduling cannot enter the result, and the two
bundled showcases are locked by byte-exact golden hashes.

PMAT version 19 writes every new field explicitly. Versions 1 through 18 still
load with legacy defaults. A file containing radial-profile fields while claiming
an older version is rejected instead of being interpreted approximately.

The native editor remains a layer frontend. These controls compile into the same
portable graph operations used by a game, a command-line tool, and a future
WebAssembly build.
