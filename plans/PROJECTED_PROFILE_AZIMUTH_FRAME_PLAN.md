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

## Follow-up: unified resolver (DONE, separate commit)

Plan north and projected azimuth are the same conceptual operation —
"resolve the scrap's stored magnetic frame into the grid-aligned plot
frame" — but they used to resolve through two separate accessors
(`noteTransformAdjustedDeclination()` for north, `resolvedViewMatrix()`
/ `resolvedViewMatrixData()` for azimuth). A placement consumer had to
remember to call **both**, and `cwScrap::mapWorldToNoteMatrix` got it
half-right: it resolved the azimuth while using the raw
`noteTransformation()->matrix()` for north.

That half-application was a **real, pre-existing bug**, not a deliberate
frame choice. A scrap is stored in the note's local frame (no
declination, no convergence) while the plotted stations are grid
aligned, so *every* mapping between the two has to strip both.
`mapWorldToNoteMatrix` backs `guessNeighborStationName`, so with the raw
north a georeferenced plan note names the wrong station — measured on
`scrapGuessNeigborPlan.cw` with a 12.5° declination, `a1`'s neighbor
`a2` is guessed as `a3`. The shot leaders (which already resolved) then
point at one station while the click names another.

Resolved by bundling both resolutions behind one accessor:

- `cwScrap::ResolvedPlacement` bundles the resolved note transform
  (north) and an owned clone of the resolved view matrix `Data`
  (azimuth), plus `caveToPageMatrix()` (running profiles, which build
  their own per-shot rotation) and `worldToPageMatrix()` (plan /
  projected — folds **both** resolutions into one matrix).
- `cwScrap::resolvedPlacement()` is the single public placement
  accessor. `resolvedViewMatrix()` was removed and
  `resolvedViewMatrixData()` made private (it now only builds the view
  half of the bundle). `noteTransformAdjustedDeclination()` stays public
  as the #628-tested resolved-north primitive.
- All three consumers route through it:
  `cwScrapManager::mapScrapToTriangulateInData` (pulls both pieces from
  one call), `cwScrapStationView::updateShotLines`, and
  `cwScrap::mapWorldToNoteMatrix` — the last now resolves north too,
  fixing the half-applied footgun.
- Tests: `testcases/test_cwScrapResolvedPlacement.cpp` verifies
  `worldToPageMatrix()` folds the resolved north (plan) and resolved
  azimuth (projected), and is never the raw composition.
  `testcases/test_cwScrapNoteFrame.cpp` covers the behavior end to end on
  real notes, freezing the fitted transform so the stored north acts like
  a hand-drawn arrow and then adding declination and a CS: shot leaders
  must keep landing on the drawn stations (worst error stays ~0.004
  normalized note units; the raw frame drifts to ~0.19), and every
  neighbor must still be guessed by name.
