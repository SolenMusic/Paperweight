# Organic structures

Paperweight v0.0.17 adds organic construction as portable, reusable graph
operations. Cel forest bark and castle foliage are ordinary editable `.pmat`
recipes composed from these operations; neither has a private generator.

## Deterministic contract

- All cells, crack segments, clusters, and leaves derive from the material seed,
  an operation seed offset, integer source coordinates, and fixed hash channels.
- Layout construction is independent of output resolution, selected material
  output, worker count, container iteration order, and thread scheduling.
- Coordinates are evaluated on the unit torus. Sampling at `(u, v)`,
  `(u + 1, v)`, or `(u, v + 1)` addresses the same construction.
- Crack and leaf layouts are prepared once per graph-evaluator plan. Pixel
  evaluation does not rebuild or mutate them.
- Stable 64-bit keys remain integer evaluator metadata. They are not packed into
  a floating-point or colour channel.
- CPU generation must remain byte-identical between the serial and threaded
  paths and between supported ARM64 and x86-64 release builds.

## Operations

`OrganicCellOperation` produces anisotropic cellular regions for bark plates.
It exposes plate coverage, boundary coverage, one random value per plate, local
coordinates, and exact region identity. Grain direction changes which material
axis receives the authored anisotropy.

`OrganicCrackOperation` creates complete periodic trunks and then attempts each
child branch in a fixed hash order. Branch probability falls by hierarchy;
width follows the authored taper. The same layout exposes all cracks, trunks,
branches, hierarchy-weighted coverage, or the complementary distance field.

`LeafClusterOperation` places bounded clusters and leaf instances on a periodic
grid. Radial, fan, vine, and canopy arrangements share the same deterministic
population contract. Ovate, lanceolate, cordate, and lobed profiles are analytic
silhouettes with optional curvature, notches, serration, lobes, midribs, and
secondary veins. The frontmost covering leaf is selected by exact integer
occlusion order and supplies aligned colour, height, roughness, and region data.

`leafSpeciesPreset` returns complete editable parameters for ivy, laurel, oak,
or ash. Presets are convenience values, not stored identities: `.pmat`
serialisation writes every resulting leaf parameter and never stores the seed
inside a template.

`OrganicAccumulationOperation` processes an accumulated graph value and active
region. Its periodic colony and breakup fields can be biased towards cavities,
boundaries, low surfaces, or a deliberately authored mask. Moss, lichen, and
colour variation may affect colour, scalar surface data, or both.

## Portable API

The public declarations are in `include/paperweight/organic.hpp` and the
operation parameter types are in `include/paperweight/layer.hpp`. The low-level
entry points are:

```cpp
OrganicCellSample evaluateOrganicCells(...);
OrganicCrackLayout buildOrganicCrackLayout(...);
OrganicCrackSample evaluateOrganicCracks(...);
LeafClusterLayout buildLeafClusterLayout(...);
LeafSample evaluateLeafCluster(...);
LeafClusterOperation leafSpeciesPreset(LeafSpecies species);
OrganicAccumulationSample evaluateOrganicAccumulation(...);
```

Ordinary callers generally author layers and use `generate`; direct-graph
callers may place the same operation types in generator and processing nodes.
The public API depends only on portable C++20.

## Bounds and validation

Cell grids remain at most 64 by 64. Crack layouts allow 16 roots, 16 segments
per root, and five branch levels. Leaf grids remain at most 64 by 64 with at
most 24 leaves per accepted cluster. A leaf cluster's spread and silhouette
extent must remain below one wrapped tile, ensuring bounded lookup and an
unambiguous periodic population. Invalid enum values, ranges, ordering, or
extents fail material validation before generation.

The tests cover exact layout reconstruction, hierarchy creation, periodic
samples, species distinction, cluster occlusion, `.pmat` version-14 round trips,
old-version rejection, one-versus-four-worker identity, and golden colour,
height, normal, and roughness checksums for both v0.0.17 showcases.
