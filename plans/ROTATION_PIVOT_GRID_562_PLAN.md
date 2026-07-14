# Rotation Pivot / Grid Picking — Issue #562

**Problem:** When the user rotates the 3D view by clicking off the centerline, the pivot can
teleport to a distant point on the grid floor plane, swinging the cave off-screen and losing
the rotation context.

Related prior work: `PREVENT_VIEW_TELEPORT_527_PLAN.html` (#527, the `acceptFallbackPoint` gate),
`CENTERLINE_PICKING_530_PLAN.html` (#530, pick radius for the thin centerline).

---

## Findings (verified against the code)

The pivot is chosen by `cwBaseTurnTableInteraction::unProject(QPoint)`
(`cavewherelib/src/cwBaseTurnTableInteraction.cpp:132`). It casts a cursor ray through the single
BVH (`cwGeometryItersecter`, held by `cwScene`) that contains the line plot, scraps, and point
clouds. Current logic:

1. `intersectsDetailed(ray, pickQuery(4mm radius))` — on a hit, pivot = `hit.pointWorld()`, the
   **actual** intersection point (for a line, the closest point on the segment). Pruned by nearest
   depth across all kinds. This path works well; the good on-geometry pivoting seen in testing is
   this, **not** a bbox-center.
2. On a miss, fall back to the **grid plane** (`m_gridPlane`, a `QPlane3D`), gated by
   `acceptFallbackPoint()`.
3. If the grid hit is rejected, return `nullopt` → caller keeps the current pivot (the #527
   no-teleport behavior).

**The grid is the culprit.** Three facts combine:

- The grid is **not in the BVH** (`cwRenderGridPlane`/`cwRHIGridPlane` never call `addObject`). It
  enters picking only as a fallback `QPlane3D`, wired in `GLTerrainRenderer.qml:86`. That fallback
  is an **infinite plane**, not the 6000×6000 quad that is drawn — so it catches rays anywhere,
  including far outside the visible grid.
- The plane sits at **Z = the cave's lowest point** (`cwRegionSceneManager::updateGridPlaneElevation`
  pins it to `minZValue`). Off-centerline clicks resolve the pivot down onto the floor, not at the
  depth of the passage being looked at.
- The only guard is spatial: `acceptFallbackPoint()` accepts a grid hit inside the scene box grown
  by **100% of its diagonal** (`kFallbackBoxMargin = 1.0f`). At a grazing view angle a ray skimming
  the floor plane hits it far out in X/Y, and up to a full scene-diagonal away it is still accepted.

There is **no logic that disables grid picking when objects exist** — geometry only wins when the
ray actually hits it; a miss falls straight through to the grid.

**Confirm the diagnosis** by enabling the `cw.interact` (`lcInteract`) log category and rotating:
off-geometry clicks print `unProject(...): gridPlane world=...` at floor Z far from the cave.
(Note: hiding the grid's *rendered* visibility does **not** change picking — the pivot fallback is
`m_gridPlane`, independent of the drawn quad.)

The point-cloud sphere pick + tube-pick (`NearMissResult`, `tryPromoteNearMiss`, `kTubeFactor`,
`setTubePickEnabled`) live in the shared `cwGeometryItersecter`, which is **also** used by the
measurement tool, the coordinate picker (`cwScenePick::snappedPoint`), and lead occlusion
(`cwLeadView`). Those need exact point hits, so that machinery must stay. It is not rotation-specific.

---

## Phase 1 — Gate the grid on "no objects" (minimal fix, commit first)

**Goal:** when survey geometry exists, an off-geometry click must never pivot on the grid. Keep the
grid pivot only for the empty/unloaded project, where it is the sole reference.

**Change:** in `unProject()` (`cwBaseTurnTableInteraction.cpp:132`), only take the grid-plane
fallback when the scene BVH bounding box is null/non-finite (no geometry). When objects exist and the
ray misses geometry, return `nullopt` → caller keeps the current pivot (no teleport).

This overlaps `acceptFallbackPoint()`'s existing empty-scene branch (lines 41-48); the cleanest
expression is to make the "geometry exists" case skip the grid entirely rather than rely on the
loose 100%-diagonal box test. Consider removing/retiring `kFallbackBoxMargin` and the box-grow test
if the object-existence check fully supersedes it (the box test only ever mattered when geometry
existed — exactly the case we now short-circuit).

**Behavior after Phase 1:** off-geometry click with objects present → pivot stays put (stable, no
context loss). Empty project → grid still works. This is a strict improvement over the teleport and
is what Phase 2b builds on.

**Tests:** QML/interaction test that, with a loaded region, a rotate drag starting on empty space
does not move `center`; with an empty region, it still resolves to the grid plane.

**→ User reviews and commits Phase 1 before Phase 2b starts.**

---

## Phase 2b — bbox-center fallback (jedwards' idea)

**Decision (agreed):** exact-hit stays **primary**; bbox-center is the **fallback** that replaces
the "keep current pivot" gap left by Phase 1. Rationale: the exact hit is the same single
`intersectsDetailed` call (free) and gives a precise pivot right on the centerline — the thing
cavers most want to orbit. bbox-center only rescues near-misses.

Rejected alternative (pure bbox-center for rotation): would drop the 4 mm pick-radius plumbing and
unify the path, but a long survey shot is one BVH leaf, so its bbox-center is the segment
**midpoint** — clicking the far end of a 30 m shot pivots ~15 m from the cursor. Unacceptable for the
primary centerline case.

**New pivot order in `unProject()`:**

1. Exact hit (`intersectsDetailed`, unchanged) → `hit.pointWorld()`.
2. **New:** nearest-BVH-node-center for a bare ray → center of the smallest/deepest BVH node whose
   AABB the ray passes through (enter+exit). Geometry-anchored, stays near what is on screen, and
   naturally catches thin lines and sparse point clouds without a pick radius or the tube-pick.
3. Grid plane — only when no objects (Phase 1 rule).
4. `nullopt` → keep current pivot.

**New API on `cwGeometryItersecter`** (needed — no existing call returns this): something like
`std::optional<QVector3D> nearestNodeCenter(const QRay3D& ray) const`, traversing the two-level BVH
with a bare ray-vs-AABB test, descending to the tightest node the ray still intersects, returning its
world-space center. Reuses the existing top-level + sub-BVH structure; no new build cost.

**Point clouds:** no change to the intersecter's sphere/tube machinery (measurement/coordinate/lead
tools still need it). The tube-pick simply becomes irrelevant *for rotation* — a between-points click
now falls to bbox-center instead of needing the near-miss promote.

