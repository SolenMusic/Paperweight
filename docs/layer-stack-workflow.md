# Layer Stack Workflow

Paperweight v0.0.31 makes the existing layer recipe behave like a native macOS
editing surface without changing material evaluation or `.pmat` format 22.

## Selection and order

The table uses standard AppKit selection: click for one row, Shift-click for a
range, and Command-click for discontiguous rows. The top row is composited last;
the bottom row is evaluated first. A multi-selection always retains that relative
evaluation order when it moves or crosses to another document.

Drag any selected row to move the complete selection as one contiguous block.
AppKit supplies the insertion marker and scrolls a long list near its edges.
Option-Up and Option-Down provide a keyboard equivalent. Duplicate and Paste
insert immediately above the highest selected evaluation layer.

## Clipboard contract

Cut, Copy, Paste, Duplicate, Delete, and Select All are active only while the
layer table has keyboard focus, so editing a name or numeric field keeps ordinary
text behavior. A copied selection publishes:

- `com.solenmusic.paperweight.layer-fragment` for safe application paste; and
- a human-readable text representation for inspection or diagnostics.

Paste accepts the typed value only. The envelope has its own format version and
round-trips every current layer variant, operation parameter, transform, mask,
route, opacity, composite mode, colour, and material-channel value. Unknown or
corrupt fragment versions fail without changing the document.

Fragments yield ordered layers, not material documents. The destination keeps
its seed, UID, friendly name, repeat size, relief, and other material-wide
properties. Pasted and duplicated layers receive fresh transient UUIDs in the
editor. Those identifiers are never written to `.pmat` or `.pwlib` and cannot
affect deterministic generation.

## Undo and previews

Add, remove, duplicate, cut, paste, drag, and keyboard movement register one
structural undo operation apiece. Undo restores the complete layer order,
selection, and transient identity set; Redo repeats it. The history is bounded
to 100 operations per editor.

Parameter editing does not yet participate in general material undo. Making a
parameter edit clears older structural undo so undo cannot restore a stack
snapshot that predates and discards that newer work.

Every structural edit computes the conservatively affected material outputs,
cancels or supersedes obsolete preview work, and retains the last complete image
until the newest revision arrives. Generated CPU output, worker-count identity,
and PMAT compatibility are unchanged from v0.0.30.

## Portable API

`paperweight::serialiseLayerFragment` and
`paperweight::parseLayerFragment` live in
`include/paperweight/layer_fragment.hpp`. They have no AppKit dependency and are
covered by lossless round-trip tests across every current layer operation.
