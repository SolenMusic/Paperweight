# Layer Groups and Reusable Overlays

Paperweight v0.0.32 makes a group part of the portable material recipe rather
than a macOS-only folder. The AppKit outline is only one editor for that model;
native, command-line, packed-library, and future WebAssembly callers all compile
the same hierarchy through the C++ core.

## Group semantics

A group owns a stable identity, optional parent identity, friendly name,
enabled state, composite mode, opacity, output routing, periodic mask, and mask
coordinate settings. Layers likewise receive stable identities and optional
parent-group identities once a material uses hierarchy.

Groups are pass-through by default. Their children evaluate against the material
accumulated below the group, and the complete before/after result is composited
once at the group boundary. Consequently, wrapping a contiguous set of layers
in an enabled, unmasked, 100%-opaque blend group is byte-identical to leaving
those layers flat—even when the first child is a processor rather than a
generator.

A group mask and opacity use one deterministic coverage value for every routed
channel. Colour cannot receive moss in one place while height or roughness uses
a differently sampled boundary. A disabled group skips its complete subtree.
Normal remains derived from the grouped height branch.

Groups may nest eight levels deep. A material may contain up to 32 groups and 32
layers. The validator rejects duplicate or malformed identities, missing
parents, cycles, excessive depth, and group descendants split into multiple
non-contiguous stack ranges. Those constraints keep the visible outline and the
evaluation order unambiguous.

## Native editor workflow

The layer panel is an `NSOutlineView`. Disclosure triangles collapse groups
without changing the document or generated output. Select a contiguous range of
sibling layers and choose **Group** or **Edit > Group Selection**. Selecting
layers already inside a group and grouping them creates a nested group. Group
names are editable directly in the outline.

Selecting a group exposes its enabled state, routed outputs, composite mode,
opacity, transform, and mask controls in the existing inspector. Copy,
Duplicate, Cut, Delete, and structural Undo/Redo treat a selected group as one
complete subtree. Paste and Duplicate generate fresh identities, so importing
the same overlay twice never creates ambiguous references.

Layer drag sorting remains native. A drag can reorder layer siblings when their
container contains only layers; grouping mixed layer/group children is kept
explicit so a drag cannot silently change ancestry.

## Reusable overlays

`.pwoverlay` is a small source format, not a rendered texture and not a linked
external dependency. It contains readable overlay metadata followed by a
version-2 layer fragment holding one group subtree. It deliberately contains no
material seed, UID, physical repeat, or global surface settings. On insertion,
the destination material supplies those values and all group/layer identities
are remapped.

Use **Tools > Material Overlays** to insert Polished Moss or Polished Lichen,
import a `.pwoverlay`, or save the selected group as a reusable overlay. Saved
overlays remain editable text and suitable for source control.

The portable API is declared in:

- `include/paperweight/material_overlay.hpp` for overlay parsing, serialisation,
  and bundled presets;
- `include/paperweight/layer_fragment.hpp` for group-subtree transport; and
- `include/paperweight/layer.hpp` for group and hierarchy data.

## Compatibility

PMAT version 23 adds `groups.count`, `group.N.*`, `layer.N.id`, and
`layer.N.parent`. Versions 1 through 22 load as flat stacks. Serialising them
with v0.0.32 performs an explicit format migration, but generation before and
after migration remains byte-identical.

Layer-fragment version 1 remains readable for v0.0.31 clipboard data. Version 2
adds groups and stable hierarchy. Unsupported future versions and malformed
overlay envelopes fail before mutating the destination document.
