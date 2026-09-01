# Reference Materials and Stylised Baking

Paperweight v0.0.18 turns the procedural vocabulary developed through v0.0.17
into a small reference catalogue aimed at the ten supplied Blastard textures.
The catalogue is an authoring aid, not a new evaluator: every result is still an
ordinary `Material`, an ordinary layer graph, and an ordinary `.pmat` file after
instantiation.

## Seedless recipes

`MaterialRecipe` contains the complete material construction except for a seed.
`makeMaterialRecipe` can lift an existing material into this seedless form, and
`instantiateMaterial(recipe, seed)` creates a normal editable material with the
caller's chosen seed. This makes the ownership rule unambiguous: a template
describes a family of materials, while the caller chooses the particular member.

The bundled `.pmat` showcase files remain ordinary seeded documents for backwards
compatibility and direct editing. When one is used through **File > New from
Reference Template**, the catalogue converts it to a seedless recipe, discards
the showcase seed, and instantiates it with the seed currently shown in the
editor. Saving writes the resulting ordinary seeded `.pmat` document.

The catalogue contains:

| Reference template | Main high-level controls |
| --- | --- |
| Castle Flagstone | slab count, unevenness, joint width, surface detail |
| Castle Foliage | foliage density, leaf size, cluster spread, moss coverage |
| Castle Roof | slate count, course overlap, crookedness, variation |
| Castle Stone | block count, course count, unevenness, joint width |
| Castle Window | lead width, frame width, surface relief |
| Cel Castle Stone | block and course count, hand-cut character, joint width |
| Cel Courtyard Gravel | density, pebble size, spacing, facet strength |
| Cel Forest Bark | plate count and length, crack width, lichen coverage |
| Cel Forest Crate | plank gap, grain detail and wander, edge wear |
| Cel Forest Rock | stone rows and columns, edge rounding, facet strength |

High-level controls are portable descriptors with one or more typed bindings to
ordinary material properties. They do not introduce hidden generator state.
Authors may use these controls for broad changes and then continue with the full
layer inspector.

## Reference comparison

The 2D editor can open a PNG or BMP reference beside the generated tile. The
reference is display-only: its path and pixels are neither copied into the
material nor saved in `.pmat`. The catalogue records each expected reference
filename so a team can keep its own licensed or project-specific reference set
outside Paperweight.

The **1 x 1** and **3 x 3** controls continue to apply to the generated side.
Tests additionally generate true three-repeat physical coverage and compare all
nine repeats byte for byte with the one-repeat result.

## Portable stylised lighting

`bakeStylisedLighting` is a dependency-free C++ operation over completed RGBA8
images. It accepts unlit colour plus either a normal map or a height map. A
normal map takes precedence; height-only input derives wrapped central-difference
normals, so the resulting bake remains seamless.

The settings expose:

- light azimuth and elevation;
- two to sixteen discrete diffuse bands;
- highlight threshold and contribution;
- ambient contribution; and
- height-derived normal strength.

The operation returns a new colour image and never mutates its inputs. It is not
a fifth authoritative material output and is not serialised into `.pmat`. In the
editor it appears as the explicitly optional **Baked presentation** mode, and a
PNG exported while that mode is active is named accordingly. Colour, height,
normal, and roughness remain the right inputs for Blastard's runtime cel lighting.

## Acceptance

All ten templates have byte-exact golden colour, height, normal, roughness, and
baked-presentation checks. The same tests exercise single-worker and multi-worker
generation, true 1 x 1 and 3 x 3 coverage, and both slices of the universal macOS
build. A checksum change therefore remains an algorithm compatibility event, not
ordinary visual housekeeping.
