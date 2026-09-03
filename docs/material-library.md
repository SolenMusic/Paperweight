# Working Folder and Material Library

Paperweight v0.0.19 treats a normal folder of readable `.pmat` files as the
authoritative material library. There is no database and no hidden copy. The
folder may be backed up, searched, edited with ordinary text tools, or placed in
Git without involving Paperweight-specific infrastructure.

## Library-centred workspace

Paperweight v0.0.22 makes the full library browser the native application's
launch screen and first tab. Opening a material creates an independent editor
session in the same macOS tab group; opening an already-open path selects its
existing editor. Each session owns its material, layer selection, dirty state,
preview products, cancellation token, render queue, controls, and represented
file URL. Changes in one tab therefore cannot leak into another.

The compact navigator on the left of every editor searches friendly names,
categories, and relative paths in the remembered working folder. Double-click
opens a material and **Library Overview** returns to the full browser. The
navigator can be collapsed from the View menu when the preview needs the room,
and refreshes after saves or file operations in the full library.

These are native AppKit tabs rather than an application-specific imitation.
**Window > Move Tab to New Window** and tab dragging detach an editor for a
second display; **Merge All Windows** restores a single-window layout. A closed
editor immediately ceases to count as an open document, while unsaved editors
still receive their own save warning when closed or when the application quits.

Paperweight v0.0.21 can derive a compact `.pwlib` deployment artefact from the
whole folder or selected rows. The readable `.pmat` files remain authoritative;
editing or importing a pack is deliberately unsupported. See
[pwlib-format.md](pwlib-format.md) for its portable in-memory API, checksums,
automatic RLE, command-line packer, and C/C++ embedding route.

The browser shows the source PMAT byte count and prospective raw or RLE payload
size for each material. The footer previews the source total and complete
PWLIB size, including pack overhead, before export. Size calculation uses the
same core encoder as the eventual file rather than an estimate.

## Identity

A library-ready material has a lowercase canonical UUID and a friendly name.
The UID is stable when a material is renamed or moved. Duplicating a material
creates a new UID. Seedless reference templates do not contain identity, so
each authored material receives identity only when the author assigns or saves
it as a library item.

Description, category, and tags are optional. They support browsing and search
but never influence generated colour, height, normal, or roughness. The editor's
Material Information dialogue edits these fields directly.

## Indexing and diagnostics

The AppKit frontend discovers `.pmat` files recursively and passes relative
path plus UTF-8 content to the portable C++ index. The core parses each source,
sorts entries deterministically, and reports malformed files, missing UIDs,
missing names, and every side of a duplicate UID. One broken material never
hides a valid sibling.

The browser searches friendly names, UIDs, descriptions, categories, tags, and
relative paths. Generated 96x96 colour thumbnails are presentation caches in
memory: they can be cancelled and regenerated and are never source assets.
Explicit refresh and window activation pick up external edits.

## Safe operations

New and duplicated materials receive collision-free filenames and stable UIDs.
Friendly rename leaves both UID and filename unchanged. Move accepts only a
destination inside the current working folder and refuses to overwrite an
existing file. Reveal opens Finder at the source file. Ordinary editor saves
notify an open library window so its diagnostics and thumbnail are refreshed.

The table permits multiple selection for pack export. Ordinary Open, Duplicate,
Rename, Move, and Reveal actions remain single-material operations. **Export
Pack** validates every chosen source before showing the save panel. Invalid
PMAT, missing identity, missing friendly name, or duplicate UID prevents an
incomplete game library from being produced.

In v0.0.20, **New Material** opens the Material Design Wizard. Its **Save to
Library** finish assigns a fresh UID, captures friendly metadata, writes
canonical readable `.pmat` text to a collision-free filename in the working
folder, and refreshes the browser. **Edit Material** instead sends the same
ordinary material to the complete editor. No private wizard document is kept.

Both `.pmat` and `.pwlib` are registered with Finder. A source material opens
or selects its own editor tab; a pack opens in the read-only Pack Inspector, from which
one entry can be instantiated with a chosen seed as a new editable material.

## Preview resolution

The editor remembers a preview size of 64, 128, 256, 512, or 1024 pixels square.
The same setting drives the ordinary 2D image, the optional stylised-lighting
bake inputs, and all four 3D preview maps. It is deliberately not serialised:
changing inspection cost must not change material identity or game output.
