# Projected-profile azimuth: magnetic manual input

Tracked as **GitHub #644** (milestone: Summer 2026). Follow-up to the
#628 grid-convergence work (`plans/AUTO_DECLINATION_PLAN.md`). Ships as
its own commit.

> This plan was originally scoped with a per-scrap magnetic/true/grid
> reference frame + combobox + persisted enum. That collapsed during
> design: the frame is fully implied by the scrap's auto/manual mode, so
> there is **no combobox, no `true` option, no persisted enum, and no
> protobuf change**. What remains is below.

## Problem

A projected profile is drawn on a vertical plane at a chosen azimuth,
stored on `cwProjectedProfileScrapViewMatrix`. Today that azimuth is
**always interpreted in grid north** — it feeds straight into the plot
rotation (`Data::matrix()` / `absoluteAzimuth()`) with no
declination/convergence resolution.

Plan scraps already do the right thing: the north-drawing tool
(`cavewherelib/qml/NoteNorthInteraction.qml`) writes `northUp`, and
`cwScrap::noteTransformAdjustedDeclination()` folds declination +
convergence back in to land it on the grid plot (#628). A hand-entered
bearing is a compass bearing → magnetic. Projected profiles are the
only scrap type not treating manual input as magnetic.

## Decision

Make the projected azimuth consistent with plan north:

- **Manual** (drawn or typed) = **magnetic**. Resolve
  `grid = magnetic + declination − convergence` at the `cwScrap`
  boundary, keeping `Data::matrix()` a pure value transform.
- **Auto** (`calculateNoteTransform == true`) = **grid**, derived from
  the plotted (grid-aligned) stations by the existing `cwMinimizer` fit
  (`cwScrap.cpp:553`) — unchanged.

The frame is implied by mode. `calculateNoteTransform` is already
persisted (`cwScrap.h:57`) and already encodes auto-vs-manual, so it
carries the frame for free — no new field, no format change.

## Why magnetic-for-manual (not grid-fixed)

Resolving a manual azimuth through the current declination/convergence
makes the plane **track the cave** when georeferencing is added after
drawing: adding a CS rotates the plot by convergence, and a
magnetic-resolved plane rotates with it, staying aligned to the
passages. A grid-fixed stored value would instead drift relative to the
passages — the surprise this avoids. Auto stays grid because it is
re-derived from the already-resolved plot.

## Design

- `cwScrap`: add the projected-azimuth resolution mirroring
  `noteTransformAdjustedDeclination()` / `planGridConvergence()` —
  compute `grid = magnetic + declination − convergence` from
  `tripCalibration()->declination()` and
  `parentCave()->gridConvergence()->angle()`, and feed that grid value
  wherever the plot rotation is built. Running profiles are untouched
  (gravity-up, no bearing).
- `Data::matrix()` stays pure (grid azimuth in → rotation out).
- UI (`NoteTransformEditor.qml`, projected block ~line 162): no
  combobox. Optionally a static "magnetic" tag beside the field in
  manual mode + a live "= X° grid" readout for reassurance.

## Migration (accepted, see #644)

Existing manual projected azimuths are stored as grid; re-interpreting
them as magnetic shifts a scrap by (declination − convergence) on load.

- **Convergence term: zero on all existing data** — grid convergence
  never existed before #628, so no convergence shift is possible on old
  projects.
- **Declination term:** a projected scrap with a manual azimuth *and* a
  trip declination shifts by the declination. Accepted — tiny
  population, and the new behavior is the more-correct one.

## Tests

Own file (e.g. `testcases/test_cwProjectedProfileAzimuthMagnetic.cpp`),
reusing the `EPSG:32613` fixture from `test_cwScrap.cpp`:

- Manual azimuth resolves `grid = magnetic + declination − convergence`.
- Auto azimuth stays grid (minimizer output unchanged).
- Running profile untouched.
- Non-georeferenced, no-declination project is a bit-for-bit no-op.

## Follow-up: unified resolver (out of scope, not in this commit)

Plan north and projected azimuth are the same conceptual operation —
"resolve the scrap's stored magnetic frame into the grid-aligned plot
frame" — but they resolve through two separate accessors:

- `cwScrap::noteTransformAdjustedDeclination()` folds declination +
  convergence into the note transform (north lives on
  `cwNoteTransformationData`).
- `cwScrap::resolvedViewMatrix()` / `resolvedViewMatrixData()` fold the
  azimuth into the view matrix (azimuth lives on the projected view
  matrix `Data`).

A consumer that places a scrap into the grid plot must remember to call
**both**. Every plot-placement site does so today
(`mapScrapToTriangulateInData`, `cwScrapStationView::updateShotLines`),
but the split is a footgun: `cwScrap::mapWorldToNoteMatrix` already
resolves the azimuth (`resolvedViewMatrix()`) while using the raw
`noteTransformation()->matrix()` for north. That is harmless — north
resolution is a no-op for non-Plan scraps, and this predates #644 — but
it demonstrates the "must apply both, at every site" fragility.

Deeper alternative: a single resolved-scrap-placement accessor (or a
small struct bundling the resolved note transform + resolved view
matrix) that consumers cannot half-apply. Bigger than #644 — the three
consumers layer different per-site factors between the two transforms,
so it is not a trivial merge. Deferred; capture as its own issue if
picked up.