**Open questions for 2b design:**
- Define "smallest node hit" precisely: deepest node whose AABB the ray enters and exits. If the ray
  only clips the top-level (whole-scene) box but no child, the fallback is the scene-center — is that
  desirable for a just-off-the-cave click, or should it require a leaf-level hit?
- Does `nearestNodeCenter` respect kind filters (should it ignore, e.g., hidden objects)?

**Tests:** off-geometry-but-near-cave click resolves to a geometry-anchored point (not the grid, not
floor Z); long-shot far-end exact click still pivots on the line, not the midpoint.

---

## Out-of-scope follow-ups (from the Phase 1 pre-commit review)

Notes only — not part of any committed phase yet. Triage later.

Refactors this change makes worthwhile:

- **Cheap `isEmpty()` on `cwGeometryItersecter`.** Phase 1's `hasGeometry` calls the O(n)
  `boundingBox()` union (`cwGeometryItersecter.cpp:652`) just to test null/finite. The BVH root
  already carries a bbox, so an O(1) emptiness check is available. Not urgent (runs on gesture-start,
  not per frame), but tidy. Would also simplify `unProject`.
- **Move the grid-plane fallback out of QML.** It's plumbed `regionSceneManager.gridPlane.plane` →
  `GLTerrainRenderer.qml:86` → interaction (`setGridPlane`), while the geometry it's a fallback *for*
  lives on `cwScene`. One picking concern split across three owners; `cwScene` has no grid knowledge.
- **Phase 2b: consolidate the pivot ladder.** When `nearestNodeCenter` lands, make the
  exact→node-center→grid→nullopt chain a single `resolvePivot(ray)` policy object rather than another
  inline `if` chain in the QQuickItem-derived interaction. The plan's open questions (leaf-vs-top-level
  node, kind filtering) are pick-policy decisions that don't belong in the turn-table.

Larger architectural changes:

- **Back-end scene-picking service.** Four consumers (this interaction, `cwScenePick::snappedPoint`,
  the measurement tool, `cwLeadView`) each re-implement ray-build + snap/fallback policy; ray
  construction is copy-pasted between `unProject` and `pick` in this file alone. A service owning
  ray-building + per-consumer kind-filtered policy would DRY all four and make pick semantics
  unit-testable without QML. Blast radius: medium-large (new class + 4 call sites).
- **Grid as a real bounded pick object (the #562/#527 root cause).** The grid is drawn as a finite
  6000×6000 quad but picked as an *infinite* `QPlane3D`, so it catches grazing rays arbitrarily far
  out. Modeling it as a bounded / first-class "reference plane" pick layer removes the teleport at its
  source. Caveat: it can't naively join the shared BVH — the measurement/coordinate/lead pickers must
  NOT hit the grid — so this only works alongside the kind-filtered picking service above.
