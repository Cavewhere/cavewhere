# Reprojection unification (follow-up to the `cwPositionItem` contract)

Captured from the pre-commit review of the `inFrustum` / `PositionItem` arc
(the commit that introduced `cwPositionItem` and routed all updater-driven
items through the typed contract). This is deliberately **out of scope** for
that commit and recorded here for a later, self-contained change.

## A1 — One owner of world→viewport reprojection

There are two parallel engines that map a model-space point into Qt view
coordinates:

- `cwTransformUpdater::updatePoint` — now typed (`cwPositionItem`), reads
  `position3D`, writes `inFrustum` + item position. Driven by the camera's
  `projectionChanged`/`viewMatrixChanged`/`viewportChanged`.
- `cwLeadView::updateItemPositions` (`cwLeadView.cpp`) — re-derives the same
  `qtViewportMatrix() * viewProjectionMatrix()` map and reprojects each lead by
  hand, then registers a depth-occluded billboard per lead.

Both now speak the typed `position3D` (`LeadPoint` was rerooted on
`PositionItem` in the reviewed commit), so the remaining split is purely the
*reprojection loop*, not the data contract.

**Goal:** a single owner of world→viewport + frustum membership. Two shapes to
weigh:
- `cwLeadView` subscribes to a `cwTransformUpdater`'s `updated()` and drops its
  own matrix math, OR
- the projection math moves into a small shared projector both call.

**The catch that makes this a design step, not a mechanical merge:** leads carry
an extra per-frame responsibility the updater doesn't provide — billboard
registration + occlusion (`addOrUpdateBillboard`). Unification means the updater
growing an extension point (an `updated()` subscription or a post-position hook)
rather than folding one path into the other. Leads also legitimately don't need
`inFrustum` (occlusion is done in 3D by the billboard) and don't route through
`addPointItem`.

**Blast radius:** medium — `cwLeadView`'s reproject + billboard path, plus
wherever the shared per-frame hook fires. Tests: `tst_Leads`,
`tst_ScrapInteraction`.
