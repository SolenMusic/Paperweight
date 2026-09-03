# Coatings and Special Surfaces

Paperweight v0.0.25 extends the portable material description from five outputs
to ten. The CPU generator remains authoritative: identical material text, seed,
coverage, resolution, and algorithm version produce identical bytes regardless
of preview hardware or worker scheduling.

## Portable output channels

The new channels are ordinary `MaterialOutput` values and use the same graph,
layer routing, cancellation, packing, and image-generation APIs as the existing
maps.

- `coating` is a general coverage mask for paint, varnish, glaze, or another
  layer above a substrate. A value of zero exposes the substrate; one denotes
  complete coverage.
- `occlusion` records ambient visibility. One is fully visible and zero is a
  deeply occluded cavity. It is deliberately authorable rather than inferred
  from a renderer-specific height heuristic.
- `clearcoat` controls the amount of an optically separate dielectric coat.
- `clearcoat_roughness` controls the coat's highlight independently of the base
  surface roughness.
- `emissive` is routed RGB colour multiplied by the material's emissive
  intensity. It is a generated map, not a light baked into the colour output.

Each scalar map has readable low/high remapping in `.pmat`. Layers route to any
combination of the nine authorable branches; normal remains derived from the
height branch so it cannot contradict the physical relief source. Existing
masks, region fields, cavity and boundary fields, lattices, scatter, filters,
and composites can therefore construct wetness, puddles, chipped paint, worn
varnish, cavity shadowing, or illuminated trim without special-case generators.

The coating map does not itself alter the colour or metalness bytes. A painted
metal recipe normally routes the same construction mask to colour, metalness,
roughness, and coating with suitable remaps. This keeps every exported channel
explicit and lets a game choose how its shader interprets the coverage mask.

## Brushed reflection

`anisotropy.strength` and `anisotropy.rotation` describe a texture-space brushed
direction. They are material properties rather than generated maps because the
direction is uniform in v0.0.25. The Metal preview uses them to stretch the GGX
reflection response. Coating coverage suppresses substrate anisotropy, which
allows exposed brushed metal to remain directional beneath isotropic paint.

Anisotropy is currently presentation metadata. It never changes colour,
height, normal, roughness, metalness, or any of the five new generated maps.

## Metal preview contract

The AppKit frontend uploads the ten completed RGBA8 images to MetalKit. Its
inspection shader applies the new data as follows:

- ambient occlusion attenuates indirect environment and ambient light, not the
  direct key light;
- clear coat contributes a second dielectric microfacet lobe using the
  clear-coat roughness map;
- emission is added after ordinary lighting and remains visible on an unlit
  face;
- anisotropy stretches the base reflection along the authored brush direction.

The preview's environment, lights, exposure, camera, shape, and GPU arithmetic
are deliberately unsaved presentation state. The wizard and full editor offer a
square plane, sphere, cube, and cylinder. Switching shape reuses the completed
maps and does not invoke the generator again.

## Included templates

- **Glazed Ceramic** uses tiled regions, recessed occluded grout, a smooth
  dielectric glaze, and small hand-made surface variation.
- **Lacquered Wood** places a polished clear coat above warped grain with a
  directional brushed response.
- **Wet Stone** accumulates a smooth water coat in low stone boundaries while
  retaining rough, faceted exposed faces.
- **Machinery Panels** combines painted plates, exposed brushed-steel seams,
  wear, and cavity occlusion.
- **Illuminated Science-Fiction Surface** routes seamless lattice trim into a
  separate emissive map above a dark plated hull.

They are normal editable PMAT recipes and seedless wizard starting points. No
template-only evaluator or hidden state is involved.

## PMAT compatibility

Format version 18 stores all new routing tokens and global controls. Versions 1
through 17 load with neutral defaults: no coating, full ambient visibility, no
clear coat, clear-coat roughness 0.1, zero emissive intensity, and no anisotropy.
The migration tests compare every historical colour, height, normal, roughness,
and metalness byte before and after the upgrade.

