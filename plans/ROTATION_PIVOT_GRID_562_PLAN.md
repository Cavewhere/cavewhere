# Rotation Pivot / Grid Picking — Issue #562

**Problem:** When the user rotates the 3D view by clicking off the centerline, the pivot can
teleport to a distant point on the grid floor plane, swinging the cave off-screen and losing
the rotation context.

Related prior work: `PREVENT_VIEW_TELEPORT_527_PLAN.html` (#527, the `acceptFallbackPoint` gate),
`CENTERLINE_PICKING_530_PLAN.html` (#530, pick radius for the thin centerline).

---

## SESSION STATUS (read first — updated 2026-07-15)

**Committed (branch `dev_4`, ahead of `origin/dev`, none pushed):**
- `5d7146f2` Phase 1 (grid gated to no-geometry; keep-pivot on miss).
- `8c95450d` Unrelated crash fix: `dumpLeafPrimitive` had no `Line` branch → read a 3rd index →
  SIGABRT when `cw.picking` logging is on. Fixed + regression test in `test_..._linePick.cpp`.
- `2f692f12` Phase 2b — rotate around survey geometry instead of distant point cloud data. The
  "uncommitted working tree" described below landed here, plus the watertight pick radius
  (`PointPickRadiusScale = 1.0`, was 0.5 = below the sqrt(2)/2 threshold) that fixed picking *through*
  a cloud to a far point.
- Tube-promote deletion — the near-miss fallback is gone; see "Review 3 findings" at the end.

**Next up:** the two items marked **next up (2026-07-15)** in the triage index — unify the two BVH
traversals, and the radius-systems naming fix. Read Review 3 first; it settles the direction on both.

**The section below describes the state BEFORE `2f692f12` and is kept as the design record.
"Uncommitted working tree" now means "landed in `2f692f12`".**

**Then-uncommitted working tree (Phase 2b — now committed):**
- `cwGeometryItersecter::nearestGeometryPoint(ray, query)` — the closest point ON real geometry
  (iteration 3; see THE FIX below). Supersedes `nearestNodeCenter`, which is deleted.
- `unProject(point, bool anchorToNearestGeometry=false)` — the anchor is gated behind the flag;
  **only rotation passes true**. Pan (`startPanning`) and zoom (`bindPerspectiveIntersection`) keep
  the default, fixing a pan-jump / zoom-drift regression (off-ray anchor). Tested:
  "startPanning on a near-miss does not jump the view" (ortho + perspective).
  **Correction (review 2): the default is NOT an "on-ray point" — earlier drafts of this line said so
  and it is false.** Only a *triangle* hit is on-ray; a line hit returns the closest point on the
  segment and a point hit returns the vertex center, so rung 1 is off-ray by up to ~4mm of screen.
  The gate narrows the error 20mm → 4mm; it does not eliminate it. Two consequences, both indexed as
  follow-ups: pan lurches ~15px on a centerline grab (pre-existing), and `m_center` — which rotation
  writes the 20mm anchor into — is also zoom's miss fallback via `value_or(m_center)`, so the anchor
  reaches zoom regardless of this flag.
- Tests: `test_cwGeometryItersecter_nearestPoint.cpp` (new — **untracked, needs `git add`**) plus
  reworked rotation tests (anchor-to-nearest-point, hidden-object-skip, far-end-exact-primacy,
  point-cloud-gap keeps pivot).

**Iteration history below is kept as the record of WHY the design is what it is — iterations 1 and 2
are NOT the current code.**

**The point-cloud bug that killed iteration 1 (confirmed empirically, fixed by iteration 3):** a
whole point cloud is ONE
BVH object (`cwRenderPointCloud.cpp:42`, `Key{this,0}`, `pickRadius = 0.5*meanSpacingXY`), so its
sub-BVH root ≈ the whole cloud. Exact/tube picking covers clicks within `2.5*spacing` of a point;
`nearestNodeCenter` only fires on clicks farther out (sparse LiDAR, gaps, grazing the edge). There,
when the ray crossed the sub-BVH root but missed both children (a gap/grazing ray), the ONLY candidate
was the root and it returned the **whole-cloud centroid at the wrong depth** → rotation teleport, and
the corrupted `m_center` then poisoned pan (miss-anchor depth = center depth) and zoom
(`value_or(m_center)`) = "rotation AND translation broken for point clouds".

**FIX ITERATION 2 — REJECTED by user (leaf-only + front-most `nearestNodeCenter`):** restricting
anchors to crossed BVH leaves killed the centroid teleport but starved the anchor everywhere: tight
leaf boxes mean a near-miss ray crosses nothing, so most off-geometry rotates degraded to keep-pivot
("still doesn't work for point based picking and the picking is worse for all other types").

**THE FIX (iteration 3, this session, uncommitted): `nearestGeometryPoint(ray, query)` — anchor on
the closest point ON real geometry, no box centers at all.** `nearestNodeCenter` is deleted. The new
query returns the closest point on a primitive to the cursor ray — a cloud point, the nearest spot on
a shot segment, or a spot on a scrap triangle (exact hit, else nearest of its 3 edges via
`raySegmentClosest`) — accepted only within the query's screen-space tolerance at that depth
(`radiusAt(t)`, same `cwPickQuery` model as line picking), front-most (smallest `tWorld`) among
accepted candidates, mirroring every other pick's nearest-by-depth rule. Reuses
`raySphereIntersectDouble` (radius 0 → always-filled double `dSq`), `raySegmentClosest`,
`rayTriangleMT` (cull off — anchors are orientation-agnostic), and the conservative
radius-at-farthest-scene-depth box pad from the line-pick path. Rotation calls it with a NEW public
constant `cwBaseTurnTableInteraction::PivotAnchorRadiusMillimeters = 20.0` (vs 4mm exact pick) —
"grab what I'm looking at" reach; farther clicks keep the pivot. Pan/zoom still never anchor
(`anchorToNearestGeometry` stays rotation-only).
Why this satisfies all the constraints at once: point clouds anchor on an actual point at the right
depth (the literal "rotate around a point cloud point" ask); line near-misses snap ONTO the shot (not
a segment-box midpoint); scrap/LiDAR-note near-misses snap onto the surface/edge (better than the old
node centers); nothing within 20mm → keep pivot (no teleports, ever — there is no aggregate candidate
to teleport to).
Tests: `testcases/test_cwGeometryItersecter_nearestPoint.cpp` (exact-point anchor in/out of
tolerance; front-most-in-tolerance wins; line snap; triangle exact/edge/far-miss; two-cluster gap →
nullopt at small radius, real cluster point at large radius — never a centroid; perspective slope
tolerance) + interaction tests reworked (anchor snaps to the actual point, hidden-object skip,
point-cloud-gap keeps pivot with the anchor radius derived in-test via
`pixelsForMillimeters`+`pickQuery`). All green: `[nearestPoint]` 6 cases,
`[cwBaseTurnTableInteraction]` 59 cases, `[cwGeometryItersecter]` 55 cases.

**STATUS (2026-07-14, after user re-test): iteration 3 WORKS WELL on real data — user approved,
pre-commit code review is the next step.** The earlier "still broken" report was likely the LAZ
cloud's BVH still building: until the async build publishes, exact pick AND anchor return nothing, so
every click keeps the pivot — indistinguishable from "picking doesn't work". Possible follow-up (not
requested): surface build-in-progress to the user (cursor/status hint) so an unaccelerated cloud
doesn't read as broken picking. Tuning knob if reach feels wrong: `PivotAnchorRadiusMillimeters`
(20mm).

**Pre-commit review (2026-07-14) — fixes applied on top of iteration 3:**
- **Exit-t prune bug (found by review, reproduced, fixed).** `QBox3D::intersection(ray)` returns the
  ENTRY parameter only while it's positive and silently switches to the EXIT parameter once the ray
  origin is inside the box (`QMath3d/qbox3d.cpp:404-414`). The prune `if (found && tBox >= bestT)`
  reads that as a lower bound on the node's candidates, so an origin-inside node was compared on its
  far side and skipped while holding the nearest candidate. The tolerance pad makes this reachable —
  it inflates every box by the accept radius at the scene's far depth, which routinely swallows the
  camera. Fixed with `boxEntryDistance()` (two-arg overload, entry clamped to 0). Pinned by
  "nearestGeometryPoint: a node containing the ray origin is not pruned away", which failed on the
  unfixed code (returned the t=50 point instead of the t=5 line spot). **The same pattern is still in
  `intersectsDetailed`** — see the follow-ups.
- **Null-deref in the debug MISS dump (fixed).** `intersectsDetailed`'s per-Object summary walked
  `subBvhs` and dereferenced every slot with no null guard, while every sibling site has one. Now
  prints `[i] <rebuilding>` and continues. **Correction (review 2):** an earlier draft of this entry
  called `8c95450d` an incomplete fix for the same bug. That is wrong — `8c95450d` fixed an *index
  overrun* (`dumpLeafPrimitive` read a 3rd index on a 2-index segment). This was a *different* defect
  that happens to live in the same debug dump. Not a race either: `invalidatePublishedSlot` is
  copy-on-write, so a slot inside a published `BvhData` never goes non-null → null; the guard handles
  a legitimate persistent state of an immutable snapshot. Both reviewers confirmed the intersecter is
  main-thread-only (the render thread never references it) and that all six slot derefs are now
  guarded. The real lesson is in the follow-ups: the debug dumper is an unexercised parallel walk of
  the BVH, and both crashes on this branch came out of it.
- **Grid gate now visibility-aware (fixed).** `unProject` gates on `isPickableEmpty()` instead of
  `boundingBox()`. See the follow-up entry for what this does and deliberately doesn't do.
- Simplify pass: extracted the verbatim-duplicated 8-corner pad derivation into
  `conservativeTolerancePad()`; cut the `unProject` contract from four copies to one (one had already
  drifted, still describing the deleted `nearestNodeCenter`); strengthened the gap test (its
  `z >= 39` assertion passed on the near cluster's own box centre at z=41 — i.e. it could not catch
  the regression it existed for) and the front-most test (now both insertion orders).

**MEASURED (spike, 2026-07-15): the anchor was O(N) in cloud size on a miss — the BVH did nothing.**
Throwaway spike (deleted; Debug, unoptimized — the *scaling* is the result and it is
build-independent). **Correction (review 2): these numbers are NOT ASan-inflated.** Earlier drafts
said "Debug+ASan, inflated several-fold"; `build/Qt_6_11_1_for_macOS-Debug` has
`ASAN_ENABLED:BOOL=OFF`, and the caches with `ASAN_ENABLED=ON` are Release configs where the
`$<$<CONFIG:Debug>>` genex never fires. So Debug→Release is maybe 2-5x, not the 5-20x assumed below,
and the extrapolation at the end of this section is correspondingly overstated.
Synthetic cloud, x/y in [-50,50], z in [-500,0] (wide depth
spread), perspective tolerances derived exactly as `cwCamera::pickQuery` does (20mm → slope 0.0787,
4mm → 0.0157), camera at the close end, ray that misses the exact pick and anchors successfully — i.e.
**the real production miss path**:

| points | exact 4mm (miss) | anchor 20mm (added) | ns/point |
|--------|------------------|---------------------|----------|
| 61 k   | 0.342 ms         | 3.116 ms            | 50.9 |
| 120 k  | 0.378 ms         | 6.023 ms            | 50.4 |
| 239 k  | 0.330 ms         | 11.457 ms           | 48.0 |
| 489 k  | 0.252 ms         | 23.226 ms           | 47.5 |
| 956 k  | 0.313 ms         | 38.561 ms           | 40.3 |

15.6× the points → **12.4× the anchor time** (linear) vs **0.92× the exact time** (flat). Constant
~40-50 ns/point says it plainly: the anchor touches a constant *fraction* of the cloud — i.e. O(N) —
while the exact pick's BVH prunes to ~O(log N). (An earlier draft said "every point in the cloud";
constant ns/point does not establish that, only that the work scales linearly.)
**Pruning does not rescue it even when the anchor succeeds** — this ray
anchors, so `found` is true and the `found && tBox >= bestT` prune is live, and the cost is still
linear. The pad is scene-scale, so almost nothing is prunable. That refutes the reviewers' hope that
the `found == false` case was the only pathological one; the ordinary near-miss is linear too.
Extrapolating (Debug+ASan → Release is maybe 5-20× faster, so treat as an upper bound): ~1M points
≈ 2-8 ms added per rotate-start; 10M ≈ 20-80 ms; 100M ≈ 0.2-0.8 s. That last one is a visible hang on
mouse-down, once per rotate.
Why "works well" on real data anyway: the cost is only paid on a rotate-start that MISSES (a hit
returns at the exact rung and never calls the anchor — confirmed: `unProject` returns at rung 1
before the anchor block, so on a hit the 20mm anchor and `isPickableEmpty()` both cost exactly zero),
and it scales with cloud size — a modest cloud is imperceptible.

**MEASURED AFTER the per-node pad fix (same spike, same build) — the fix works: ~40× at ~1M points.**

| points | scene-global pad | per-node pad | speedup | ns/point (before → after) |
|--------|------------------|--------------|---------|---------------------------|
| 61 k   | 3.116 ms         | 0.259 ms     | 12.0×   | 50.9 → 4.2 |
| 120 k  | 6.023 ms         | 0.331 ms     | 18.2×   | 50.4 → 2.8 |
| 239 k  | 11.457 ms        | 0.436 ms     | 26.3×   | 48.0 → 1.8 |
| 489 k  | 23.226 ms        | 0.718 ms     | 32.3×   | 47.5 → 1.5 |
| 956 k  | 38.561 ms        | 0.957 ms     | 40.3×   | 40.3 → **1.0** |

15.6× the points → **3.7× the time** (sublinear, ns/point falling — the BVH is pruning again). The
exact pick is the control and stayed flat throughout (0.342 → 0.313 ms = 0.92×), proving the harness
and the BVH were never the problem. Pinned by the `[performance]` regression test: ratio **1.31×**
with the fix, **34.75×** with it reverted (fails the 10× threshold). ~7.6× headroom below, ~3.5×
above. Verified non-flaky by review 2: 48 runs at 8-way concurrency under 32 spinners on 16 CPUs gave
a 1.302–1.327× range (±1% spread), 0 failures, 0.099 s per case.

**Caveat on the control (review 2, conf 85): it could not have caught the same bug in
`intersectsDetailed`.** That path folds the root pad in *only for line objects*
(`subPad = isLineObject ? max(subTubeDist, linePad) : subTubeDist`), and the spike's scene is a point
cloud — so `linePad` is never folded in and the control is structurally blind to it. See the
follow-ups: the centerline still has the scene-global pad, on a **per-frame** path.

**Reproducing the pad/latency stall by hand — what to try:** zooming out alone is
not quite the lever. The pad is `radiusAt(farDepth)` where `farDepth` is the ray-projected depth of
the **root box's far corner**, and it's applied to every node at every level. So the blow-up factor is
`farDepth / nodeDepth` — i.e. it needs **depth range**, not just distance. Recipe: a large LAZ cloud in
a deep scene (a long cave, or the cloud plus distant survey), **perspective** projection (ortho's
`tolerance.constant` doesn't grow with depth — only `slope` does, and `cwCamera::pickQuery` only sets
`slope` for perspective), camera positioned **close to one end** so near geometry sits at small t while
the far end pushes `farDepth` large. Then rotate-start on a gap *near the close end*, where the local
accept radius is small (so nothing is accepted → `found` stays false → the prune never engages) but
every box is padded by the far-scene radius. Zooming out increases `farDepth`, but it also increases
the accept radius at the cloud's own depth, which makes a candidate more likely to be found and
re-enables pruning — so a zoomed-out view of a shallow scene may not reproduce it. If a manual repro is
awkward, the honest measurement is a headless timing test: synthetic cloud with a wide depth spread,
compare `nearestGeometryPoint` vs `intersectsDetailed` wall time on a no-candidate ray.

**Build/test:** `build/Qt_6_11_1_for_macOS-Debug`; note other build dirs still carry a stale
`MacOSX26.2.sdk` sysroot and need `-DCMAKE_OSX_SYSROOT=.../MacOSX.sdk` on next rebuild.

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
2. **New (final form):** `nearestGeometryPoint(ray, query)` → the closest point ON a primitive within
   `PivotAnchorRadiusMillimeters` (20mm screen-space) of the cursor, front-most among accepted
   candidates. Geometry-anchored at full tolerance reach for every kind (points and triangles too,
   unlike the exact pick).
3. Grid plane — only when no objects (Phase 1 rule).
4. `nullopt` → keep current pivot.

**New API on `cwGeometryItersecter`** (needed — no existing call returns this):
`std::optional<QVector3D> nearestGeometryPoint(const QRay3D& ray, const cwPickQuery& query) const` —
two-level BVH traversal with tolerance-padded box tests; per-primitive closest-point via
`raySphereIntersectDouble` / `raySegmentClosest` / `rayTriangleMT`+edges. No new build cost.

**Point clouds:** no change to the intersecter's sphere/tube machinery (measurement/coordinate/lead
tools still need it). The tube-pick simply becomes mostly irrelevant *for rotation* — a
between-points click reaches the nearest point through the anchor's much wider tolerance.

