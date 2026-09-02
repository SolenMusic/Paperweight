# Working Folder and Material Library

Paperweight v0.0.19 treats a normal folder of readable `.pmat` files as the
authoritative material library. There is no database and no hidden copy. The
folder may be backed up, searched, edited with ordinary text tools, or placed in
Git without involving Paperweight-specific infrastructure.

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

## Preview resolution

The editor remembers a preview size of 64, 128, 256, 512, or 1024 pixels square.
The same setting drives the ordinary 2D image, the optional stylised-lighting
bake inputs, and all four 3D preview maps. It is deliberately not serialised:
changing inspection cost must not change material identity or game output.
