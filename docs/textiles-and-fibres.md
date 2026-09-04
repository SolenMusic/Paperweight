# Textiles and Fibres

Paperweight v0.0.28 adds one reusable portable textile operation. It constructs
cloth and pile from material-space yarn or tuft geometry; upholstery, carpet,
canvas, and banner cloth are recipes rather than separate generators.

## Construction models

- Plain weave alternates the top warp and weft yarn at every crossing.
- Basket weave groups crossings using `weave_span` before alternating them.
- Twill weave advances the over-under pattern by `twill_step` each row.
- Loop pile builds ring-shaped yarn loops over a backing.
- Cut pile builds dense raised tufts with an authored directional fibre lay.

Round, flat, and twisted yarn profiles control the cross-section independently
from the weave. Yarn width, roundness, crossing relief, jitter, pile radius,
pile height, fibre frequency, fibre strength, and twist remain ordinary scalar
parameters. The operation can output its finished material or individual warp,
weft, over-under, fibre, pile, damage, colour-variation, direction, or height
fields for further masking and composition.

## Stable variation and damage

Thread and tuft identities come from wrapped integer coordinates and the mixed
material/layer seed. Missing fibres, local damaged segments, accent or repair
fibres, colour variation, and pile ordering therefore do not depend on pixels,
resolution, or worker scheduling. Changing only output resolution samples the
same construction more or less densely.

All sampling is periodic across both axes. Uniform orientation accepts any tile
count. Alternating rows require an even row count, alternating columns require
an even column count, and checkerboard rotation requires both counts to be even;
validation rejects combinations whose outer boundary could not repeat exactly.

## Native authoring and preview

Add **Textile / Fibres** from the normal layer menu. The inspector exposes the
construction model, output field, yarn profile, tile direction, thread and tile
counts, relief, fibres, damage, four colours, and seed offset. The result routes
to any existing material channels and composes with masks, transforms, and all
ordinary processors.

The bundled Woven Upholstery, Alternating Carpet, and Heraldic Banner Cloth
showcases demonstrate dense plain weave, checkerboard cut pile, and fine twill.
The banner material is intentionally a cloth substrate: shape primitives,
radial motifs, borders, and other existing layers can add heraldry without a
special-purpose flag generator.

The MetalKit 3D preview adds **Wavy Flag** in both the full editor and Material
Design Wizard. Its pinned edge, droop, moving waves, and recomputed surface
normal make cloth response visible under the existing material maps. Manual
phase and Play Animation controls move the full editor's flag; the wizard shows
the same geometry at its current phase. This deformation is GPU presentation
only and is never serialised or included in deterministic exports.

## Compatibility

Textile layers require `.pmat` format 20. Older formats cannot name the new
operation, so reading versions 1 through 19 keeps every historical generated
byte unchanged. The C++ model and evaluator use no AppKit or Metal types and
remain suitable for native, game-library, and future WebAssembly builds.