**Caller scoping — rotation only (regression fix during Phase 2b testing).** The node-center anchor is
a point *off the cursor ray* (the ray only crossed the node's AABB; its center is offset). That is fine
for a rotation pivot — you can orbit an off-ray point — but it is wrong for **pan** and **zoom**, which
both call the same `unProject()`. `startPanning` sets the pan anchor to `unProject`'s result and builds
the pan plane through it; an off-ray anchor makes the first pan tick's `intersection − m_panAnchor`
non-zero, so the view lurches. Zoom-to-cursor drifts laterally for the same reason. Fix: `unProject`
grows a defaulted `bool anchorToNearestGeometry` (default false) that gates step (2); only
`startRotating` passes `true`. Pan (`startPanning`) and zoom (`bindPerspectiveIntersection`) keep the
honest on-ray result (exact hit → gated grid → nullopt→`rayPointAtCenterDepth`). Covered by
"startPanning on a near-miss does not jump the view" (ortho + perspective sections).

**Design decisions (agreed):** the goal throughout is anti-teleport, so pin the fallback to real
geometry under the cursor and never let it jump to a cross-object aggregate.

The two-level BVH gives five levels of granularity, coarse → fine:
1. `topLevel` **root** box — whole-scene union (the big jump; forbidden as a pivot)
2. `topLevel` **inner** nodes — groups of several objects (forbidden)
3. `topLevel` **leaf** — one object's world box (line plot, one scrap, one cloud)
4. sub-BVH **inner** nodes — spatial subdivisions within one object
5. sub-BVH **leaf** — a small primitive cluster (≈ a segment / a few points)

Rule (REVISED TWICE — final form in SESSION STATUS): box centers turned out to be the wrong currency
at every level. The original "globally tightest crossed node" teleported to inner-node centroids on
point-cloud gap clicks (levels 1–4 boxes contain the empty gap between their children); the leaf-only
revision fixed that but starved the anchor (tight leaf boxes mean a near-miss ray crosses nothing).
The final rule drops nodes entirely: anchor on the **closest point ON a primitive** within a
screen-space tolerance (`nearestGeometryPoint`), front-most among accepted candidates. The BVH is
only a traversal accelerator (boxes padded by the radius at the farthest scene depth, as in line
picking), never a candidate source — so there is no aggregate center to teleport to, and reach is
governed by one tunable screen radius instead of box-crossing luck.

Why global-tightest rather than "nearest object first": the line-plot object's world box spans the
whole cave (union of every centerline segment ≈ the scene box), so a nearest-by-object-box-entry rule
would lock onto the line plot almost always and ignore a scrap or point cloud with a tighter node right
under the cursor. Smallest-box-globally sidesteps that and is a single-criterion, single-traversal pass.
- **Hidden objects:** always ignored. Reuse the existing per-object visibility hoist
  (`cwRenderObject::isVisible()`, `cwGeometryItersecter.cpp:948`) that `intersectsDetailed` already applies.
- **Anti-teleport backstop:** if the ray reaches no single object (only clips the whole-scene box),
  return `nullopt` → caller keeps the current pivot. Never pivot on scene-center.

**Tests:** off-geometry-but-near-cave click resolves to a geometry-anchored point (not the grid, not
floor Z); long-shot far-end exact click still pivots on the line, not the midpoint; hidden object is
skipped (pivot holds, does not anchor).

**Known residual — RESOLVED (was the Phase 2b pre-commit review's grazing-centroid finding,
confidence ~66):** the predicted failure shipped as the point-cloud regression ("rotation and
translation no longer work for point clouds"). Iteration 2's leaf-only rule killed the teleport but
starved the anchor, so the fix that shipped is iteration 3: with no box centres anywhere, a gap or
grazing ray simply finds nothing within tolerance and the pivot holds — there is no aggregate
candidate to teleport to. Regression-tested in `test_cwGeometryItersecter_nearestPoint.cpp` ("a gap
ray through a large point cloud anchors nothing") and the "startRotating in a point-cloud gap keeps
the pivot" interaction test.

---

## Out-of-scope follow-ups (Phase 1 + Review 2 pre-commit reviews)

Notes only — not part of any committed phase yet. Triage later.

### Triage index

Everything deferred off this branch, in rough value order. Detail for each is in the prose below and
in "Review 2 findings" at the end of the file — this is the index, not a second copy. Size is a rough
commit-shape guess, not an estimate.

`▸ line N` pointers were correct when written and rot on the next edit; each item's bolded title is
repeated verbatim at the target, so grep that if the number has drifted.

**Correctness / latency — earn their way in first**

- [ ] **`intersectsDetailed`'s scene-global line pad** — the per-node fix already written next door,
  applied to the path that runs *per mouse-move* instead of once per press. The centerline is padded
  by the radius at the scene's far corner (`pickRadius = 0.0f` means nothing competes with it).
  Needs a **line-object** perf test; the existing point-cloud control is structurally blind to it.
  *Size: small fix, real test.* ▸ line 478
- [ ] **Exit-t prune pattern in `intersectsDetailed`** (`:921`, `:1064`) — same one-line
  `boxEntryDistance()` helper fixes it. Latent (small pads rarely swallow the camera), but it touches
  leads / `cwScenePick` / measurement, so it wants its own regression test and a full pick-suite run.
  *Size: one line + test + broad re-run.* ▸ line 504
- [ ] **Pan lurch on a line or point grab** — rung 1 returns an off-ray point, `startPanning` uses it
  as `m_panAnchor`, and the first tick translates by the miss distance (~15px at 4mm). Pre-existing
  and byte-identical on `@{upstream}`. A triangle hit gives exactly 0, so 0 is the intent. Fix =
  project the rung-1 hit onto the ray for on-ray callers. *Size: small, but a behavior change.*
  ▸ line 636

**Test integrity**

- [ ] **`startPanning on a near-miss…` can't catch its own invariant** — its click misses, so the
  anchor is on-ray by construction and `translateAmount ≈ 0` trivially. The path that *can* violate it
  (a rung-1 hit on a point or line) is never exercised. A test that did would fail today — so this is
  really the pan-lurch item's regression test. *Size: small, blocked on the decision above.* ▸ line 644
- [ ] **Coverage gaps in the anchor tests** — each ships a green suite while the behaviour breaks.
  ▸ line 516
- [ ] **Hoist duplicated test helpers to `testlib/`** — `makeLineObject` is byte-identical across two
  files; `makeTriangleObject` / `ortho()` / `cluster18` are near-duplicates. Candidate:
  `cwPickTestHelpers` alongside `LazFixtureHelper`. *Size: mechanical.* ▸ line 511
- [ ] **`kPixelTolerance` used as a world-space margin** (`test_cwBaseTurnTableInteraction.cpp:872,995`)
  — it's documented as a *pixel* tolerance for `project()` comparisons. Works today; a retune would
  silently loosen world assertions. Wants its own `kWorldTolerance`. *Size: trivial.*

**Cleanup**

- [ ] **`isPickableEmpty()` computes a full box union to answer a boolean** — could return `false` at
  the first pickable finite box. Cold path; cleanliness, not latency. *Size: trivial.*
- [ ] **Unprotected `std::max`** at `cwGeometryItersecter.cpp:366,391` (Windows min/max macro safety).
  Deliberately left — ~12 other unprotected uses in the same file, so fix all 14 or none.
  *Size: trivial, whole-file.*

**Design — need a decision before they're worth scoping**

- [ ] **`m_center` carries three semantics** (orbit pivot / pan depth reference / zoom fallback) —
  the reason the bool exists, and the reason the gate can't fully isolate rotation's off-ray value
  from zoom. Splitting it touches `viewState` serialization, `centerLocked`, `centerOn`,
  `framingViewState`, and the QML `center` property. *Size: large.*
- [ ] **`anchorToNearestGeometry` is named for the mechanism, not the requirement** — the real axis is
  *"must the result lie on the cursor ray?"* → `RayConstraint::{OnRay, AnyPoint}`, and then the ladder
  falls out of the constraint instead of being a correctly-ordered if-chain. Explicitly **not** a
  context struct (that rule targets virtual/interface methods; `unProject` is private and non-virtual).
  Pairs with `resolvePivot`. *Size: medium.* ▸ line 529
- [ ] **Consolidate the pivot ladder into `resolvePivot`** — moves product policy out of a low-level
  input handler. Also gives the two tuning knobs one home (today `kPivotPickRadiusMillimeters` is
  anon-namespace in the `.cpp` and `PivotAnchorRadiusMillimeters` is public in the `.h`). ▸ line 529
- [ ] **Unify the two BVH traversals behind a per-kind accept policy** — **next up (2026-07-15).**
  ~340 lines, ~80% shared. The tube-promote deletion made this the strongest item on the list: the
  fallback was a *third* variant of the same traversal, and removing it states the architecture
  cleanly — exact picking and off-ray anchoring are two policies, off-ray is opt-in via a separate
  entry point. Keep that split; delete only the copy-paste. Not blocked on the picking service.
  *Size: large; hot path, wants its own commit + profiling.* ▸ line 459
- [ ] **Reconcile the two radius systems — a naming fix, NOT a behavior fix** — **next up (2026-07-15).**
  (`cwPickTolerance` screen-space vs `pickRadius` world-space; which applies is decided by call order in
  `unProject`, not by stated policy.) Review 3 settled the direction: points must **not** start
  consulting the screen-space tolerance — that is the off-ray snap this branch just deleted. Document
  `tolerance` as Lines-only and rename `cwScenePick`'s `pixelRadius` → `lineToleranceMillimeters`.
  *Size: small; no behavior change.* ▸ line 471
- [ ] **Make the queries static-on-`QuerySnapshot`**, like `pointsInBox` already is — they read nothing
  but `BvhData`. Drops the main-thread constraint, lets a picking service hold a *value* instead of a
  back-reference to a mutable subsystem, and removes the `waitForFinish()` test hazard. Natural
  prerequisite: the traversal unification. *Size: ~6 mechanical call sites.* ▸ line 665
- [ ] **Make the debug dumper a consumer of the shared traversal, not a copy of it** — both crashes on
  this branch came out of it; it re-derives index layouts and null-slot rules independently and nothing
  exercises it. Two for two is a defect shape. *Size: large; wants the snapshot + unification first.*
  ▸ line 669
- [ ] **Pick queries can't express "BVH not ready"** — plus the hide-until-ready proposal and its 4 open
  questions ▸ line 536
- [ ] **Back-end scene-picking service** (four consumers) ▸ line 567
- [ ] **Grid as a real bounded pick object** — the #562/#527 root cause ▸ line 572
- [ ] **`gridPlane` should be `std::optional<QPlane3D>`** — its default is the **YZ plane through the
  world origin**, a vertical wall, and `NoteLiDARItem.qml:69` has its binding commented out, so that
  viewer runs the ladder with the default. Inert today only because the GLTF registers and
  `isPickableEmpty()` goes false — but before it loads, the scene *is* pickable-empty. This commit
  makes it strictly better (the grid rung used to fire on every miss). Optional makes the silence
  explicit instead of accidental. Requires deciding what the note viewer's reference plane *should* be
  — a product question. *Size: small code, open question.* ▸ line 526
- [ ] **Move the grid-plane fallback out of QML** ▸ line 526
- [ ] **Split the monolithic line-plot BVH object** ▸ line 577

---

Refactors this change makes worthwhile:

- **~~Cheap `isPickableEmpty()` on `cwGeometryItersecter`~~ — DONE (Phase 2b review).** Shipped as
  `isPickableEmpty()`: the visibility-filtered twin of `boundingBox()`, now what `unProject` gates the
  grid on. Fixes the hidden-geometry seam (all objects hidden → nothing pickable → the grid returns as
  the only way to un-stick the pivot; *some* hidden → grid stays gated off). Two notes on what it
  deliberately does **not** do: (1) it is still O(n) over `Nodes`, not the O(1) BVH-root check — the
  root box is the wrong source (see below); (2) it reads the **authoring** list, not `m_bvh`, on
  purpose. Gating on the built BVH would report "empty" during an async build and hand the pivot to
  the grid — reintroducing the exact #562 teleport. Mid-build keep-pivot is correct behaviour; the
  problem there is that it's *unexplained*, which is the "BVH not ready" item below.
- **Unify the two BVH traversals behind a per-kind accept policy.** On a miss, `unProject` runs
  `intersectsDetailed` then `nearestGeometryPoint` — but the duplication is far worse than the double
  descent: the whole traversal skeleton is copy-pasted (top-level walk, near/far child ordering,
  `subPtr` null check, `isPickable` hoist, `pickKindOf` filter, `transformRayWithInverse`, sub-BVH
  walk), and the per-primitive index extraction is a third copy in `testPrimitive`. The two functions
  are **the same algorithm with a different per-kind accept predicate** — and for `Lines` they are not
  different at all, just a different radius (20mm vs 4mm). Right shape: one traversal parameterised by
  a policy (`exact` = MT with cull / line tolerance / pickRadius sphere; `anchor` = MT no-cull
  else nearest edge / line tolerance / point tolerance).
  **Correction to the earlier note here:** this is *not* blocked behind the scene-picking service.
  `intersectsDetailed`'s public contract does not change — only `testPrimitive`'s **private**
  signature does. It can land on its own. Wants its own test matrix per kind × policy.
  **Sharpened by the tube-promote deletion (review 3, 2026-07-15) — the strongest item on this list:**
  the near-miss fallback was a *third* variant of the same traversal, an off-ray policy fused into the
  exact path. Deleting it leaves the architecture stated cleanly for the first time: **exact picking
  and off-ray anchoring are two policies, and off-ray anchoring is opt-in via a separate entry point.**
  That is why `cwLeadView::isOccluded` is now correct *by construction* — it calls `intersectsDetailed`
  and therefore cannot receive an off-ray point, while `cwBaseTurnTableInteraction` opts into
  `nearestGeometryPoint` deliberately. The policy split is the good part and must survive the
  unification; only the copy-paste goes. Current spans: `intersectsDetailed` ≈ `:882-1090`,
  `nearestGeometryPoint` ≈ `:1122-1305` — ~340 lines, sharing ~80%. The live cost of *not* doing it:
  the exit-t prune and the scene-global line pad (both below) each have to be fixed twice, in two
  places that have already drifted. Two side-effects it would clean up, both visible only now that the
  tube is gone: `RaySphereHit::dSq` has exactly **one** consumer (`nearestGeometryPoint`, probing with
  radius 0 purely to read a distance — a `raySquaredDistance()` wearing a sphere-hit costume), and
  `kPointAabbPadScale = 1.0f` is a multiply-by-one that exists only to name the leaf-pad/pickRadius
  relationship. *Size: large; hot path, wants before/after profiling and its own commit — bundling it
  with anything else makes a perf regression unbisectable.*
- **Reconcile the two radius systems governing point picking — a NAMING fix, not a behavior fix.**
  `cwPickTolerance` is documented as "screen-space pick tolerance for ray-vs-line picking"
  (`cwPickQuery.h:15`), but `nearestGeometryPoint` uses it for points and triangles too. So a point is
  governed by `Object::pickRadius()` (world-space, data-derived: `meanSpacingXY *
  cwRenderPointCloud::PointPickRadiusScale`) on the exact path, and by `cwPickTolerance` (screen-space,
  camera-derived) on the anchor path — and *which one applies is decided by call order in `unProject`*,
  not by any stated policy.
  **Resolved direction (review 3, 2026-07-15): do NOT make points consult the tolerance.** The obvious
  reading of this mismatch — "`cwScenePick.cpp:29` asks for a 1.5mm screen-space snap and points
  silently ignore it, so wire it up" — is **wrong, and this commit is why**. A screen-space tolerance is
  right for a 1-D line (a ray never exactly hits one). For a point cloud standing in for a *surface*, a
  world-space watertight sphere is not a degraded substitute, it is the correct model: watertightness is
  view-independent, so a ray over the cloud surface hits at any zoom. Making points honor a screen-space
  radius means accepting points the ray missed and reporting their centres — precisely the off-ray snap
  just deleted, and `test_cwLeadView_occlusion.cpp` would fail again. The tube already proved the damage:
  `cwCoordinatePicker::pick` fed the promoted centre straight into `m_geoReference->toGlobal()`, so a
  WGS84 readout named a point the user was never over.
  So the follow-up is **typing and naming only, no behavior change**: document `tolerance` on
  `cwPickTolerance` as Lines-only, and rename `cwScenePick::snappedPoint`'s `pixelRadius`
  (`cwScenePick.h:39-42`) — which reads as a global snap budget — to something like
  `lineToleranceMillimeters`, so the next caller doesn't assume points widen with it. If sparse-cloud
  reach ever genuinely matters, the lever is `PointPickRadiusScale`, which keeps picks on-ray and
  watertight. *Size: small; renames through `cwScenePicker` / `cwCoordinatePicker` /
  `cwMeasurementInteraction`. Pairs naturally with the accept-policy parameter above, which is where the
  per-kind rule gets named.*
- **[HIGH — live per-frame cost] Scope `intersectsDetailed`'s tolerance pad per node.** DONE for
  `nearestGeometryPoint` (this commit); **`intersectsDetailed` still has it** and is the worse of the
  two. It computes `linePad = conservativeTolerancePad(ray, topLevel.at(0).bbox, ...)` — the
  *root* box — and folds it into every line object's sub-BVH pad.
  **Updated after the tube-promote deletion (review 3):** that pad line now reads
  `subPad = isLineObject ? linePad : 0.0f` — the `max(subTubeDist, ...)` competitor is gone, and
  `linePad` is the only pad left in the traversal, so it is now the *sole* remaining consumer of this
  code path and the item is unchanged in substance: every centerline node is still inflated by the
  radius at the scene's
  far corner. Unlike the anchor (once per rotate-press), this runs **per mouse-move**: the measurement
  tool's `HoverHandler.onPointChanged` → `cwMeasurementInteraction::hover` → `snapPick` →
  `cwScenePick::snappedPoint` → `intersectsDetailed`, and `cwCamera::pickQuery` always enables the
  tolerance. Verified chain end-to-end (review 2, conf 85). Perspective only (`slope > 0`); ortho sets
  `constant` and is depth-independent. Rough scale: 1.5mm at 1000px/55°fov → `slope ≈ 0.012`; a 6 km
  root depth → `linePad ≈ 75 m`, i.e. near-total collapse to a linear scan of `raySegmentClosest` over
  every shot, per mouse move, growing with cave size. Fix is the change already written next door —
  hoist `conservativeTolerancePad(rayModel, sbn.bbox, ...)` into the sub-BVH loop at `:1064`, exactly
  as `nearestGeometryPoint:1218` does; the conservative argument carries over unchanged. Needs a
  **line-object** perf test — the existing control is a point cloud and is structurally blind to this.
  Correctness is unaffected either way (the pad errs conservative; the fine `radiusAt(tWorld)` test
  still rejects). **Do not "fix" it with a per-node `radiusAt(tBox)`** — `radiusAt` is increasing, so a
  sound per-node bound must use the node's **far** corner depth, not its entry depth.
  - If taken, `conservativeTolerancePad` moves onto a per-frame path and two cheap wins become
    worthwhile: `QRay3D::projectedDistance` is out-of-line and divides by `lengthSquared()` per call,
    so the 8-corner loop is 8 non-inlinable calls + 8 divides per node visit — the max of an affine
    function over an AABB is at the corner picked per-axis by the direction's sign, so **one**
    evaluation suffices; and under ortho (`slope == 0`) the result is provably box-independent.
    Not worth doing while it stays once-per-press.
- **The exit-t prune pattern still exists in `intersectsDetailed`** (`:921`, `:1021`), which was the
  source `nearestGeometryPoint` copied. Fixed in the anchor path via `boxEntryDistance()`; the exact
  path still calls `QBox3D::intersection(ray)` and prunes on its result, which is the **exit**
  parameter whenever the ray origin is inside the box. Latent there because the 4mm pad makes
  origin-inside far rarer, and because on-ray hits are bounded below by the entry t. Same one-line
  helper fixes it; needs its own regression test and a full pick-suite run because it touches leads,
  `cwScenePick`, and the measurement tool.
- **Hoist the duplicated test helpers.** `makeLineObject` / `makeTriangleObject` / `ortho()` are
  near-verbatim in `test_cwGeometryItersecter_linePick.cpp` and `test_cwGeometryItersecter_nearestPoint.cpp`,
  and they have **already diverged**: the nearestPoint copies dropped the `modelMatrix` parameter, the
  `setCullBackfaces(false)` call, and the `kinds` parameter. That divergence is exactly why the
  coverage gaps below are unreachable today. Hoist into a shared test header.
- **Coverage gaps in the anchor tests** (each ships a green suite while the behaviour breaks):
  the behind-camera guard (`tWorld <= 0` — no test ray ever starts inside geometry); non-identity model
  matrices (`cwRenderTexturedItems` registers with `item.modelMatrix`, and the model-space `dSq` vs
  world-space radius comparison is only exact for rigid transforms); back-face triangle anchoring
  (the design explicitly passes `cull=false`, but the only triangle test hits the front face);
  the `kinds` filter (currently dead — `cwCamera::pickQuery` hardcodes `All` — but advertised as API);
  the empty intersecter (`topLevel.at(0)` would abort if the guard regressed); and zoom-not-anchoring
  (only pan has a test, though the header asserts the anchor "drifts the zoom"). Also: the pan
  no-anchor test's `extent = 105.0f` has no precondition proving the anchor path would fire, so a
  retuned radius makes it vacuous — and its perspective section may already be vacuous.
- **Move the grid-plane fallback out of QML.** It's plumbed `regionSceneManager.gridPlane.plane` →
  `GLTerrainRenderer.qml:86` → interaction (`setGridPlane`), while the geometry it's a fallback *for*
  lives on `cwScene`. One picking concern split across three owners; `cwScene` has no grid knowledge.
- **Phase 2b: consolidate the pivot ladder.** When the anchor lands, make the
  exact→anchor→grid→nullopt chain a single `resolvePivot(ray)` policy object rather than another
  inline `if` chain in the QQuickItem-derived interaction. Open questions (anchor radius, kind
  filtering) are pick-policy decisions that don't belong in the turn-table.

Larger architectural changes:

- **Pick queries can't express "BVH not ready" — and the proposed answer: don't show the object until
  its picking is built.** `intersectsDetailed` returns a miss when `m_bvh` is null; `nearestGeometryPoint`
  returns nullopt. Neither can say "not built yet", so every consumer reads it as "nothing there":
  `unProject` keeps the pivot (looks like broken picking — this is what the LAZ report turned out to
  be), `cwScenePick::snappedPoint` doesn't snap, and `cwLeadView::isOccluded` returns **false = not
  occluded**, so leads genuinely behind a scrap are tappable during the build. That last one fails
  *open*, which is the worst direction.

  **Proposal (vpicaver, review of Phase 2b): don't render the object until its BVH is published.**
  Attractive because it makes the screen and the picker agree by construction — the user can't aim at
  something that isn't pickable yet, so there is no "picking is broken" state to explain, and no
  per-consumer readiness decision to plumb. It also composes with the shipped `isPickableEmpty()`:
  since that filters on visibility, a not-yet-shown object is automatically "not pickable", so during
  the build the scene reads as empty and the grid fallback is correct rather than a teleport.
  Open questions to settle before committing to it:
  - **Which objects?** Point clouds/LiDAR notes take tens of seconds and are the actual problem. The
    line plot builds fast and is the thing users most want to see immediately — hiding the centerline
    while a LAZ note builds would be a regression. Probably per-object, not global.
  - **What does the user see meanwhile?** A cloud that pops in seconds after load needs *some*
    affordance. The progress is already globally observable — `cwGeometryItersecter` registers the
    build with `cwFutureManagerModel` as `"Accelerating picking"` (`:168`, `:445`, wired at
    `cwRootData.cpp:185`, surfaced by `TaskListView.qml:18`) — so no new progress pipeline is needed,
    but a spinner in the task list is easy to miss.
  - **Does it help the render path?** Rendering already works fine without the BVH; this would make
    the render wait on a pick-only structure, which is a new coupling from rendering onto picking.
  - **Fallback still needed?** `cwLeadView::isOccluded` failing open is a bug regardless — if an object
    can ever be visible-but-unbuilt (e.g. the hide is per-object and leads sit near an unbuilt cloud),
    the readiness accessor is still wanted. Hiding may reduce the blast radius rather than remove it.

  Blast radius either way: medium. Alternative if hiding proves too blunt: a `readiness()` accessor /
  tri-state return so each consumer degrades on its own terms (pivot: keep + hint; lead: fail closed).
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
- **Split the monolithic line-plot BVH object.**
  `cwRenderLinePlot.cpp:57` registers the whole centerline as a single object (`Key{this,0}`), so its
  top-level box ≈ the whole scene box. The nearest-point anchor no longer depends on node granularity,
  but object-level picking still suffers: any "nearest object first" policy is meaningless while one
  object's box spans the scene. Registering the line plot as per-cave /
  per-run sub-objects (or partitioning the top level spatially) makes object-level picking meaningful:
  the pivot fallback could prefer the nearest object, and `cwScenePick` snapping / lead occlusion get
  tighter object boxes for free. Blast radius: `cwRenderLinePlot` add-object granularity + BVH build +
  anything keyed on `Key{this,0}`.

---

## Review 2 findings (pre-commit review of the working tree, 2026-07-15)

Second full review, covering everything written *after* the Phase 1 review: `boxEntryDistance()`,
`isPickableEmpty()`, the per-node pad fix, and the `[performance]` test. Six read-only lenses
(correctness, performance, thread-safety, ownership, test-quality, architecture).

**Clean:** thread safety and ownership, both at high confidence and by independent reasoning. The
intersecter is main-thread-only; `invalidatePublishedSlot` is copy-on-write; all six slot derefs are
guarded; `isPickableEmpty` is `const` so `Nodes` binds `const begin()/end()` and never detaches; the
build worker snapshots by value (`auto nodesSnapshot = Nodes;`). The `const&` (not copy) `shared_ptr`
binding is correct, not a UAF. No `detach()` anywhere in the new traversal (`topLevel[...]` /
`sub.bvhNodes[...]` bind through const refs → const `operator[]`).

**Fixed in this pass (doc/test truth only, no behavior change):**
- `unProject`'s header contract claimed "The result lies on the cursor ray, so it is safe as a pan
  anchor / zoom target." **False**, and load-bearing — it was the stated reason pan/zoom may pass
  `anchorToNearestGeometry=false`. Only *triangle* hits are on-ray: `fillLineHit` returns the closest
  point **on the segment** (`:1588`), and `fillPointHit` returns the vertex **center** (`:1560`,
  deliberately — "coordinate readouts snap to the data point rather than to the sphere surface"). The
  real invariant is narrower: rung 1 is off-ray by up to ~4mm of screen, rung 2 by up to 20mm. Rung 2
  is opt-in because it's *5× worse*, not because rung 1 is clean.
- `startRotating`'s comment claimed an off-ray pivot never becomes a zoom target. It does:
  `setCenter(*picked)` → `m_center` → `bindPerspectiveIntersection`'s `value_or(m_center)` (`:213`).
- `conservativeTolerancePad`'s param was still named `rootBox` with a comment about "the scene's
  farthest depth" — stale the moment it went per-node.
- `isPickableEmpty`'s doc said "non-finite bounds". `QBox3D::isFinite()` is `boxtype == Finite`, a
  *type flag* — a box with NaN corners still reports Finite. (Also: `unite()` poisons the whole union
  if *any* box is Infinite, which would invert the quantifier and report empty despite geometry —
  unreachable today, nothing constructs an Infinite box.)
- **The `[performance]` test passed vacuously under debug logging.** `nodesVisited`/`dumpLeafPrimitive`
  instrument `intersectsDetailed` only — `nearestGeometryPoint` has no debug branch — so an inherited
  `cw.picking.debug=true` inflates the *denominator* alone. Measured: ratio 0.2366× with debug on vs
  1.3242× off, i.e. headroom silently 7.5× → 42×. Now pins the category off with `qScopeGuard`.
  Verified: `QT_LOGGING_RULES="cw.picking.debug=true"` now gives 1.315×, was 0.237×.
- **`linePick` clobbered global logging state.** It used `setFilterRules(...)` then
  `setFilterRules(QString())`, which wipes *all* rules rather than restoring them, with no guard
  against an early `REQUIRE` exit. `test_cwGeometryItersecter_phakeCave.cpp:57` documents the correct
  pattern verbatim and warns against exactly this. It composes with the above: `linePick` links before
  `nearestPoint` alphabetically, so a leak lands on the perf test. Now `setEnabled` + `qScopeGuard`.
- The pivot-kept test asserted against a default-constructed `m_center` = **(0,0,0)**, so "kept" and
  "reset to the origin" were the same assertion. Now `setCenter(5,6,7)` first, like its sibling.
- The perf test hardcoded `20.0`; `PivotAnchorRadiusMillimeters` is public precisely so a retune fails
  loudly. (`4.0` genuinely is unavailable — `kPivotPickRadiusMillimeters` is anon-namespace.)

**Reported, NOT fixed (correctness / out of scope):**
- **[HIGH] `intersectsDetailed`'s scene-global line pad, on a per-frame path.** See the follow-up
  above. The single most valuable thing left on this branch.
- **The pan lurch is real and pre-existing.** Grab within ~4mm of a centerline → `m_panAnchor` = the
  off-ray segment point → `PanPlane` through it → `translateLastPosition()` computes
  `translateAmount = intersection - m_panAnchor` ≠ 0 → the view steps ~15px on mouse-down. A triangle
  hit gives exactly 0, so zero is clearly the intent. Byte-identical on `@{upstream}`, and
  `startPanning` is untouched here — **not introduced or worsened by this diff**. Fix would be to
  project the rung-1 hit onto the ray for on-ray callers; that's a behavior change, own commit.
  *Points are affected too* — an earlier note assumed only lines were, because points ignore the
  tolerance; but they return the sphere center, which is equally off-ray.
- **`startPanning on a near-miss does not jump the view` cannot catch its own invariant.** Its click
  misses, so `unProject` returns `nullopt` and `rayPointAtCenterDepth` is on-ray *by construction* —
  `translateAmount ≈ 0` trivially. It still earns its keep (it guards against someone flipping
  `startPanning` to `anchorToNearestGeometry=true`), but the one path that can violate the invariant —
  a rung-1 hit on a point or line — is never exercised. A test that did would fail today.
- `isPickableEmpty()` computes a full box union to answer a boolean; could return `false` at the first
  pickable finite box. Cold path — cleanliness, not latency.
- `kPixelTolerance` (documented as a *pixel* tolerance for `project()` comparisons) is used as a
  **world-space** margin at `test_cwBaseTurnTableInteraction.cpp:872,995`. Works, but a retune of a
  pixel constant would silently loosen world assertions. Wants its own `kWorldTolerance`.
- Lint: `cwGeometryItersecter.cpp:366,391` unprotected `std::max` (Windows min/max macro safety). Left
  deliberately — ~12 other unprotected uses in the same file; fixing 2 of 14 is worse than fixing 0.

**New architectural items (detail in the sections above):**
- `m_center` carries three semantics at once (orbit pivot / pan depth reference / zoom fallback), which
  is *why* the bool exists and why the gate can't fully isolate rotation's off-ray currency from zoom.
- The bool is named for the mechanism, not the requirement. The real discriminator is *"must the result
  lie on the cursor ray?"* — `RayConstraint::{OnRay, AnyPoint}`. Named that way the ladder falls out of
  the constraint instead of being an if-chain that happens to be ordered correctly. Not a context
  struct — CLAUDE.md's context-struct rule targets virtual/interface methods; `unProject` is private
  and non-virtual, so a struct would be ceremony.
- The queries could be static-on-`QuerySnapshot` like `pointsInBox` already is — they read nothing but
  `BvhData`. That drops the main-thread constraint, lets a picking service hold a *value* instead of a
  back-reference to a mutable subsystem, and makes the traversal unit-testable without a QObject, an
  async build, and an event loop (which is why these tests need `waitForFinish()` today).
- **Both crashes on this branch came out of the debug dumper**, which is a hand-maintained parallel
  walk of the BVH that re-derives index layouts and null-slot rules independently, and which nothing
  exercises. Two for two is the shape of the defect, not a coincidence. Make it a *consumer* of the
  shared traversal rather than a copy of it.
- `gridPlane` is optional, unvalidated, view-supplied config whose default (`QPlane3D()`) is the **YZ
  plane through the world origin** — a vertical wall. `NoteLiDARItem.qml:69` has its binding commented
  out, so that viewer runs the whole ladder with that default; it's inert today only because the GLTF
  registers and `isPickableEmpty()` goes false. Before the GLTF loads, the scene *is* pickable-empty.
  This commit makes it strictly better (the grid rung used to fire on *every* miss), but
  `std::optional<QPlane3D>` would make the second viewer's silence explicit instead of accidental.

## Review 3 findings (pre-commit review of the tube-promote deletion, 2026-07-15)

The commit: deleted the near-miss / "tube pick" fallback (`tryPromoteNearMiss`, `NearMissResult`,
`kTubeFactor`, the test-only `setTubePickEnabled`/`isTubePickEnabled`, the per-pick traversal box pad,
`RaySphereHit::tCenter`, `BvhData::maxPickRadius`) and added `testcases/test_cwLeadView_occlusion.cpp`.
Net −303 lines. The two items marked **next up** in the triage index came out of this review.

**Why the pad went, against the earlier recommendation to keep it.** The prior session's note here said
to keep the pad as budget for future WYSIWYG work. That was wrong, and the reason is worth recording:
`primitiveModelBox` already bakes a full `pickRadius` pad into every point's *leaf* box at build time,
so an unpadded traversal still reaches every leaf whose sphere the ray can exactly hit. The pad's only
job was reaching leaves the ray **misses** — i.e. the tube, and nothing else. Verified adversarially:
all five `bbox` write sites traced (no path builds a node box from raw vertices); production clouds
register with an identity matrix; and a bit-exact replica of `primitiveModelBox` +
`QBox3D::intersection` + `raySphereIntersectDouble` lost **zero** hits across 198M randomized grazing
rays plus exhaustive float-grid sweeps. The one failing regime (`r < ULP/2`, where the padded box
collapses to zero extent) needs a cloud ~168 km from its project origin — `cwLazLoader` rebases every
cloud to the region origin in double, and the old pad vanished there too.

**Two bugs fixed, not one.** The known one is `cwLeadView::isOccluded` — it picks `cwPickQuery::Solid`
with no tolerance, and neither triangles nor points consult the tolerance, so the tube was the only
near-miss path reachable there and had nothing to lose to; an off-ray cloud point was promoted, reported
its *centre* depth in front of the lead's billboard, and made a plainly-visible lead unclickable. The
second was missed until review: `startPanning` (`cwBaseTurnTableInteraction.cpp:316-318`) calls
`setCenter(*clickWorld)` on the unlocked path, so the tube was feeding **pan** a pivot up to 2.5
spacings off-ray — exactly what `cwBaseTurnTableInteraction.h`'s doc says must never happen ("would
visibly jerk a pan or drift a zoom — the other reason only rotation opts in"). Pan does *not* regress
from the deletion: `PanPlane` is fully determined by the anchor's depth, so the tube's lateral error was
discarded anyway.

**The one accepted regression: `cwScenePick::snappedPoint`.** Its cloud reach drops 2.5× → 1.0×
spacings, and it is the only consumer with no near-miss fallback of its own. Bites where local spacing
exceeds ~1.41× the mean (`PointPickRadiusScale = 1.0` vs the 0.7071 threshold) — e.g. a wall met at 60°
grazing. Accepted deliberately: the tube *violated* this consumer's own stated contract ("triangle and
point hits keep their exact surface point"), handing `cwCoordinatePicker` → `toGlobal()` → WGS84 a
coordinate for a point the user was never over. Returning nothing beats lying. See the radius-systems
item above for why the fix is **not** a fallback here.

**Deferred out of this commit (candidates, not decisions):**
- **`Object`'s `float pickRadius = 0.0f` is the real hole in the watertight invariant.** The
  `static_assert` in `cwRenderPointCloud.h` guards the *constant* (`PointPickRadiusScale >= 0.7071068f`),
  not the value that reaches the BVH (`pickRadius >= meanSpacing * sqrt(2)/2`). Any future `Type::Points`
  producer — a decimated LOD, a second loader, a mesh-to-cloud path — can register with any radius,
  including the default `0.0f`, which makes the cloud silently unpickable. It holds today only by
  coincidence of there being exactly one producer, which matters more now that the header states this
  constant is "the only thing keeping a cloud pickable where it is drawn". Fix: `Object` takes the
  spacing (or a small policy struct) and derives the radius itself, so the rule is applied where it is
  depended on and a zero-radius cloud becomes unrepresentable. Folds in the "`pickRadius` is a per-Object
  field doing a per-Kind job" note — that becomes true for free, and isn't worth a commit alone.
  *Size: constructor signature + ~8 test files.*
- **`cwRayHit` has never promised `pointWorld()` lies on the query ray** — the root-cause *class* here.
  `fillPointHit` reports the vertex centre; the deleted promote reported a centre up to 2.5 radii
  off-ray; `nearestGeometryPoint` returns an off-ray point by design. Three producers, three off-ray
  budgets, one return type, no stated contract — which is why `isOccluded` consumed an off-ray point as
  a depth with no type-level reason to suspect it. This commit establishes the rule empirically; it
  survives only as prose in two files that a change to a third won't read. *Size: small in code, large
  in leverage. Pairs with the traversal unification — the contract is what distinguishes the policies.*
- **WYSIWYG picking — the pad was never the blocker; correct an earlier note here.** The blocker is
  `fillPointHit` setting `pointWorld = centerWorld`. At `pickRadius ≈ one spacing` that's centimetres
  off-ray and harmless; at a splat-sized radius a rim hit would hand the measurement tool a coordinate
  far off the ray the user pointed — the *same* defect class as the tube, reached by growing the sphere
  instead of by a fallback. And the pad mechanism isn't gone from the file: `conservativeTolerancePad` +
  the `linePad` path is a live, working template of exactly that shape. So the ordering is **(1) decide
  what a point hit reports, (2) then the radius can grow** — not the reverse. Note the draw/pick gap is
  parameter-dependent, not one number: the drawn half-extent is `worldRadius/2` (`gl_PointSize` is a
  square's *side*, and `PointCloud.frag` fills it with no disc discard), so it is ~1.6× at a tuned
  `worldRadius = 0.565` on a 0.172 m cloud, but ~13× at the `1.29` default on a 5 cm scan.
- **`cwPickQuery::Kind` conflates three taxonomies, and it is already load-bearing** — not future drift.
  `Kind` is mechanically the geometry primitive type (`pickKindOf` is a straight `cwGeometry::Type`
  switch), yet it is used as provenance (`kSurveyGeometryKinds = Triangles|Lines`, "everything a user
  authors or surveys") and as occlusion semantics (`Solid = Triangles|Points`). LiDAR notes **already**
  ride `Kind::Triangles` into `kSurveyGeometryKinds`, so "scanned geometry is orbit-worthy" is a
  coincidence of rendering choice, not a decision — a meshed point cloud would join silently and no test
  would notice. Symmetrically, sketch strokes rendering as `Lines` would become pivot-eligible and stay
  non-occluding. Fix: `Object` carries a pick *role*; queries filter on role. Composes with the
  `pickRadius` item above (both want a richer `Object` construction API). *Size: medium.*
- **`cwLeadView`'s ray/plane solve has zero coverage repo-wide** (conf 85). It carries the file's longest
  comment — `quadDistance` solves the ray/billboard-plane crossing rather than reusing the marker
  centre's depth, because "comparing the cave hit to the centre's depth would mis-gate off-centre taps
  under perspective". Every test that touches it (the three new C++ cases, `tst_Leads.qml:571`,
  `tst_ScrapInteraction.qml:309`) taps the **centre** under **ortho**, where the rays are parallel to
  the plane normal and the crossing depth equals the centre depth for every pixel — the solve is a no-op
  by construction, so replacing it with the naive centre-depth compare passes everything in the repo.
  Needs an off-centre tap under perspective. Deliberately not added to this commit: it tests
  pre-existing production code the deletion never touched, and a version that doesn't distinguish the
  two implementations is worthless. *Size: small, but the geometry is fiddly — note `rayFromQtViewport`
  starts the ray on the near plane, not at the eye.*
- **`cwRenderPointCloud::setWorldRadius` doesn't clamp** while `cwLazLayersSceneNode.cpp:386` clamps to
  `[0.01, 50]`, and the offline `sink_repatcher --point-radius` path (`cwRHIPointCloud` reading
  `appearance->worldRadius`) bypasses both. Inert today — the only caller of the unclamped setter passes
  an already-clamped value. *Size: 3 lines; an issue, not a refactor.*

**Test-integrity note settled in this commit.** Both "miss" tests in the point suites were being decided
by the **broad-phase AABB**, never reaching the sphere test — `addPoints` pads the object box by a full
`pickRadius`, so an x-only offset past 1× is box-rejected. Fixed with diagonal rays that sit inside the
padded box but outside the sphere, and confirmed by mutation: widening the accept region to the box's
corner distance (`sqrt(3)*r`) fails **only** on the new diagonal cases while the axis-aligned ones pass.
Worth recording that the reviewer's proposed mutation for this — deleting `testPrimitive`'s
`if (!sphere.hit)` guard — does **not** work: on a miss `raySphereIntersectDouble` returns `tNear = 0.0`
and the next guard (`if (sphere.tNear <= 0.0)`) catches it, so the sphere rejection is double-guarded.
Ran the mutation before believing it.
