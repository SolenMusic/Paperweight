# Metals and reflections

Paperweight v0.0.24 separates authoritative material data from interactive
presentation. Metalness is a deterministic CPU-generated map. Reflections are
drawn by the native preview and are never baked into that map or the colour map.

## Portable material contract

`MaterialOutput::metalness` is the fifth output. Like roughness, it evaluates
its routed graph branch at each pixel centre, remaps the scalar through
`metalness.low` and `metalness.high`, and writes linear greyscale RGBA8. Values
have the usual metallic/roughness meaning:

- zero is dielectric material such as paint, stone, ceramic, or water;
- one is conductive metal;
- intermediate values are useful at filtered transitions, weathered paint, and
  corrosion boundaries.

Pure materials should normally be near an endpoint rather than uniformly half
metal. The colour map remains unlit. In the preview, dielectric colour supplies
diffuse albedo, while metallic colour tints the reflected light.

`surface.ior` controls dielectric reflectivity. Normal-incidence reflectance is
calculated as:

```text
F0 = ((IOR - 1) / (IOR + 1)) squared
```

The default IOR 1.5 therefore produces F0 0.04. IOR is retained even on a fully
metallic material because one definition may contain paint, rust, varnish, or
another dielectric region.

PMAT format 17 stores all three new global values and permits `metalness` in a
layer's output list. Versions 1 through 16 read as metalness zero and IOR 1.5.
Their existing output bytes do not change.

## Native preview

The MetalKit view uses a Cook-Torrance-style metallic/roughness model with GGX
distribution, Smith visibility, and Schlick Fresnel. A procedural studio
environment gives reflective surfaces something legible to reflect without an
external HDR image dependency.

The inspection presets are:

- Chrome Studio: dark room, broad softboxes, and a clear horizon;
- Brushed Metal: long high-contrast reflection strips;
- Ceramic: bright, broad, warm illumination for dielectric gloss;
- Wet Surface: outdoor sky, dark ground, and a compact glint;
- Neutral: low-drama illumination for judging source colour.

Environment intensity and rotation remain editor state. The existing direct
light, animation, shape, camera, map switches, and optional toon mode also
remain presentation-only. Different GPUs may shade a preview by slightly
different floating-point amounts; they cannot change generated textures.

## Bundled templates

Chrome, steel, copper, and brass use constant full metalness with distinct
colour and roughness ranges. Painted Steel and Corroded Metal route the same
stable surface construction into a varying metalness map so exposed metal,
paint, and oxidation remain spatially coherent. All six definitions are normal
editable `.pmat` files, wizard starting points, benchmark inputs, and packable
library entries.

Automated acceptance covers PMAT round trips, source/packed generation,
single/multiple-worker byte identity, every output map, and exact 1x1/3x3
repetition. The preview shader is deliberately outside those cross-architecture
byte guarantees.
