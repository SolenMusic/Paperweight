# Material Design Wizard

Paperweight v0.0.20 adds a guided path from a broad idea to a useful editable
material. It is intended for someone who knows that they want, for example, a
worn stone wall but does not want to begin by assembling a layer stack.

## Workflow

Choose **File > New Material** or **New Material** in the working-folder
library. The wizard preserves progress while moving through four steps:

1. **Family** - choose masonry, stone, wood, metal, organic, foliage, gravel or
   debris, or abstract, then choose a suitable seedless starting point. The
   catalogue begins with the v0.0.18 reference templates and also promotes
   proven bundled recipes where a family needs its own honest material.
2. **Scale** - choose a useful physical-size preset or enter the width and
   height, in metres, covered by one seamless repeat. A summary translates that
   into millimetres per preview pixel without confusing physical size with
   image resolution.
3. **Design** - choose a starting seed, two ordinary material colours, and the
   template's compact construction, surface, and wear controls.
4. **Choose** - generate four deterministic alternatives, compare their
   thumbnails, and inspect the selected result in the full-size 2D or 3D
   preview.

Locks are available for seed, physical size, the colour pair, and every friendly
template control. Alternative generation changes only unlocked values. The same
session and seed always produce the same candidates in the same order; candidate
identity never depends on preview resolution, pixel traversal, worker count, or
thread scheduling.

## Finishing

**Edit Material** opens the selected result in the complete editor, where every
layer and operation remains accessible. **Save to Library** assigns a new UID,
collects a friendly name, category, and tags, chooses a collision-free filename,
and writes directly into the remembered working folder.

Both routes produce an ordinary `Material`. Saving serialises the same readable
version-16 `.pmat` text used elsewhere in Paperweight. Wizard state, locks,
family selection, comparison thumbnails, and preview mode are not serialised.

## Architecture

The portable `MaterialWizardSession` owns only a seedless `MaterialRecipe`,
physical size, colour pair, typed template-control values, and lock state. It
uses the v0.0.18 template catalogue plus selected existing showcase recipes and
`applyTemplateControl` to construct a normal material. Showcase sources are
lifted into `MaterialRecipe`, which deliberately removes their seed and
identity before the caller chooses a variation. The AppKit controller owns
pages, controls, background preview work, and library file panels.

The family catalogue deliberately follows the substance of a recipe rather
than incidental details within it. Metal therefore offers Painted Metal and
Weathered Metal, not a wooden crate merely because that recipe happens to
contain nails. Wood also offers Knotty Wood; stone includes Graphic Marble and
Mossy Pebbles; gravel or debris includes Scattered Debris and Mossy Pebbles;
and abstract includes Graphic Marble and Ember.

This deliberately leaves one material engine, one layer/graph vocabulary, and
one file format. Automatic image recreation, AI-authored recipes, visual node
graph editing, and wizard-only special generators remain outside v0.0.20.
