# Organic silhouettes and hierarchical clusters

Paperweight v0.0.30 expands the existing organic operations into a reusable
vocabulary for dense foliage, layered rocky ground, moss colonies, lichen, and
other overlapping natural structures. The implementation remains portable C++20
and evaluates analytic shapes; it does not embed bitmap stamps or add an asset-
specific generator.

## Hierarchical populations

One `LeafClusterOperation` can contain three populations. The primary population
uses the ordinary `profile`, palette, and size. Secondary and tertiary
populations add a profile, weight, scale, and palette. Their combined weight may
not exceed one; the remaining probability belongs to the primary population.

Population selection, placement, rotation, height, roughness, and occlusion order
come from stable integer hashes of the material seed, operation seed offset,
cluster identity, and instance identity. Candidate order and worker scheduling
cannot affect the result. The `clusterColourVariation` and
`instanceColourVariation` controls blend independent broad and fine colour
channels. Legacy defaults reproduce the original per-instance colour exactly.

## Analytic silhouettes and fields

The existing ovate, lanceolate, cordate, and lobed leaf profiles are joined by:

- `blob`: a rounded, irregular multi-lobed form;
- `rosette`: a stronger radial petal or low plant silhouette;
- `lichen`: a smaller, more irregular colony stamp.

The silhouettes expose material, fill, edge, midrib, vein, instance-random,
outline, inner-highlight, cluster-random, and population fields. Outline and
inner-highlight widths are authored in normalised local-shape space, so later
layers can colour or sculpt the same structural boundary consistently.

`groundScatter` uses the same bounded periodic cluster layout as foliage but is
named for hierarchical stones, chips, debris, and ground cover. All instance
centres are stored on the unit torus, and evaluation uses shortest wrapped
distances.

## Moss and lichen accumulation

`OrganicAccumulationOperation` now offers `noise`, `colonies`, and `speckles`
profiles. Each can return its complete material treatment or its fill, outline,
inner-highlight, or detail field. Cavity, boundary, low-height, and authored-mask
biases continue to use the active graph region or scalar input.

## Compatibility and examples

PMAT version 22 serialises every population, silhouette, colour-variation, and
accumulation parameter. Versions 14 through 21 load with zero-weight additional
populations, the original colour variation, and the original accumulation
profile, preserving historical output exactly.

Four editable materials are bundled under `examples/materials/`:

- `hierarchical-foliage.pmat`;
- `hierarchical-ground.pmat`;
- `rich-moss-colonies.pmat`;
- `rich-lichen-stamps.pmat`.

They are also included in the native showcase menu and benchmark catalogue.
Tests cover exact torus samples, format round trips and version rejection,
one-versus-four-worker identity, and fixed output checksums.
