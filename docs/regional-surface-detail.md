# Regional Surface Detail and Visual Richness

Paperweight v0.0.33 adds a reusable `regional_detail` processor for turning a
clean construction mask into a visually finished surface. It deliberately sits
after structural generators: courses, shapes, scatter, organic clusters,
textiles, and later generators can all share it.

## Physical frequency bands

Macro, meso, and micro sizes are authored in metres. The evaluator converts the
material repeat or current region extent into deterministic periodic noise
periods. The apparent construction therefore remains stable when preview or
export resolution changes; higher resolution resolves more of the same signal
rather than selecting a different recipe.

- Macro establishes broad value and colour movement.
- Meso provides mottling and material-scale breakup.
- Micro provides grain, fibres, pores, and fine roughness variation.

Each band has its own strength. Mottling, grain, and directional strokes are
separate reusable signals built from the bands rather than baked presentation
effects.

## Local frames and variation scope

Texture orientation evaluates over the complete seamless repeat. Region
orientation evaluates in the stable local frame published by a course, shape,
scatter instance, organic cluster, or textile element. Directional grain and
strokes can consequently follow individual slabs, leaves, or threads.

Stable variation can be keyed per material, containing layer-group identity,
parent region, or region. An ungrouped layer in group mode falls back to its
operation seed. The key is combined with the material seed and layer seed
offset. Pixel order, resolution, thread scheduling, and container traversal
never participate.

## Multi-band edges

Boundary distance becomes four independently selectable fields:

- outer shadow or cavity;
- bevel;
- body;
- inner highlight.

Outer, bevel, and inner widths are authored in metres. Irregularity moves the
edge with a stable meso signal, breakup removes deterministic segments, and
taper varies the combined band width without changing topology. The older Ink
Contour processor remains available when an intentionally flat, uniform outline
is the desired style.

## Coordinated material mode

Every raw field can target colour, scalar surface data, or both. `material` mode
uses the same field set to coordinate authored palette variation, relief,
roughness, coating or clear-coat wear, and cavity occlusion. Wear can favour
exposed edges, cavities, upward-facing planar regions, local patches, or a
weighted mixture.

This is still ordinary layer processing. Output routing, layer/group opacity,
masks, transforms, copy/paste, overlays, cancellation, and multithreaded
generation keep their existing behaviour.

## Compatibility and determinism

Texture-oriented gradients, grain, and strokes use tile-compatible torus
winding. Region-oriented signals use the region's stable local frame. Both
paths remain mathematically seamless, including arbitrary authored angles.

`.pmat` format 24 stores all parameters explicitly. Versions 1 through 23 have
no regional-detail operation and compile through their historical paths, so
existing material definitions retain their exact output. Tests cover parser
round trips, torus repetition, graph compilation, raw-field routing, invalid
physical values, and byte identity between one and four workers.

The production pass applies the operation to Arch Stone Panel, Attached Paving,
Hierarchical Foliage, Woven Upholstery, and Detailed Target Panel. Their colour
maps remain readable without preview lighting, while physical maps add depth and
material response in the 3D inspector.

## Production-pass golden comparison

The pass deliberately opts these five examples into PMAT 24. The table records
the unlit 32 x 32 RGBA colour checksum before and after the authored regional
detail layer; height, normal, and roughness checksums are recorded beside them
in the automated showcase corpus.

| Showcase | v0.0.32 colour | v0.0.33 colour | Authored change |
| --- | ---: | ---: | --- |
| Arch Stone Panel | `3595844545952752315` | `8983970020371048834` | Broad slab variation, facets, worn multi-band edge |
| Attached Paving | `1746044955187033111` | `17356369355658264478` | Per-stone palette, meso breakup, cavity-led wear |
| Detailed Target Panel | `16521662476113841838` | `9917969139753378835` | Coherent panel grain, edge highlights, coating wear |
| Hierarchical Foliage | `17377723102774959275` | `1463068211007462449` | Cluster variation, leaf-scale grain, broken contours |
| Woven Upholstery | `202589064041873769` | `11855165821438660058` | Macro dye movement, fibre grain, directional strokes |

Unmodified PMAT 1–23 fixtures keep their prior checksums. The v0.0.33 golden
changes above are therefore explicit material revisions, not silent evaluator
changes.
