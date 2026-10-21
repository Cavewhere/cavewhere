# Passage tube rendering — finished follow-up tasks

Verbatim specs of the T07-letter tasks that have landed on `tube_render`,
moved out of `TUBE_RENDERING_IMPLEMENTATION_PLAN.md` so the workflow agents
stop reading them. They are history: the code and the commit bodies are the
truth, and `TUBE_RENDERING_RESEARCH.md` §3.6 has one row per artifact. Open
follow-ups live in `TUBE_RENDERING_FOLLOWUPS.md`.

Order landed: T07b, T07c, T07e, T07f, T07g, T07d0, T07d, T07h, T07i, T07j, T07k, T07l, T07m, T07n, T07p, T07q, T07r, T07v, T07w, T07y, T07z, T07aa, T07ac.

---

### T07b — LRUD ring shape and shaft readings

**Goal.** Fix the two LRUD defects seen on Phake Cave 3000 between a11 and
a12 (a figure-8 tube up to 35 m across): the ring ellipse is laid out with
the wrong radius, and pit depths recorded as a station's up or down are
used as the passage ceiling or floor.

**Starting state.** The working tree already holds uncommitted work that is
part of this task; build on it, do not revert it:
- `cwPassageTubeBuilder.cpp`: `stationLegDirections`, `isAlongLeg`,
  `ringLrud` (drops an up/down reading that runs along a leg leaving the
  station), `continueAlongGentleLeg` (refills a dropped reading from the
  other end of a gentle leg), `borrowedRadii` (a steep ring borrows a round
  ring at the fitted ring's horizontal half-width), and the
  `startRingLrud`/`endRingLrud` members of `WalkedLeg`.
- `test_cwPassageTubeBuilder.cpp`: three passing tests for the above
  ("keeps the pit depth out of the lip ring and rounds the shaft", "drops up
  and down that run along a steep leg", "keeps a side pit's depth out of the
  through passage ring") and one FAILING test, "keeps the Phake Cave a11 to
  a12 leg a single sane tube" (lines ~993–1140, with the survey table
  `kPhakeShots` and `phakeCavePositions()`). That test is the acceptance
  test for this task: make it pass without weakening its assertions.

**Read.** Research §3.2.1 (LRUD ellipse), §3.2.4 (fallback), §3.2.5 (ring
fit), §3.6 rows "Pinched ring from `0 0 0 0` LRUD" and "Source change";
§5 decision 1. Code: `fitLrudRing`, `placeRingVertices`, `buildRings`,
`walkedLeg`, `chainSamples`, `capOffset` in `cwPassageTubeBuilder.cpp`;
the ring dump of the current output at
`/Users/cave/.claude/jobs/ab1fa6ed/tmp/tube-phake-dump.txt`.

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeMesh.h` (warning
type), `test_cwPassageTubeBuilder.cpp`.

**Spec.**
- **Ellipse polar radius.** `fitLrudRing` currently uses
  `hypot(a·cosθ, b·sinθ)`, the distance to the ellipse point at *parameter*
  θ, while `placeRingVertices` lays it out along *direction* θ. Use the
  polar radius of the quadrant ellipse at angle θ:
  `ρ(θ) = a·b / hypot(b·cosθ, a·sinθ)`, with `a = cosθ ≥ 0 ? r : l` and
  `b = sinθ ≥ 0 ? u : d` as before. Axis bins are unchanged (θ = 0 gives a,
  θ = 90° gives b). Update the one existing expectation in "fits a
  horizontal LRUD ring to the quadrant ellipse": for r = 2, u = 3 the 45°
  bin is `6 / hypot(3·cos45°, 2·sin45°) ≈ 2.3534`.
- **Shaft readings.** Surveyors record a pit's depth as the down (or an
  aven's height as the up) of the stations next to it, including a dead
  end with no leg into the pit (a12: L 0, R 2, U 8, **D 35**; a4: D 23;
  a10: U 24). Such a reading is a shaft, not a wall distance. Decide per
  station occurrence after the zero/unit rules and before the along-leg
  rule:
  - `width = l + r` over the present values (absent when both are absent).
  - `neighbor` = median of the same reading (up for up, down for down) at
    the far end of every leg attached to the station (all legs, all chains,
    from the full `legs` list, keyed by cave id and station name like
    `stationLegDirections`), over the values that are present; absent when
    none are. Use the raw neighbor readings here (before their own shaft
    rule) so the rule has no order dependence.
  - `reference = max(width, neighbor)` over the ones present. When neither
    is present, keep the reading.
  - The reading is a shaft when `reading > kShaftRatio · reference`,
    `kShaftRatio = 3.0`.
  - A shaft reading is replaced, for the ring only, by `neighbor` when
    present, else by `width / 2`; the raw value still feeds `capOffset`.
    Emit one `Warning{ShaftReading, tripId, station, message}` per replaced
    reading, e.g. "Station a12's down of 35 m reads as a pit; the tube
    floor uses 0.5 m." Add `ShaftReading` to `cwPassageTubeMesh::Warning::Type`.
  - Check against the Phake Cave data (see the test's table): a12 D 35
    (width 2, neighbor 0.5), a4 D 23 (chunk 1 occurrence width 5, neighbors
    a3 1 / a5 2 / a6 23 → median 2), a6 D 23, a10 U 24 (width 4.3,
    neighbors a9 24 / a11 3 / a13 1.8 → median 3) are shafts; a3 U 4.5
    (width 4.5) and a5 U 11 (width 5, neighbor 2 → 11 ≤ 15) are kept.
  - Order in `walkedLeg`: shaft replacement → `ringLrud` along-leg drop →
    `continueAlongGentleLeg`. The fill then carries the replaced (sane)
    value, so a6 no longer inherits a4's 23 m and a9 no longer inherits
    a10's 24 m.
- Keep the along-leg rule and `borrowedRadii` as they are.

**Done when.** The Phake Cave test passes unchanged. New tests: a dead end
with L 0 / R 2 / U 8 / D 35 after a station with D 0.5 gets a 0.5 m floor
on every ring of the leg and one `ShaftReading` warning naming the
station; a three-station canyon 1 m wide and 10 m tall emits no warning and
keeps its 10 m ceiling; a station whose neighbors on both sides carry the
same shaft depth (a4/a6 pattern, with a vertical leg leaving the second
station) has no ring radius above the passage's width on either leg; the
45° bin of r = 2, u = 3 is ≈ 2.3534; the ring for L = R = U = D is still a
circle. All `[PassageTube]` tests pass; the full C++ suite passes.

**Commit subject.** `Fix the LRUD ring shape and keep shaft depths out of passage rings`

---

### T07c — LRUD facing and junction arm ends

**Goal.** Fix the unjoined junctions seen on Phake Cave 3000 at a6 (arm
a4→a6 meets a7–a6–a9) and a10 (arm a13→a10 meets a9–a10–a11). The ring
dump at `/Users/cave/.claude/jobs/ab1fa6ed/tmp/tube-phake-junctions.txt`
(built after T07b) shows two causes:

1. **Every leg lays a station's full left and right across itself.** A
   station occurrence's LRUD was measured facing one way (along its chunk),
   but each leg that touches the station draws the whole reading
   perpendicular to its own direction, swapping left and right on legs
   walked backwards. At a6, chunk 1's record `L 5 R 0 U 3 D 23` becomes a
   5 m wall to the NW on the through chain (leg a7→a6) and a 5 m wall to
   the NE on the arm (leg a4→a6), 75° apart, so the arm is a 5 m × 5 m
   vertical fin standing outside the through tube's 0.2 m SE wall.
2. **The dome is flat and the arm is pushed along its own axis.** The dome
   height is the ring's smallest radius, which is `kMinRadiusMeters` (0.05)
   whenever any reading is 0, so arms end in a flat cap. The push
   (`kArmInsetFactor × mean radius`) moves the a4→a6 arm 0.95 m past a6
   into a region where the through tube has no volume. At a10 the arm has
   no LRUD at its a10 occurrence (chunk 3), borrows a13's ring (1.8 m
   right, 1.7 m down) and is pushed 0.5 m past a10, so a 1.7 m-deep box
   hangs below the through tube's floor (a10's D is 0).

**Read.** Research §1.1 (LRUD follow the chunk's station order), §3.1,
§3.2.1 (ellipse; its "Facing" paragraph), §3.3.3 (junction arms: the dome
rule, which replaced the push), §3.6 row "Junction stubs visible from
inside". Code: `extractLegs`, `stationLrud`, `swappedLeftRight`,
`walkedLeg`, `chainSamples`, `lerpLrud`, `fitLrudRing`, `buildRings`,
`classifyChainEnds`, `insetJunctionArms`, `addDome`, `addChainMesh` in
`cwPassageTubeBuilder.cpp`; the tests "swaps left and right on a leg
walked backwards", "pushes a junction arm into the through tube and domes
it", and the Phake Cave table `kPhakeShots` / `phakeCaveSurvey()` in
`test_cwPassageTubeBuilder.cpp`.

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.**

- **One facing per station occurrence.** A station occurrence's LRUD faces
  the way aven draws a `*data passage` block: along the chord between its
  chunk neighbors. For station `i` of a chunk, `facing = horizontal(P[next]
  − P[prev])` normalized, where `prev`/`next` are the nearest positioned
  stations before and after `i` in the chunk (`prev = i` at the chunk's
  first positioned station, `next = i` at its last). When that horizontal
  component is shorter than `kMinHorizontalLength` (a vertical shot: a6 at
  the start of chunk 2, whose only shot drops 23 m), the occurrence takes
  the facing of the nearest station in the chunk that has one; a chunk
  with none faces north. The record's right direction is `R = (facing.y,
  −facing.x, 0)` (facing north puts right to the east) and left is `−R`.
  Compute facings in `extractLegs` and store `fromFacing`/`toFacing`
  (unit horizontal `QVector3D`) on `Leg`. Reversing a leg leaves readings
  and facings alone: delete `swappedLeftRight`. `WalkedLeg`, `ChainSample`
  and `lerpLrud` carry the facing with each record (a small struct such as
  `LrudRecord {cwStationLrud lrud; QVector3D facing;}`); mid-leg samples
  lerp the readings as today and the facing as `normalize(lerp(fA, fB))`
  (the ends share a chunk; fall back to `fA` when the lerp degenerates).
- **A ring takes each wall's component across itself.** `frame.right` is
  horizontal for every frame with LRUD weight. In `fitLrudRing`, for each
  record compute `c = R · frame.right`; when `c ≥ 0` the record's right
  axis reading is `r·c` and its left `l·c`; when `c < 0` they swap:
  right axis `l·|c|`, left axis `r·|c|`. Up and down are unchanged. Then
  run the existing quadrant-ellipse fit on those four values (same bins,
  same per-bin max across records, same `hasLrudWeight` gate). A record
  whose facing runs along the ring's tangent (`|c| < kMinProjectedLength`)
  contributes only up and down. A leg walked forward or backward now gives
  the same world-space ring, which is what the swap used to achieve.
- **Junction arm ends.** Remove `insetJunctionArms` and `kArmInsetFactor`:
  the arm's last ring stays at the junction station. The dome reaches the
  through passage's far wall: `height = max(kMinRadiusMeters, max over the
  junction station's wall offsets of offset · outward)`, where `outward`
  is the arm's tangent leaving the junction and the wall offsets are
  `−l·R`, `r·R`, `u·ẑ`, `−d·ẑ` (world, per record's facing) of **every**
  LRUD occurrence of that station on every leg in `legs` (passage LRUD,
  i.e. after the shaft rule; keyed by cave id and canonical station name
  like `stationLegDirections`). When the station has no readings at all,
  `height` is the arm's last ring mean radius. Store the heights on
  `Chain` (e.g. `startDomeHeightMeters`/`endDomeHeightMeters`, set where
  `classifyChainEnds` or `buildRings` has the data) and use them in
  `addDome` in place of `min ρ`. Dome ring count and quarter-ellipse
  shrink are unchanged. A ring whose own occurrence has no LRUD still
  borrows as today.
- Keep the shaft rule, the along-leg rule, `continueAlongGentleLeg`,
  `borrowedRadii` and `capOffset` as they are.

**Done when.** New or rewritten tests, all `[PassageTube]`:
- "takes the component of a wall across the ring": chunk 1 a→b→c turning
  90° at b (a = (0,0,0), b = (10,0,0), c = (10,10,0)) with `L 5 R 0 U 1
  D 1` at b and `L 1 R 1 U 1 D 1` at a and c; chunk 2 b→d with d =
  (10,−10,0) and no LRUD. The through chain is c–b–d, the arm is a→b. b
  faces the chord a→c, (1,1,0)/√2, so its 5 m left wall lies toward
  (−1,1,0)/√2 and each ring takes its component: the through ring at b
  has its farthest vertex within 0.2 m of `b + (−3.54, 0, 0)` (today
  5 m), the arm's last ring within 0.2 m of `b + (0, 3.54, 0)` (today
  5 m).
- "keeps a ring's world shape whichever way the chunk is entered"
  replaces "swaps left and right on a leg walked backwards": a→b→c with
  `L 5 R 1 U 1 D 1` at b, and c→b→a with `L 1 R 5 U 1 D 1` at b (the same
  passage, as stored LRUD follow chunk order), give the same set of world
  vertices on the ring at b (within 1e-3).
- "falls back to the next station's facing on a vertical shot": chunk 1
  a→b→c along +x (a = (0,0,0), b = (10,0,0), c = (20,0,0), `L 1 R 1 U 1
  D 1` at a and c, none at b); chunk 2 b→q→r with q = (10,0,−10) and r =
  (0,0,−10), `L 3 R 0 U 1 D 1` at b. b's chord in chunk 2 is vertical, so
  it takes q's facing (chord b→r, west), putting b's 3 m left wall to the
  south: the ring at b on chain a–b–c has its farthest vertex within 0.2 m
  of `b + (0, −3, 0)`.
- "domes a junction arm to the through passage's far wall" replaces
  "pushes a junction arm into the through tube and domes it": with the
  through chunk a→b→c along +x and `L = R = U = D = 2` at b, the arm d→b
  from +y ends with its last ring centered at b (no push), the dome apex
  at `b − (0, 2, 0)` (b's 2 m right wall, facing +x), the mesh closed with
  positive volume.
- "takes the dome height from a junction without readings": the same T
  with no LRUD anywhere → apex at `b − (0, defaultRadius, 0)`.
- "joins the Phake Cave arms to their through passages", from
  `phakeCaveSurvey()`: (1) no ring center of the arm a4→a6 lies past a6
  along the arm, and none of the arm a13→a10 past a10; (2) the a13→a10
  dome apex lies between 1.5 m and 2.5 m past a10 along the arm (a10's
  2 m right wall, chunk 2 facing ≈ SSW, projected on the arm tangent);
  (3) the mesh is closed.
- The Phake a11–a12 test and every other `[PassageTube]` test pass; the
  full C++ suite passes.

**Commit subject.** `Face LRUD by chunk order and dome junction arms to the far wall`

---

### T07e — Mesh quality checks

**Goal.** A library of algorithmic artifact detectors over a
`cwPassageTubeMesh` (and the builder's `Debug` rings), so every builder
test and the parametric suite (T07f) and gallery (T07g) assert "the mesh
looks right" with code instead of eyes.

**Read.** Research §3.6 (each row names the artifact a check catches),
§4.4. Code: the test helpers `isClosedSurface`, `signedVolume`,
`checkNoFoldBack`, `checkFramesContinuous` in
`test_cwPassageTubeBuilder.cpp` (they move into the new class);
`cwGeometryItersecter.h` for the existing BVH (reference only; the check
uses its own grid).

**Create.** `cavewherelib/src/cwPassageTubeMeshCheck.h/.cpp`,
`testcases/test_cwPassageTubeMeshCheck.cpp`. **Modify.**
`test_cwPassageTubeBuilder.cpp` (use the class; delete the moved helpers).

**Spec.** `class cwPassageTubeMeshCheck` (static functions, no state),
every function pure and safe to call from a worker:

- `struct Report { QVector<Issue> issues; int componentCount; QVector<ComponentStats> components; }`,
  `struct Issue { Type type; QVector3D position; QString detail; int triangle = -1; int otherTriangle = -1; }`,
  `enum Type { OpenEdge, InconsistentWinding, NonManifoldVertex, NegativeVolume, DegenerateTriangle, SliverTriangle, SelfIntersection, NestedShell, Crease, RadiusJump, FrameTwist, FoldBack, WallMissed }`.
- **Watertight.** `closed(mesh, range)`: every directed edge of the range
  has exactly one partner reversed → otherwise `OpenEdge` (one issue per
  open edge, at the edge midpoint) or `InconsistentWinding`. Per
  connected component (union–find over shared vertices): `V − E + F =
  2 − 2g`; report `g` in `ComponentStats` (a tree-shaped cave is `g = 0`;
  each independent survey loop adds one). `NonManifoldVertex`: a vertex
  whose incident triangles form more than one edge-connected fan (the
  bow-tie of a twisted segment).
- **Orientation.** `signedVolume(mesh, range) > 0` per component, else
  `NegativeVolume`.
- **Degenerate geometry.** Triangle area `< kMinTriangleArea = 1e-6` m²
  → `DegenerateTriangle`; longest edge / shortest altitude `>
  kMaxAspectRatio = 50` → `SliverTriangle`.
- **Self-intersection.** Triangle–triangle intersection (Guigue–Devillers
  or Möller's interval test, implemented locally, ~80 lines) over all
  candidate pairs from a uniform grid hash with cell size = median edge
  length (`QHash<cell, QVector<int>>`; a triangle is inserted in every
  cell its AABB touches). Skip pairs sharing a vertex index; pairs that
  share a vertex *position* within `kWeldTolerance = 1e-5` m but not an
  index are also skipped (mouth vertices coincide with trimmed strip
  vertices by design). Report `SelfIntersection` at the midpoint of the
  intersection segment with both triangle indices. This is the detector
  for interior faces (an arm penetrating a through), fold-backs, bow-ties
  and zip errors; O(n) expected for a tube mesh.
- **Nested shells.** For every pair of components, test one vertex of the
  smaller against the larger by ray parity (count triangle crossings of a
  ray along `+x` with a tiny irrational tilt); inside → `NestedShell`
  (an arm whose mesh is entirely inside another tube, or a junction that
  did not merge).
- **Visual quality.** `Crease`: adjacent triangles (shared edge) with a
  dihedral angle above `kMaxCreaseDegrees = 150` whose edge is not a
  mouth, cap or window edge (the builder passes a set of "intended crease"
  vertex indices — mouth loops and cap centers — through `Debug`;
  `Report` lists the rest). On `Debug` rings: `RadiusJump` when
  consecutive rings on a chain differ by a factor over
  `kMaxRadiusRatio = 3` in the same bin without a station between them;
  `FrameTwist` when `right_i · right_{i−1} ≤ 0`; `FoldBack` when a ring
  vertex lies behind the previous ring's plane by more than 1e-4.
- **Survey fidelity.** `wallsOnSurface(debug)`: for each leg endpoint with
  passage LRUD, the four wall points `P + l·(−R)`, `P + r·R`, `P + u·ẑ`,
  `P − d·ẑ` (world, with the record's facing) must lie within
  `kWallTolerance = 0.15` m of the mesh surface (point–triangle distance
  over triangles whose AABB is within range, grid-accelerated) unless the
  reading was replaced by the shaft rule or dropped with a warning;
  otherwise `WallMissed`. This catches rings that swallow or ignore a
  reading.
- `check(mesh, debug, options) → Report` runs everything; `toText(report)`
  prints one line per issue for `INFO` and the gallery.

**Done when.** `test_cwPassageTubeMeshCheck.cpp` builds small hand-made
meshes and proves each detector fires once and only there: an open cube
(`OpenEdge`), a cube with one flipped face (`InconsistentWinding`), two
tetrahedra sharing one vertex only (`NonManifoldVertex`), an inside-out
cube (`NegativeVolume`), a zero-area triangle, two interpenetrating boxes
(`SelfIntersection` count > 0, none when the boxes are moved apart), a
small cube inside a large one (`NestedShell`). `test_cwPassageTubeBuilder`
asserts `check(...).issues.isEmpty()` on every geometric test **without a
junction** and still passes. The junction tests ("domes a junction arm...",
"takes the dome height...", "joins the Phake Cave arms...") also get the
assertion but are tagged `[!mayfail]` with a comment naming T07d: today's
dome overlaps the through tube, so `SelfIntersection` (and for Phake
`NestedShell`) fire by design until the merge lands. Record in the commit
body which issues each junction test reports; that list is T07d's starting
point. Runtime of `check` on the Phake Cave mesh under 200 ms in Debug.

**Commit subject.** `Add algorithmic artifact checks for passage tube meshes`

---

### T07f — Junction case catalog and sweep tests

**Goal.** A parametric generator of synthetic surveys covering the
junction space — plan angle, profile angle, size ratio, number of arms,
through bend, section change — shared by the tests and the gallery (T07g),
plus the sweep tests that run every mesh check over it.

**Read.** `plans/TUBE_JUNCTION_GALLERY.html` (the 18 hand-made cases,
`CASES` array, are the seed catalog), T07d's tests, `makeTripsInput`,
`kPhakeShots`/`phakeCaveSurvey()`.

**Create.** `cavewherelib/src/cwPassageTubeCases.h/.cpp`,
`testcases/test_cwPassageTubeCases.cpp`. **Modify.**
`test_cwPassageTubeBuilder.cpp` only if `makeTripsInput` moves.

**Spec.** `class cwPassageTubeCases` (production, because the gallery page
uses it):

- `struct Section { double left, right, up, down; }` and
  `struct ArmSpec { double planDegrees; double profileDegrees; double lengthMeters; Section at; Section atJunction; bool hasJunctionLrud; }`.
- `struct JunctionCase { QString name; QStringList tags; Section throughSection; Section throughAtJunction; double throughBendDegrees; double throughProfileDegrees; QVector<ArmSpec> arms; double sampleSpacing; }`.
- `toSurvey(const JunctionCase&) → cwCavingRegionData + positions` (one
  cave, one trip, a chunk per passage, LRUD strings as the test helper
  writes them). Station names `t0..tn`, `a<arm><k>`; the junction is `J`.
- `catalog()`: the 18 gallery cases by name and the Phake Cave survey, plus
  the **sweep**: every combination of plan angle `{90, 60, 45, 30, 20}`,
  profile angle `{0, 30, 60, 90 (pit), −30, −60, −90 (passage into
  shaft)}`, size ratio arm/through `{0.3, 1, 2.5}`, arm count `{1, 2
  opposite, 2 same side 45° apart, 3 (one per 120°)}`, through bend at `J`
  `{0, 40, 90}`, through section change at `J` `{none, ×2 past J}`. Skip
  combinations where a profile angle makes the plan angle meaningless
  (|profile| = 90 keeps plan 90 only). ~2,500 cases; tag each with its
  parameters so a test can filter (`tags` like `plan:60`, `arms:3`).
- `random(seed, junctionCount)`: a seeded `std::mt19937` tree survey:
  `junctionCount` junctions, leg lengths 3–15 m, bearings uniform, incl.
  mostly within ±20° with 10 % steep legs, LRUD log-uniform 0.1–5 m, 5 %
  stations without LRUD. Deterministic per seed.

Tests (`[PassageTube][cwPassageTubeCases]`):
- "the catalog builds clean meshes": for every catalog case, build with
  `buildDebug` and `cwPassageTubeMeshCheck::check`; `INFO(case.name)`;
  `CHECK(report.issues.isEmpty())`. Use Catch2 `GENERATE(from_range(...))`
  over the names so a failing case is reported by name; the full sweep is
  tagged `[Sweep]` and `[.]`-hidden by default (run with
  `cavewhere-test "[Sweep]"`; the nightly runs it), while a `kSmokeStride =
  25` subsample runs in the default suite. Every case with a junction is
  **expected to fail until T07d** (the dome overlaps the through tube), so
  these test cases carry `[!mayfail]` with a comment naming T07d; a
  companion test "the catalog cases without a junction build clean
  meshes" (straight, bent and looped cases from the sweep's `arms:0`
  variants, which the generator must also produce) is a hard `CHECK`.
- "random surveys build clean meshes": seeds `1..50`, `junctionCount` 1–6;
  the failing seed is in `INFO`; `[!mayfail]` for the same reason, plus a
  hard variant with `junctionCount = 0`.
- "a case survey round-trips its parameters": `toSurvey` of a 60° arm
  puts the arm's far station at the expected bearing and inclination;
  `random(seed, n)` is deterministic (two calls give identical surveys)
  and has exactly `n` stations of degree > 2.

**Done when.** The generator tests and the junction-free tests pass; the
`[!mayfail]` tests run and their current failure counts (smoke subsample,
full sweep, random seeds) are written in the commit body as the baseline
T07d must drive to zero.

**Commit subject.** `Add a parametric junction catalog and sweep tests for passage tubes`

---

### T07g — Tube gallery executable

**Goal.** A separate executable, `cavewhere-tube-gallery`, that links
`cavewherelib`, renders any catalog case or random seed with the real
builder and render object in 3D, shows the mesh-check report next to it,
and writes screenshot sheets on request. It is a developer tool for
visually checking tube generation; it ships with the build tree, not the
installer. The 2D HTML gallery is retired once this covers its cases.

**Read.** Top-level `CMakeLists.txt` (`qt_add_executable(CaveWhere ...)`,
`cavewhere-qml-test` and its `qt_import_qml_plugins`, the
`CaveWhereStyleplugin` link), `main.cpp` (engine setup:
`cwRootData::initCavewherelib()`, `QQuickStyle::setStyle("CaveWhereStyle")`,
`loadFromModule`), `test-qml/cavewhere-qml-test-main.cpp`,
`GLTerrainRenderer.qml` and `RegionViewer`/`RhiViewer` + `Scene` + `Camera`
(`cwRhiViewer.h`, `cwScene.h`, `cwCamera.h`), `cwRenderPassageTube.h`,
`cwRenderLinePlot.h`, `cwRenderBillboards`, `WindowGrabber` (used by
`tst_StyleGalleryPage.qml`), `cwPassageTubeMeshCheck`, `cwPassageTubeCases`,
`Theme.qml`, `StandardPage.qml`.

**Create.** `tube-gallery/main.cpp`, `tube-gallery/CMakeLists.txt`,
`tube-gallery/qml/TubeGalleryWindow.qml` (+ small components as needed,
in a QML module `cw.TubeGallery` that imports `cavewherelib`),
`cavewherelib/src/cwPassageTubeGalleryModel.h/.cpp` (in the library so a
C++ test can drive it), `testcases/test_cwPassageTubeGalleryModel.cpp`,
`scripts/gen-tube-gallery-screenshots.sh`. **Modify.** top-level
`CMakeLists.txt` (`add_subdirectory(tube-gallery)` next to the test
targets, desktop only), `cwRHIPassageTube.cpp`/`PassageTube.frag`/`.vert`
(clip plane and opacity uniforms), `cwRenderPassageTube.h/.cpp`
(properties `clipPlane: vector4d`, `clipEnabled: bool`, `opacity: real`).
`cwRhiScene`, `cwRhiViewer`, `cwScene` and `cwCamera` are **not**
modified: the gallery reuses the library's scene as is.

**Spec.**
- **Rendering: the scene stays, the tube object learns two things.**
  *Translucency* uses the existing `RenderPass::Transparent` and
  `cwRenderMaterialState::BlendMode::Alpha` the way `cwRHIGridPlane` does:
  when `opacity < 1`, `cwRHIPassageTube::gather` draws the fill in the
  `Transparent` pass with alpha blending and `setDepthWrite(false)`
  (depth test on, against the opaque scene), `color.a = opacity` in
  `TubeBlock`, and the pipeline key already carries `blendMode` so the
  opaque and translucent pipelines coexist; the wireframe variant stays in
  the opaque pass. Triangles within the tube are not depth-sorted, so
  overlapping walls blend order-dependently — acceptable for a diagnostic
  view and gone where T07d removes the overlap. *Cut plane* is a `vec4
  clipPlane` and a `float clipEnabled` appended to `TubeUniform`/
  `TubeBlock`; the vertex shader also passes the world position and the
  fragment shader discards when `clipEnabled > 0.5 && dot(vec4(worldPos,
  1), clipPlane) > 0`. The derivative normal already lights both sides,
  so the interior exposed by the cut lights correctly. These properties
  are product features (translucent tubes for fly-throughs), not gallery
  scaffolding; the app simply has no menu for them yet.
- **Viewer.** Use the plain `RhiViewer` (`cwRhiViewer`, properties
  `scene`, `camera`) with a `Scene {}` the window owns — not
  `RegionViewer`/`GLTerrainRenderer`, which bind to
  `RootData.regionSceneManager`. Check whether the app's turntable
  interaction works without `RootData`; if it does not, write a minimal
  orbit/pan/zoom with `DragHandler`/`WheelHandler` on the camera's view
  matrix.
- **Target.** `qt_add_executable(cavewhere-tube-gallery MANUAL_FINALIZATION
  main.cpp)`, links `Qt6::Quick Qt6::QuickControls2 cavewherelib`, and
  `CaveWhereStyleplugin` when that target exists, then
  `qt_import_qml_plugins` and `qt_finalize_executable`. `main.cpp` mirrors
  the app's engine setup (`initCavewherelib`, style, `QSurfaceFormat`
  samples) without creating `cwRootData` or opening a project, and loads
  `cw.TubeGallery/TubeGalleryWindow`. Command line (`QCommandLineParser`):
  `--case <name>` opens on that case, `--seed <n> [--junctions <k>]`
  opens a random survey, `--grab <dir>` writes the four preset
  screenshots of the opened case and exits, `--grab-all <dir>
  [--failing]` loops the catalog (or its failing cases) and exits, `--sweep`
  includes the `[Sweep]` cases in the list.
- **Model.** `cwPassageTubeGalleryModel : QAbstractListModel`
  (`QML_ELEMENT`): rows are the catalog cases plus any random seeds added
  with `Q_INVOKABLE void addRandom(int seed, int junctionCount)`; roles
  `name`, `tags`, `issueCount`, `issueSummary`. `Q_PROPERTY(int
  currentIndex)` selects a case; on change the model builds it with
  `cwPassageTubeBuilder::buildDebug` and `cwPassageTubeMeshCheck::check`
  on the GUI thread (cases are small; keep a case under 100 ms) and
  exposes `mesh` (to a `cwRenderPassageTube*` it owns or is given), the
  centerline geometry (to a `cwRenderLinePlot*`), `issues`
  (`QVector<QVector3D>` positions + text list for QML), `boundsCenter`,
  `boundsRadius`, and the first arm's horizontal direction for the
  profile preset. Issue counts for the list are computed lazily on first
  display and cached (a `--grab-all` run computes them all).
- **Window.** Left: text filter on name/tags, "failing first" toggle,
  the list with an issue badge (`Theme.success` for 0, `Theme.error`
  otherwise). Right: `RhiViewer` with its own `Scene` and `Camera`
  holding the `RenderPassageTube`, the `RenderLinePlot` and a
  `RenderBillboards` (or point) layer of red issue markers; a turntable
  interaction (reuse the app's interaction component if it is usable
  without `RootData`, else a minimal drag-to-orbit `DragHandler` on the
  camera's view matrix). Below the view: the issue list (`toText`) and
  the case's parameters. Presets as buttons: Plan (ortho, down −z),
  Profile (ortho, along the first arm's horizontal direction), Along arm
  (from the arm's far end toward `J`), 3/4 perspective; each fits the
  camera to `boundsCenter`/`boundsRadius`. Toggles: wireframe, **cut** (a
  clip plane through the junction normal to the camera's right axis; the
  fragment shader discards on the positive side), **translucent**
  (opacity 0.5, depth write off — interior faces show), **issues**
  (markers), **centerline**.
- **Screenshots.** "Grab sheet" writes `<dir>/<case>-<preset>.png` for the
  four presets with `QQuickWindow::grabWindow()` (a `WindowGrabber`-style
  helper in `main.cpp` or reused from the lib); `--grab-all` does it for
  every case without showing the list. The default `<dir>` is
  `<build-dir>/tube-shots/`. `scripts/gen-tube-gallery-screenshots.sh
  [--failing] [build-dir]` builds the target and runs `--grab-all`.
- QML rules: `QC.Label`, `Theme` tokens, `TapHandler`/`DragHandler`, no
  `property var`, `import QtQuick as QQ`.

**Done when.** `test_cwPassageTubeGalleryModel.cpp` (`[PassageTube]`):
the model lists the catalog, selecting the symmetric T builds a mesh and a
report, `addRandom(7, 3)` appends a row whose name carries the seed, and
the issue count of a junction-free case is 0. The executable builds and
`cavewhere-tube-gallery --case "1. symmetric T" --grab <tmp>` writes four
non-blank PNGs (verified by the fix-and-commit agent by running it and
checking the file sizes; the window may open briefly). The gallery shows
the T07c dome overlap as red `SelfIntersection` markers at the junction
(expected until T07d) and the translucent toggle makes the dome visible
inside the through tube.

**Commit subject.** `Add the passage tube gallery executable with mesh checks and screenshots`

---

### T07d0 — Inner-bend folds

**Goal.** Rings at a sharp bend keep their inner wall in front of their
neighbors' planes, so the fold-back pull in `placeRingVertices` never
collapses vertices into zero-area, crossing triangles. T07e's checks showed
this on every bend whose radius exceeds the fixed 0.5 m corner inset: the
vertical pit, the 90° bend at radius 3, the 120° bend, both loops, 16
junction-free catalog cases and 48 of 50 junction-free random seeds.

**Read.** Research §3.3.2 (corner rounding) and §3.6 row "Inner-bend
fold". Code: `chainSamples` (`kCornerInsetMeters`,
`kCornerInsetLegFraction`, `isSharpBendAfter`), `buildRings`,
`placeRingVertices`/`placeChainVertices` in `cwPassageTubeBuilder.cpp`;
the `[!mayfail]` tests "keeps the inner wall of a sharp bend clean" in
`test_cwPassageTubeBuilder.cpp` and "catalog cases without a junction and
with known builder defects build clean meshes" and "random surveys without
a junction build clean meshes" in `test_cwPassageTubeCases.cpp`; the
gallery's `arms:0` cases (`cavewhere-tube-gallery --case <name>`).

**Modify.** `cwPassageTubeBuilder.cpp`, `test_cwPassageTubeBuilder.cpp`,
`test_cwPassageTubeCases.cpp`.

**Spec.** Geometry: at a bend of turn angle `φ` with an inset ring
perpendicular to each leg at distance `inset` from the corner and the
station ring in the bisector plane, the inner-wall vertex of a ring lies
behind its neighbor's plane exactly when `ρ_inner · tan(φ/2) > inset`,
where `ρ_inner` is the ring's radius on the inside of the bend. Two
rules make that impossible:

1. **Radius-aware corner inset.** In `chainSamples`, for a sharp bend
   (`φ > options.subdivideBendDegrees`) compute `ρ_max` from the
   station's passage LRUD at the corner (largest of the readings of
   every record at that sample, `options.defaultRadiusMeters` when it
   has none) and set `inset = clamp(kCornerInsetSafety · ρ_max ·
   tan(φ/2), kCornerInsetMeters, kCornerInsetLegFraction · legLength)`
   per leg, with `kCornerInsetSafety = 1.1` and `kCornerInsetLegFraction`
   raised to `0.45` so the two insets of a short leg never cross. Mid-leg
   samples that fall inside the inset region are dropped (`appendSample`
   already merges rings within `kMinRingSpacingMeters`; also skip
   fractions between `cornerInsetFraction` and the corner).
2. **Concavity clamp.** When a leg is too short for the full inset, the
   fold condition can still hold; so in `buildRings`, for each ring at a
   sharp corner (the station ring and the two inset rings; a loop's
   closing corner included), clamp every bin whose direction `d` points
   into the bend (`d · inside > 0`, where `inside = normalize(t_out −
   t_in)` at the corner) to `radius ≤ inset / tan(φ/2) ·
   kConcavityClampFactor` with `kConcavityClampFactor = 0.95`, and mark
   the bin `measured = false`. Record one `Warning::BendClamped` per
   station where a measured reading was reduced by more than
   `kClampWarningMeters = 0.1`.
3. `placeRingVertices` keeps its pull as a safety net, but a pulled
   vertex now lands at `max(toPlane, kMinRadiusMeters)` only when the
   two rules left it behind the plane — which the tests assert never
   happens on the catalog.

Gentle bends (`φ ≤ subdivideBendDegrees`) are unchanged.

**Done when.**
- "keeps the inner wall of a sharp bend clean" loses `[!mayfail]` and
  passes: the vertical pit, the pit left on a new bearing, the 90° bend
  at radius 3, the 120° bend, and both loops give an empty
  `cwPassageTubeMeshCheck::check` report.
- "widens the corner inset with the ring radius": a 90° bend with `L = R
  = U = D = 3` on 20 m legs puts the inset rings `3.3 ± 0.05` m from the
  corner; with radius 0.3 they stay at `0.5` m.
- "clamps the inner wall of a bend on a short leg": the same 90° bend on
  4 m legs (inset capped at 1.8 m) gives inner-side radii `≤ 1.8 ·
  0.95 + 1e-3` on the corner rings, a `BendClamped` warning, and a clean
  check report.
- In `test_cwPassageTubeCases.cpp`, "catalog cases without a junction and
  with known builder defects build clean meshes" merges into "catalog
  cases without a junction build clean meshes" (hard, all 19), and
  "random surveys without a junction build clean meshes" loses
  `[!mayfail]`; if a seed still fails for a reason outside bends, name
  the seed and the issue in the commit body and keep only that seed in a
  named `[!mayfail]` test.
- Every other `[PassageTube]` test passes; the `[!mayfail]` junction
  counts in the commit body (they will change as bends stop contributing
  to Phake's numbers).

**Commit subject.** `Keep the inner wall of sharp bends in front of the neighboring rings`

---

### T07d — Clean junction merge (stitch pass)

**Goal.** Replace the dome at a junction arm with a watertight merge: the
through tube grows to take the arm's section across to the far wall, the arm
is trimmed where it meets the through surface, the through surface is cut
open inside the arm's mouth and the two boundaries are zipped. The mesh at a
junction becomes one closed, consistently wound surface with no faces inside
the tube, so transparency, cutaways and fly-throughs show no interior
geometry. The rule works on ring columns only, so it is the same for LRUD,
splay (T08) and scrap (T09/T10) rings.

The rule was reviewed in 2D in `plans/TUBE_JUNCTION_GALLERY.html` (variant
"Clean merge", 18 cases: open the file locally; the JavaScript functions
`ringTube`, `armTubeOf`, `extrudedInside`, `fillsFor`, `grownThroughRings`,
`unionTube` and the `merge` branch of `buildArm` are the reference
implementation of steps 1–3 below). Do not publish that file anywhere.

**Read.** Research §3.3.3 (rewritten for this task), §3.3.4, §3.6 rows
"Junction stubs visible from inside" and "Interpenetrating tubes", §4.4.
T07e–T07g landed first: run `cavewhere-test "[PassageTube]"` and
`cavewhere-test "[Sweep]"` before writing code to see the `[!mayfail]`
baseline, and use `cavewhere-tube-gallery --case <name>` to look at a
junction in 3D (translucent toggle shows interior faces). Code: `buildChains`, `buildRings`, `classifyChainEnds`, `domeHeight`,
`farWallDistance`, `stationRecords`, `placeRingVertices`,
`placeChainVertices`, `MeshWriter`, `addDome`, `addChainMesh` in
`cwPassageTubeBuilder.cpp`; `Chain`, `Ring`, `EndCap` in the header; the
tests "domes a junction arm to the through passage's far wall", "takes the
dome height from a junction without readings", "joins the Phake Cave arms
to their through passages" and the helpers `isClosedSurface`,
`signedVolume`, `makeTripsInput` in `test_cwPassageTubeBuilder.cpp`.

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `test_cwPassageTubeBuilder.cpp`.
**Create.** `cavewherelib/src/cwPassageTubeJunction.h/.cpp` (the stitch
pass; keep `cwPassageTubeBuilder.cpp` from growing past its current size)
and `testcases/test_cwPassageTubeJunction.cpp`.

**Spec.**

*Definitions.* A **tube** is a chain's ring sequence read as a solid: a
point `p` belongs to ring `i` when `|(p − c_i) · t_i|` is the smallest over
the chain's rings (nearest ring by along-distance); `p` is inside the tube
when its coordinates in ring `i`'s frame, `(a, b) = ((p − c_i) · right_i,
(p − c_i) · up_i)`, lie inside the ring polygon (`|(a, b)| ≤ ρ_i(θ)` with
`ρ_i` the piecewise-linear radius between bins). An **arm tube** is the arm
chain's tube with its last ring carried on past the junction: points beyond
the last ring along the arm's end tangent belong to the last ring. These
are pure functions of `Ring` data (`center`, `radii`, `frame`); implement
them once in `cwPassageTubeJunction` as `struct RingTube { const
QVector<Ring>* rings; QVector3D extendTangent; }` with `inside(p, tol)`,
`nearestRing(p)`, `localCoordinates(p, i)`.

*Where the pass runs.* After `buildRings` and before `placeChainVertices`:
`stitchJunctions(legs, chains, options, warnings)`. It works on
`Ring::radii` and on new per-chain mouth data; `placeChainVertices` then
places vertices as today (fold-back pull included) and `addChainMesh` emits
the result. `EndCap::Dome` becomes `EndCap::Mouth`; delete
`startDomeHeightMeters`/`endDomeHeightMeters`, `domeHeight`,
`farWallDistance` and `addDome`. `classifyChainEnds` is unchanged apart
from the rename: an arm end at a degree > 2 station that another chain
passes is a `Mouth`.

*Per junction* `J` (a canonical station with degree > 2 that a chain `T`
passes through — the through chain — and `m ≥ 1` arms `A_1..A_m` whose
`Mouth` end is at `J`; an arm starting at `J` is handled by reversing the
roles of its first and last rings, i.e. index arms from their junction end):

1. **Densify the through near `J`.** Let `reach = Σ_a maxρ(A_a's last
   ring) / max(|n_a|, kMinJunctionSine) + kJunctionReachMargin`, with
   `n_a = t_T(J) × t_a`, `t_a` the arm's unit tangent arriving at `J`,
   `kMinJunctionSine = sin(10°)` and `kJunctionReachMargin = 0.5` m. For
   consecutive through rings within `reach` of `J` (along-distance) whose
   spacing exceeds `kJunctionRingSpacing = min(options.sampleSpacingMeters,
   maxρ(arm) / kJunctionRingsPerRadius)` with `kJunctionRingsPerRadius =
   2`, insert interpolated rings (lerped `center` and `radii`, frame from
   `makeFrame` with the lerped tangent and slerped up, `measured` false,
   same `chainLeg`). Rings closer than `kMinRingSpacing` are never
   inserted. This only adds rings; existing station rings stay.
2. **Fills.** For each arm: `fill_a(p) = armTube_a.inside(p) ∧
   extruded_a(p)`, where `extruded_a(p)` tests `p` against the through
   ring nearest to `p` after extruding that ring's polygon along `±n̂_a`:
   with `n̂_a` expressed in the ring frame as `(n_r, n_u)`, `p` passes when
   `(a, b)` is inside the polygon or the line `(a, b) + s·(n_r, n_u)` hits
   the polygon for some `s`. When `|n_a| < kMinJunctionSine` (arm nearly
   parallel to the through) `extruded_a` is the plain through inside test.
   The fill is the arm's section carried from the through's near wall to
   its far wall; for a pit dropping into a passage (`t_a` vertical) it is
   the pit's section between the passage ceiling and floor; for a passage
   entering a shaft it is the passage's section between the shaft's near
   and far walls.
3. **Grow the through rings.** For each through ring within `reach` of `J`
   and each column `k`: march `r` from `radii[k]` outward in steps of
   `kGrowStep = 0.05` m while `c + r·d_k` is inside some fill (`d_k` the
   bin direction), stop at the first point outside, bisect `kGrowBisections
   = 12` times between the last inside and first outside radius, and set
   `radii[k] = max(radii[k], r)`; cap at `kMaxRadiusMeters`. Columns whose
   first step is outside every fill are unchanged. Growth tapers with the
   arm's outline, so no blend constant is needed. Mark grown bins
   `measured = false`.
4. **Trim each arm.** `union(p) = throughTube.inside(p, kJunctionTolerance)
   ∨ ∃a fill_a(p, kJunctionTolerance)` with `kJunctionTolerance = 0.02` m,
   using the through rings **before** growth (the fills already cover the
   grown region). For each arm column `k`, walk its vertex polyline from
   the far end toward `J` (and on along the extended last ring up to
   `reach` past `J`); find the first vertex inside `union`, bisect on the
   segment before it to `kGrowBisections` to get the entry point `M_k`.
   The arm keeps rings `0..m` where `m` is the last ring with every column
   still outside; rings after `m` are dropped and the **mouth loop** `M =
   (M_0..M_{N−1})` becomes the arm's final ring (`Chain::endMouth`, a
   `QVector<QVector3D>` of `N` points; `startMouth` for arms starting at
   `J`). An arm with no ring outside the union (a stub shorter than the
   through's wall) emits no mesh and adds a `Warning::ArmInsideThrough`
   for its far station. A column whose polyline never enters the union
   within `reach` (data where the arm misses the through entirely) ends
   at its own last ring vertex; the mouth then lies partly off the through
   surface and the zip below closes it anyway.
5. **Cut the window.** Parameterize the (grown) through surface by `(i,
   u)`: ring index `i` (fractional along between neighbors) and column `u
   ∈ [0, N)` (fractional, from the angle of `p` in ring `i`'s frame). Map
   each `M_k` to `(i_k, u_k)`; the mouth is a closed polygon in `(i, u)`
   (unwrap `u` across the `N → 0` seam by continuity). A through quad `(i,
   k)` (between rings `i, i+1`, columns `k, k+1`) is **removed** when its
   center lies inside the mouth polygon. When no quad is removed (a mouth
   smaller than one quad) remove the single quad containing the mouth
   centroid. Collect the window's boundary: quad edges with exactly one
   removed neighbor, oriented as the kept quads see them, chained into a
   loop (vertices on the grown through rings). Two arms at the same
   junction whose windows share a quad are not merged in this task: the
   second arm (by chain index) falls back to a **fan cap** over its mouth
   loop (closed, overlapping inside the first arm) and adds
   `Warning::JunctionOverlap`; the first proceeds normally.
6. **Zip.** Triangulate the band between the mouth loop `M` (N vertices,
   outward winding seen from the arm) and the window boundary loop `W`
   (`|W| ≥ 4` vertices): start `W` at the vertex nearest `M_0`, run both
   loops in the same angular sense about the mouth centroid in `(i, u)`
   space, and advance greedily (emit the triangle with the shorter
   bridging diagonal). Zip triangles belong to the arm's trip. The result
   per junction: through quads outside the window, the arm's strips up to
   ring `m`, one strip from ring `m` to `M`, and the zip band — closed and
   consistently wound with every other triangle in the mesh.

*Data.* `Chain` gains `startMouth`/`endMouth` (`QVector<QVector3D>`, empty
when not a mouth) and `windows` (`QVector<JunctionWindow>`, on the through
chain): `struct JunctionWindow { QSet<QPair<int,int>> removedQuads;
QVector<QVector3D> boundary; QVector<QVector3D> mouth; QUuid tripId; }`.
`addChainMesh` skips removed quads, emits the arm's last strip to the mouth
and the zip bands. Mouth vertices are shared between the arm's strip and
the zip band (one vertex index per `M_k`); window boundary vertices are the
through's own ring vertices (shared indices). Trip-boundary fan caps are
unchanged; hiding the through's trip opens the window (state this in the
`cwPassageTubeManager` docs).

*Constants* (anonymous namespace, `k` prefix): `kMinJunctionSine`,
`kJunctionReachMargin`, `kJunctionRingsPerRadius`, `kGrowStep`,
`kGrowBisections`, `kJunctionTolerance`.

**Done when.** Tests, all `[PassageTube]`; the geometric ones build the
survey with `makeTripsInput` and check with `isClosedSurface`,
`signedVolume` and a new helper `insideTube(debug, chainIndex, p)` that
reimplements the ring-tube inside test for the test side:
- "merges a symmetric T without interior faces": through a→b→c along +x
  (20 m, `L = R = U = D = 1` everywhere), arm d→b from −y (10 m, radius
  0.5). Closed; signed volume within 3 % of `π·1²·20 + π·0.5²·(10 − 1)`;
  no arm mesh vertex strictly inside the through tube (more than
  `kJunctionTolerance` inside); every mouth vertex within 0.05 m of the
  through surface; the through ring at b keeps `radii` within 1e-3 of 1
  (the arm is smaller than the through, so no growth).
- "fills a trench to the far wall": through 2 m wide, floor at the
  station (`D 0.05`), arm 1 m wide with `D 2.5`. The through rings within
  the arm's footprint have their down columns at `2.5 ± 0.1` m and the
  rings 1.5 m past the footprint keep `D 0.05`; closed.
- "chambers a through passage under a tall arm": arm `U 5`, through `U 1`
  (case 7): through ceiling columns inside the footprint grow to `5 ±
  0.1` m, the through's far-wall column is unchanged; closed.
- "lands a pit on the passage floor": vertical arm from 10 m above into a
  horizontal through (`U = D = 1`): the mouth loop lies on the through
  ceiling (every `M_k` within 0.05 m of `z = 1`), the through floor ring
  radii under the pit are unchanged, closed, volume within 3 % of the sum
  of the two cylinders minus the overlap counted once (`π r²·(10 − 1)` for
  the pit above the ceiling).
- "joins a passage to a shaft": vertical through (shaft radius 1.5),
  horizontal arm radius 0.8: closed; mouth on the shaft wall; shaft rings
  within the arm's height have their far-side columns grown by at most
  `0.2` m (the "D" flattening is bounded).
- "merges arms at a four-way junction": two arms from ±y into the same
  through: two windows, no shared quads, closed, both mouths on the
  surface.
- "keeps a loop's junction closed": the junction station lies on a loop
  chain; closed.
- "falls back to a fan cap when two mouths overlap": two arms 30° apart
  on the same side; mesh closed; exactly one `JunctionOverlap` warning.
- "drops an arm that lies inside the through": a 0.2 m arm into a 2 m
  passage; mesh closed; `ArmInsideThrough` warning; the arm chain's rings
  emit no triangles.
- "joins the Phake Cave arms to their through passages" is rewritten:
  closed mesh, no a13→a10 vertex inside the a9–a10–a11 tube beyond
  tolerance, mouth of a13→a10 within 0.05 m of the through surface.
- Remove every `[!mayfail]` tag added by T07e and T07f: the junction
  builder tests, "the catalog builds clean meshes" (smoke subsample and
  the full `[Sweep]`) and "random surveys build clean meshes" are hard
  checks and pass. A sweep case that still fails is fixed, or — only when
  it is one of the stated limits (overlapping mouths, grazing joins under
  20°) — moved to a named `[!mayfail]` test with the reason, and listed
  in the commit body.
- `cavewhere-tube-gallery --grab-all <tmp> --failing` writes no sheets
  (no failing case) for the default catalog; the Phake Cave a10 and a6
  junctions show no red markers.
- Every existing `[PassageTube]` test passes with `Dome` → `Mouth`.

**Commit subject.** `Merge junction arms into the through tube with a stitched mouth`

---

### T07h — Sections: sloped walls, steep rings, thin tips

**Goal.** Fix the three builder defects that T07e's checks report away from
every junction and that dominate the random seeds: a sloped passage misses
its up/down walls, a steep leg ignores its own left/right readings and ends
in a cone, and a thin section creases along its tip. Evidence from the
`[PassageTube]` run after T07d (the gallery sheets are in the commit
message of this task's predecessor; regenerate with `cavewhere-tube-gallery
--sweep --case "<name>" --grab <dir>`):

- `no arms, through profile 30, bend 0, same section` — a straight tube —
  reports `WallMissed` for t0 up/down, J up and t1 up. All 12 sloped
  junction-free cases, gallery 14 and the ±30°/±60° arm ends of the sweep
  fail the same way.
- `8. Pit dropping into a passage`: the 1.5 m round pit is drawn at the
  0.5 m default radius with a pointed cone on top; gallery 9's shaft ends
  likewise (`WallMissed` left/right at the shaft's ends).
- `7. Arm ceiling 5 m above the through ceiling`: a ridge of 150–160°
  `Crease` issues along the arm's ceiling (edges `N`–`N+16`, one ring to
  the next). Random seeds 12, 45 and most others show the same ridge
  creases on their thin sections.

**Read.** Research §3.2.1 (quadrant ellipse), §3.2.5 (ring fit), §3.3.2
(along a chain) and §3.6 rows "Sloped walls off the ring", "Pit at the
default radius" and "Thin-section ridge". Code: `lrudAcross`,
`fitLrudRing`, `borrowedRadii`, `capOffset`, `inclinationWeight`,
`chainFrames`, `binDirection`, `placeRingVertices` and `addCap` inside
`addChainMesh` in `cwPassageTubeBuilder.cpp`; `binDirection`,
`unpulledVertex`, `polarRadius`, `ringExtent`, `RingTube::wallDistance`
in `cwPassageTubeJunction.cpp`; the `[!mayfail]` tests "sloped catalog
cases without a junction build clean meshes", "catalog cases with builder
defects away from the junction build clean meshes" and "random surveys
without a junction and with known builder defects build clean meshes" in
`test_cwPassageTubeCases.cpp`; the `WallMissed` and `Crease` checks in
`cwPassageTubeMeshCheck.cpp`.

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeJunction.cpp`,
`test_cwPassageTubeBuilder.cpp`, `test_cwPassageTubeCases.cpp`.

**Spec.**

1. **Foreshortened up and down.** `lrudAcross` projects up and down onto
   `frame.up` the way it already projects left and right onto
   `frame.right` (`acrossRing(up, kWorldUp, frame.up)`), with or without a
   miter. A ceiling 1.5 m vertically above the axis of a 30° passage is
   `1.5 · cos 30°` from the axis in the ring plane; the ring previously
   used 1.5 and passed 0.23 m above the measured wall.
2. **Plumb dead ends.** The end ring of a chain whose end leg is gentle
   (`inclinationWeight < kSteepWeight`) lies in the vertical plane through
   the station: frame tangent = the leg direction's horizontal projection
   (normalized), up = world up, right = tangent × up; its radii are the
   station's raw readings, so the four wall points lie on the ring. The
   ring 1 m in is still square to the leg, so a tall section folds through
   it when `up · sin(incl)` or `down · sin(incl)` exceeds the ring spacing;
   treat the end as a bend by reusing `SharpBend` with the outgoing
   direction being the leg direction reflected across the horizontal
   plane (its bisector plane is then the plumb plane), so T07d0's inset
   and concavity rules place an inset ring and clamp the inner wall,
   warning `BendClamped` only when a measured reading moves. Steep end
   legs keep the perpendicular ring. Loops and mouths are unchanged.
3. **Steep rings from their own readings.** `fitLrudRing` currently skips
   every ring with `1 − inclinationWeight < kMinSampleWeight`, so a pit
   chain without a gentle ring borrows nothing and falls to
   `defaultRadiusMeters`. A steep ring whose records carry left or right
   now fits a round section of radius `(L + R) / 2` (one of them when the
   other is missing), marks every bin measured, and counts as fitted for
   `nearestFitted`. `borrowedRadii` stays for rings without readings.
4. **Flat pit caps.** A steep dead end extends by `capOffset` through a
   fan to a point; a floor or ceiling is flat. `addCap` emits one extra
   ring at the offset with the end ring's radii (quads between them) and
   fans that ring flat (center at the ring's centroid, no further offset).
   Gentle ends keep the fan on the plumb ring.
5. **Arc-length bins.** `fitLrudRing` samples the quadrant ellipse at
   uniform angles, so for `a = 0.8, b = 7` the bins at 67.5° and 90° land
   at (0.77, 1.86) and (0, 7) and one vertex spans 5 m of wall. Sample
   each bin by eccentric anomaly instead: `t_k = 2πk/N`, local vertex
   `(a cos t_k, b sin t_k)` with `a`, `b` the quadrant's horizontal and
   vertical readings (the filled-in bins and `borrowedRadii` use the same
   `t_k` with their interpolated `a`, `b`). Store the bin's polar angle on
   the ring (`Ring::binAngles`, monotonic in `k`, `θ_0 = 0`) and route
   every consumer through the ring: `binDirection(ring, bin)`,
   `unpulledVertex`, `polarRadius` (binary search in `binAngles`, chord
   interpolation), `ringExtent`, `lerpRing` (interpolates angles too),
   `growThrough` (grows along the bin's own direction), `zipBand`'s
   `outwardAt`, and `pulledVertexCount` in the tests. Rings of a chain
   keep bin `k` on the same side so the columns stay monotonic; a
   round ring has `θ_k = t_k` exactly, so every existing round-tube test
   is unchanged.
6. Move `binDirection`, `isFinite`, `hasValidFrame`,
   `chainStartStation`, `chainEndStation` and `kMinRingSpacingMeters` into
   one internal header `cwPassageTubeGeometry.h` (anonymous-namespace
   free functions become `inline` functions in namespace
   `cwPassageTubeGeometry`) so the builder and the junction pass share
   them (T07d review finding).

**Done when.**
- "foreshortens up and down on a slope": a straight 30° passage with
  `U 1.5, D 1` has every interior ring's up bin at `1.5 · cos 30° ± 1e-3`
  along `frame.up`, and the vertical wall points of every station
  (`isOnSurface`) lie on the mesh.
- "ends a sloped passage with a plumb ring": the same passage's end rings
  have `|frame.tangent.z()| < 1e-6`, their up vertices at `station + 1.5
  · worldUp ± 1e-3`; with `U 7` on a 30° leg the end gets an inset ring, a
  `BendClamped` warning only if the floor moved, and a clean check.
- "fits a pit from its own left and right": a 12 m vertical pit with
  `L = R = 1.5` and no other readings has every ring radius `1.5 ± 1e-3`
  and its top cap flat: the cap's triangles have normals within 1° of
  vertical; `WallMissed` is empty.
- "spreads bins by arc length on a thin section": a ring with `L = R =
  0.8, U = 7, D = 1` has no two consecutive vertices farther apart than
  the chord of its longest semi-axis over `N / 4` bins (with `N / 4` bins
  per quadrant the eccentric anomaly cannot do better than about `2.5 ·`
  the mean spacing on this section; the T07h run measured it), and the
  straight 0.8 × 7 passage yields no `Crease`.
- "sloped catalog cases without a junction build clean meshes" and
  "catalog cases with builder defects away from the junction build clean
  meshes" lose `[!mayfail]` (merged into the hard tests); gallery 7, 8, 9
  and 14 are clean.
- "random surveys without a junction and with known builder defects build
  clean meshes": the seeds that now pass move into `kCleanRandomSeeds`;
  seeds that still fail are listed in the commit body with the issue kind
  (hairpins and crossing passages are T07k's and T07l's; anything else is
  fixed here).
- Every other `[PassageTube]` test passes; the symmetric T, pit-into-
  passage and Phake gallery sheets show no new markers.

**Commit subject.** `Fit sloped, steep and thin sections to their walls`

---

### T07i — Junction trim against the mesh

**Goal.** Put every mouth vertex exactly on the through mesh, weld the
mouth to nearby through vertices, keep a dropped arm from leaving its
growth behind, and hide the fallback cap. After T07d the mouth is found
against `RingTube::wallDistance`, which interpolates the radius linearly
between bins while the mesh is the chord; at `r = 1.2` with 16 bins the
chord lies `r (1 − cos π/16) ≈ 2 cm` inside that surface, the size of
`kJunctionTolerance`, so mouth vertices float above the wall and the zip
triangles fold by ~150° (gallery 5, 11, the 1-arm sweep cases). Where an
arm's ceiling coincides with the through's (`plan 20, profile 0, ratio 1, 1
arm`), mouth and boundary vertices sit centimeters apart and zip into
slivers; Phake Cave's a10/a13/a14 bunch the same way. `plan 45, profile 0,
ratio 0.3, 3 arms` drops its 15° arm as inside the through, but
`growThrough` had already bulged the through rings where the arm's tip
poked out, leaving 176–180° creases at `y = 7`. When `cutWindow` returns
nullopt the arm's fan lies on the through wall and z-fights with it.

**Read.** Research §3.3.3 (trim, window, zip) and §3.6 row "Mouth off the
through mesh". Code: `trimArm`,
`growThrough`, `applyTrim`, `surfaceParameter`, `mouthPolygon`,
`removedQuads`, `windowBoundary`, `zipBand`, `cutWindow`,
`stitchJunctions` in `cwPassageTubeJunction.cpp`; `addCap`, `addZipBands`
in `cwPassageTubeBuilder.cpp`; the `[!mayfail]` tests "catalog cases at a
stated junction limit build clean meshes", "catalog junctions the merge
leaves unclean build clean meshes" and "sweep subsample at a junction limit
builds clean meshes" in `test_cwPassageTubeCases.cpp`; `rayTriangle` /
Möller–Trumbore in `cwPassageTubeMeshCheck.cpp` for the intersection
routine to copy.

**Modify.** `cwPassageTubeJunction.h/.cpp`, `cwPassageTubeBuilder.h/.cpp`,
`test_cwPassageTubeBuilder.cpp`, `test_cwPassageTubeCases.cpp`.

**Spec.**

1. **Exact trim.** After growth, build the grown through's quads near the
   junction (rings `span.first..span.last`, every column; each quad split
   along its shorter diagonal exactly as `MeshWriter::addSides` does, so
   the trim sees the triangles the mesh will have). `trimArm` walks each
   arm column's polyline (far end → junction → extension) and takes the
   first segment that enters the through: the entry point is the
   Möller–Trumbore hit on those triangles (nearest hit along the segment
   whose face normal points against the segment direction). The hit also
   yields the mouth point's surface parameter directly: ring `i +
   fraction` and column `k + fraction` from the hit triangle's barycentric
   coordinates. `surfaceParameter`/`slabOf` are no longer used for the
   mouth; `mouthPolygon` reads the stored parameters. The ring-tube
   `inside` test stays for growth and for the "lies entirely inside"
   decision.
2. **Welding.** After trimming, a mouth vertex within `kWeldMeters =
   0.03` of a window-boundary vertex is replaced by that vertex (the
   `ZipCorner` refers to the through ring/column, not `kMouthRing`), and
   consecutive mouth vertices within `kWeldMeters` of each other collapse
   to their first. The arm's last strip then ends on the welded loop;
   `zipBand` skips triangles with two equal corners, and the mesh writer
   shares the through vertex. Welding happens in (ring, column) space and
   3D together: a mouth point is welded only when both distances are
   small.
3. **Re-grow without dropped arms.** `stitchJunctions` grows, trims, and
   if `applyTrim` dropped any arm (or capped it as a fan because it had
   no ring outside), restores the through rings' pre-growth radii and
   `isGrown` flags and runs `growThrough` and the trims again without
   that arm (one repeat is enough: dropping an arm never makes another
   arm droppable).
4. **Hidden fallback cone.** When `cutWindow` returns nullopt, the arm
   keeps its mouth loop on the surface and closes with a fan whose center
   is pulled inside the through along the arm's arriving tangent by half
   the arm's largest radius (clamped to stay inside the through by the
   ring-tube test). The warning text says the arm "ends inside" the
   passage. The same cone applies to a `JunctionOverlap` fallback.
5. `cutWindow`'s orientation check keeps rejecting a mouth whose column
   order is reversed, but logs which case via the warning `Warning::
   JunctionUnzipped` (new type, message names the station) so a test can
   count them.

**Done when.**
- "puts every mouth vertex on the through mesh": symmetric T (T07d's
  case) — each mouth vertex lies within `1e-4` of some through triangle
  (point-to-triangle distance over the trip's triangles), not merely
  within 0.05 m of the ring tube.
- "welds a mouth that runs along the through's ceiling": the `plan 20,
  profile 0, ratio 1, 1 arm, bend 0, same section` sweep case builds
  clean; at least one zip corner references a through ring (welded).
- "re-grows the through after dropping an arm": `plan 45, profile 0,
  ratio 0.3, 3 arms, bend 0, section x2 past J` builds with an
  `ArmInsideThrough` warning and no `Crease`; the through rings 7 m past
  J have radii within `1e-3` of a build without that arm.
- "hides the fallback cone": a junction forced to the nullopt path (two
  arms 30° apart on the same side, the overlap fallback) has every fan
  triangle of the capped arm strictly inside the through tube; no
  `SelfIntersection` between the fan and the through wall.
- "catalog cases at a stated junction limit build clean meshes" (gallery
  5, 13) and "catalog junctions the merge leaves unclean build clean
  meshes" (gallery 11, Phake Cave) lose `[!mayfail]`; for Phake Cave the
  remaining issues, if any, are at stations reading 0 on one side and
  are listed in the commit body with their kind and count.
- The `sweep subsample at a junction limit builds clean meshes` cases with
  one arm, `bend 0` and `ratio ≤ 1` pass; the names of the cases still
  failing, grouped by `bend 90`, `ratio 2.5`, `3 arms`/`2 arms same side`
  (T07j, T07l), are in the commit body.
- Every other `[PassageTube]` test passes.

**Commit subject.** `Trim junction arms against the through mesh and weld their mouths`

---

### T07j — Elbow rings at bends and the through chosen by size

**Goal.** Make the two remaining geometric junction families mergeable:
a through that turns sharply at the junction, and an arm larger than its
through. `densify` only inserts rings between parallel rings, so a 90°
bend at J leaves the corner rings 1.65 m from J with nothing for the window
to land on (`bend 90` sweep cases: `Crease`, `SelfIntersection`, `Sliver`).
With `ratio 2.5` the arm's mouth wraps the whole through, `mouthPolygon`
returns nullopt and the arm ends in a fan; the big passage is the real
through at that node.

**Read.** Research §3.3.1 (chains), §3.3.2 (along a chain, corner
rounding) and §3.3.3. Code:
`buildChains` (`straightestUnusedLeg`), `chainSamples`, `fitSharpBends`
in `cwPassageTubeBuilder.cpp`; `densify`, `lerpRing`, `isParallel`,
`junctionRingIndex` in `cwPassageTubeJunction.cpp`; the sweep `bend:90`
and `ratio:2.5` tags in `cwPassageTubeCases.cpp`.

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeJunction.cpp`,
`test_cwPassageTubeBuilder.cpp`, `test_cwPassageTubeCases.cpp`.

**Spec.**

1. **Elbow rings.** At every sharp bend (`φ > subdivideBendDegrees`)
   `chainSamples` replaces the single station ring in the bisector plane
   with `n = max(1, ceil(φ / kElbowStepDegrees))` rings, `kElbowStepDegrees
   = 30`, whose tangents rotate from the incoming to the outgoing
   direction about the bend axis (`t_in × t_out`) and whose centers lie
   on the arc tangent to both legs at their inset rings: radius `R =
   inset / tan(φ/2)`, center `C = station + inside · inset / sin(φ/2)`
   with `inside = normalize(t_out − t_in)`; ring `j` (`j = 1..n`) sits at
   the arc angle `(j − ½) · φ / n` from the incoming inset ring, square
   to the arc there. The station's records are attached to the middle
   ring (the one whose tangent is the bisector); the others interpolate
   records between the inset rings. Each elbow ring's inner bins keep the
   concavity clamp (`radius ≤ R · kConcavityClampFactor` on the inside),
   so consecutive rings are in front of each other by construction;
   `fitSharpBends` applies to all of them. A bend with `n = 1` reproduces
   T07d0 exactly, so its tests stay.
2. `densify` treats consecutive rings whose tangents differ by at most
   `kElbowStepDegrees + 1°` as densifiable: `lerpRing` already slerps up
   and blends radii; add a slerp of the tangent and place the inserted
   centers on the chord. The junction ring index at a corner is the
   middle elbow ring.
3. **Through by size.** `buildChains` picks the next leg at a node of
   degree ≥ 3 by section area: among unused legs turning less than
   `kMaxChainTurnDegrees = 100°`, take the one whose mean section area at
   the node (from the leg's passage LRUD at that station, `π · (L+R)/2 ·
   (U+D)/2`, default radius when missing) is closest to the current
   leg's, ties by straightness; nodes of degree 2 keep the straightest
   continuation. Document the rule in the `buildChains` comment.

**Done when.**
- "rounds a sharp bend with elbow rings": a 90° bend at radius 1 on 10 m
  legs has 3 elbow rings, every consecutive pair in front of each other
  (`isInFrontOf`), a clean check, and the inner wall at the middle ring at
  `R · 0.95 ± 1e-3`.
- "merges an arm where the through turns 90°": `plan 90, profile 0, ratio
  1, 1 arm, bend 90, same section` builds clean with a window whose
  removed quads span at least two elbow rings.
- "chains the big passage as the through": gallery 3 (`Big arm into a
  small passage`) builds a chain whose legs are the arm and one through
  leg; the small leg carries the mouth; closed and clean.
- The `bend 90` and `ratio 2.5` sweep cases with one arm or two opposite
  arms pass in "sweep subsample at a junction limit builds clean meshes";
  the names still failing are listed in the commit body by family.
- Every other `[PassageTube]` test passes; gallery sheets for cases 3, 4
  and `10.` show no markers.

**Commit subject.** `Round sharp bends with elbow rings and chain through the largest passage`

---

### T07k — Hairpins and cave-like random surveys

**Goal.** Stop hairpins from blowing up the miter, and make the random
generator produce surveys a cave surveyor could have made. `random()` draws
bearings uniformly, so near-180° turns are common; `stretchAcrossMiter`
divides by `sqrt(1 − inside² · sin²(φ/2))` and `lrudAcross` by `cos(φ/2)`,
both unbounded as `φ → 180°` (seed 12's plan view is one giant spike).
Independent LRUD per station (0.1–5 m) next to 3 m legs makes cones that
no inset can keep clean.

**Read.** Code: `buildChains`, `lrudAcross`, `stretchAcrossMiter`,
`SharpBend` in `cwPassageTubeBuilder.cpp`; `cwPassageTubeCases::random`;
the random tests in `test_cwPassageTubeCases.cpp` and the gallery's
`--seed`/`--junctions` options.

**Modify.** `cwPassageTubeBuilder.cpp`, `cwPassageTubeCases.h/.cpp`,
`test_cwPassageTubeBuilder.cpp`, `test_cwPassageTubeCases.cpp`,
`tube-gallery/main.cpp` (a `--wild` flag).

**Spec.**

1. **Chains break at hairpins.** `buildChains` never continues through a
   turn of `kMaxChainTurnDegrees = 100°` or more (shared with T07j);
   both legs end there with fan caps that overlap inside each other.
   `stretchAcrossMiter` and `lrudAcross` clamp the miter stretch to
   `kMaxMiterStretch = 3` times the square radius.
2. **Cave-like `random`.** Each leg's bearing drifts at most
   `kRandomMaxTurnDegrees = 60°` from the previous leg of its passage
   (the first leg of a branch leaves its junction at least 30° from the
   passages already there); LRUD readings are a random walk in log
   space along the passage (step at most ×2 per leg, within 0.1–5 m),
   with the up/down of a steep leg's stations set to the shaft depth as
   a surveyor would read it; no station lies within 1 m of another
   passage's leg. The old generator becomes `randomWild(seed,
   junctions)`; the gallery's `--wild` flag selects it.
3. Tests: the clean-mesh random tests use `random` (cave-like) and lose
   `[!mayfail]`; `randomWild` keeps only the closure and no-degenerate
   tests (hard) plus a `[!mayfail]` clean test named "wild random surveys
   build clean meshes".

**Done when.**
- "breaks a chain at a hairpin": three legs with a 170° turn build two
  chains, no `Crease` above 150°, closed mesh.
- "random surveys build clean meshes" and "random surveys without a
  junction build clean meshes" are hard tests over all 50 seeds;
  `kCleanRandomSeeds` is gone. A seed failing only on `SelfIntersection`
  between different chains waits for T07l: list it in the commit body
  and tag it in a named `[!mayfail]` test.
- "random surveys are deterministic and have the requested junctions"
  still holds for `random` and `randomWild`.
- Every other `[PassageTube]` test passes.

**Commit subject.** `Break chains at hairpins and generate cave-like random surveys`

---

### T07l — Checker: overlaps between passages and sliver policy

**Goal.** Make the checker report what a viewer sees. Two tubes that run
into each other (three 3 m arms at 120°, random passages crossing) draw
as their union — a hard seam, no z-fighting — which is the look the clean
merge gives up on at its stated limits; the checker counts every crossing
as `SelfIntersection` (5913 in one sweep case). A long thin triangle
renders fine unless it is nearly degenerate; the sliver check flags every
triangle of aspect ratio over 50, which includes legitimate quads between a
0.05 m bin (a station on the wall, Phake Cave) and its 1 m neighbor.

**Read.** Code: `selfIntersections`, `degenerateTriangles`, `Report`,
`Options` in `cwPassageTubeMeshCheck.h/.cpp`; `Debug` in
`cwPassageTubeBuilder.h`; `MeshWriter` in `cwPassageTubeBuilder.cpp`; the
gallery's issue markers in `cwPassageTubeGalleryModel.cpp`.

**Modify.** `cwPassageTubeMeshCheck.h/.cpp`, `cwPassageTubeBuilder.h/.cpp`,
`cwPassageTubeGalleryModel.cpp`, `test_cwPassageTubeMeshCheck.cpp`,
`test_cwPassageTubeCases.cpp`.

**Spec.**

1. `Debug` gains `QVector<int> triangleChain` (one entry per triangle:
   the chain whose rings, cap or mouth strip emitted it; a zip band's
   triangles belong to the through chain). `MeshWriter` records it.
2. `selfIntersections(mesh, range, debug)` classifies each crossing: both
   triangles from the same chain, or either from a zip band, is
   `SelfIntersection`; otherwise it is the new kind `PassageOverlap`,
   reported with the two chains' first stations. `check` puts
   `PassageOverlap` in `Report::warnings` (new field, same `Issue` type)
   and keeps `Report::issues` for everything else; `toText` prints both.
   The gallery draws overlaps in a second color (`Theme` token) and its
   `--failing` filter counts issues only.
3. `SliverTriangle` requires both `aspectRatio > maxAspectRatio` and the
   triangle's smallest altitude below `Options::minSliverAltitudeMeters =
   0.02`; `DegenerateTriangle` is unchanged.

**Done when.**
- "classifies a crossing between two passages as an overlap": two
  straight tubes crossing at 90° give zero `issues` and a `PassageOverlap`
  warning; a tube whose ring is folded on itself still gives
  `SelfIntersection`.
- "keeps large thin triangles": a quad between a 0.05 m bin and a 1 m bin
  is not a sliver; a triangle with an altitude of 5 mm is.
- "sweep subsample at a junction limit builds clean meshes", "full sweep
  at a junction limit builds clean meshes" and every random clean test
  lose `[!mayfail]`; `cavewhere-tube-gallery --grab-all <tmp> --failing`
  writes no sheets. A case that still fails is fixed here or named in
  the commit body with the kind, and only then kept in a named
  `[!mayfail]` test.
- Every other `[PassageTube]` test passes.

**Commit subject.** `Report passage overlaps apart from self-intersections and only true slivers`

---

### T07m — Mouth loop sampled to the through's grid

**Goal.** Give the mouth as many vertices as the through grid it crosses.
The mouth loop has one vertex per arm column (16), but a mouth as big as
the through, or oblique to it, spans 10–50 through rings: random seed 1's
only junction has a 10 m mouth whose edges are up to 4.5 m long over a
0.5 m through spacing; seed 4's arm c4 (3.97 m against a 3.12 m through
at 35°) has an 11 m mouth edge over 49 rings and strips up to 18 m long.
A mouth edge that long is a chord through the through's surface (it cuts
across the roof-to-wall corner), so every zip triangle on it is a sliver
along the same line as the arm's strip triangle beside it (fold 150–180°),
zip triangles fanned from one mouth vertex to a dozen boundary vertices
fold on each other, and the strip quad between two columns whose mouths
sit 0.5 m and 5 m from the end ring twists. Diagnosed on the 39
`[!mayfail]` random seeds with a per-issue classifier
(`/Users/cave/.claude/jobs/ab1fa6ed/tmp/test_zzTubeDiag.cpp`, kept out of
the tree): all 273 creases and all 350 self-intersections and slivers lie
in zip bands or mouth strips; 52 of the 58 windows with creases have a
mouth edge longer than twice the through's spacing at the junction, and
the sloped single-arm `bend 40` / `bend 90` sweep cases show the same
numbers (edges 2.2–2.9 m over 0.43 m spacing). Welding (T07i) never fires
because no mouth vertex lands within 3 cm of a grid vertex; a denser mouth
gives it something to weld.

**Read.** Research §3.3.3 step 3 (trim, mouth loop) and 4 (cut and zip),
§3.6 row "Sparse mouth loop". Code: `trimArm` (the `entry`,
`ontoThroughWall` and `findEntry` lambdas), `Trim`, `cutArm`,
`mouthPolygon`, `weldedMouthCorners`, `cutWindow`, `zipBand`,
`closeWithFan`, `applyWelds`, `stitchJunctions` in
`cwPassageTubeJunction.cpp`; `Chain::startMouth/endMouth`,
`JunctionWindow::mouthCorners`, `ZipCorner` in `cwPassageTubeBuilder.h`;
`addCap` (the `mouth.size() == resolution` branch) and `addZipBands` in
`cwPassageTubeBuilder.cpp`; the mouth consumers in
`cwPassageTubeMeshCheck.cpp` (`ringIssues`) and the gallery model; the
`[!mayfail]` tests "random surveys with junctions at the merge's limits
build clean meshes" (`kJunctionLimitRandomSeeds`) and "sweep subsample at
a junction limit builds clean meshes", `kOpenSweepCases`,
`bendsSharplyAtJunction` in `test_cwPassageTubeCases.cpp`.

**Modify.** `cwPassageTubeJunction.cpp`, `cwPassageTubeBuilder.h/.cpp`,
`cwPassageTubeMeshCheck.cpp` (only if it reads mouths),
`test_cwPassageTubeJunction.cpp`, `test_cwPassageTubeCases.cpp`.

**Spec.**

1. **Mouth points carry their column.** `cwPassageTubeBuilder::MouthPoint
   { QVector3D position; double column; }` where `column` is the arm bin
   plus the fraction toward the next bin (`k + f`, `0 ≤ f < 1`);
   `Chain::startMouth`/`endMouth` become `QVector<MouthPoint>`, in column
   order, one or more points per column, and `JunctionWindow::
   mouthCorners` has one corner per mouth point. `Trim::parameters` has
   one entry per mouth point.
2. **Refine between columns.** `trimArm` keeps its integer-column pass
   (`keptRings` is unchanged). Then, for each pair of adjacent columns `k,
   k + 1`, it traces sub-columns at fractions `f`: the polyline is the
   point `lerp(unpulledVertex(ring, k), unpulledVertex(ring, k + 1), f)` on
   every ring from the kept end ring to the last, plus the same tangent
   extension, walked with the same `entry` and `ontoThroughWall` logic
   (a sub-column starts at the kept end ring, so it starts outside the
   through by the integer pass's choice of that ring; a sub-column whose
   start the chord puts inside the through is skipped). It bisects
   recursively while two adjacent mouth points are farther apart in 3D
   than `kMouthStepFactor = 1.5` times the through's junction spacing
   (the `spacing` `stitchJunctions` passed to `densify`; pass it into
   `trimArm`), to at most `kMaxMouthDepth = 4` levels (16 points per
   column), so a 16-column arm has at most 256 mouth points. Points are
   appended in column order.
3. **The strip to the mouth** is a fan per column: for column `k` with
   mouth points `m_k = m_0, m_1, …, m_n = m_{k+1}` (all points with column
   in `[k, k + 1]`), triangles `(e_k, m_i, m_{i+1})` for `i < n` and then
   `(e_k, m_n, e_{k+1})`, wound like `addSides` for an arm arriving at its
   end and reversed for one leaving at its start; a triangle with two
   equal vertices (welded) is skipped. `MeshWriter::addMouthStrip` replaces
   the `addSides(endRing, mouth)` calls in `addCap` and `addZipBands`;
   `closeWithFan` and the fan cap close over the mouth loop with
   `mouth.size()` vertices (`addFan` takes the loop's count). The result
   is manifold: each end-ring edge `e_k e_{k+1}` belongs to one strip
   triangle and one ring quad.
4. **Zip over mouth points.** `mouthPolygon`, `weldedMouthCorners`,
   `cutWindow`'s loop, `zipBand`, `applyWelds` and `addZipBands` iterate
   mouth points instead of columns; `isAtMouth` compares the kept ring's
   vertex `k` with the mouth point at column `k`. `removedQuads` and
   `windowBoundary` are unchanged. The zip DP is `O(M · B)` with `M ≤
   256`; keep it so.
5. **Tests moved, not deleted.** `kOpenSweepCases` loses the four `ratio
   2.5, 1 arm, bend 0` names (they build clean since T07l; confirmed with
   the diagnostic) so the full sweep covers them. Random seeds that build
   clean leave `kJunctionLimitRandomSeeds` for the hard test; the sweep
   cases that build clean leave the open set. The remaining open seeds and
   cases are listed in the commit body with the issue kinds and, per
   window, the longest mouth edge, the through spacing, how many columns
   the window wraps and `armMean / thrMean` (the diagnostic prints these).

**Done when.**
- "samples the mouth to the through's grid": random seed 1 (`Cases::
  random(1, 1)`) has more than 16 mouth points on its one zipped arm and
  no two consecutive mouth points farther apart than `2 ·` the through's
  junction spacing; the symmetric T keeps exactly 16 (its mouth edges are
  already short).
- "meshes the mouth strip as a closed fan": every random seed still builds
  a closed, consistently wound tube ("random surveys build closed tubes").
- "zips a sparse mouth": a test junction built from random seed 1's trip
  reports no `Crease`, `SliverTriangle` or `SelfIntersection`.
- At least 30 of the 39 seeds in `kJunctionLimitRandomSeeds` build clean
  and move to the hard test; every single-arm sweep case without a
  `JunctionOverlap` warning builds clean except the `plan 20` (grazing)
  cases; names of the rest in the commit body.
- Every other `[PassageTube]` test passes.

**Commit subject.** `Sample each junction mouth to the through's grid`

---

### T07n — One window for mouths that share through quads

**Goal.** Zip every arm of a junction whose windows overlap instead of
hiding the second arm in a cone. In the sweep, 1001 of 1836 junction
cases end with a `JunctionOverlap` warning (every 2- and 3-arm junction
whose arms are the through's size or larger: `2 arms opposite, ratio 1`
is 50 of 50 cases, `3 arms, ratio 1` 50 of 50), and the tests accept
them as a stated limit; on screen the second arm is a passage ending in a
cone inside the first, and Phake Cave's a10 is one of them. For opposite
arms the mouths are far apart: each mouth wraps more than half the
through, so the two windows share only the roof and floor quads.

**Read.** Research §3.3.3 step 4 and "Known limits" (merge the windows).
Code: `removedQuads`, `windowBoundary`, `zipBand`, `overlapsWindows`,
`cutWindow`, the `JunctionOverlap` branch of `stitchJunctions`,
`closeWithFan` in `cwPassageTubeJunction.cpp`; `JunctionWindow` in
`cwPassageTubeBuilder.h`; `addChainMesh`'s `removedColumnsByRing` and
`addZipBands` in `cwPassageTubeBuilder.cpp`; `overlapsMouths` and the
sweep tests in `test_cwPassageTubeCases.cpp`.

**Modify.** `cwPassageTubeJunction.cpp`, `cwPassageTubeBuilder.h/.cpp`,
`test_cwPassageTubeJunction.cpp`, `test_cwPassageTubeCases.cpp`.

**Spec.**

1. **Measure first.** Before changing code, count over the sweep (the
   `[Sweep]` tests, or a scratch test) how many `JunctionOverlap` cases
   have mouth polygons that are disjoint in (ring, column) space and how
   many have polygons that intersect (arms that run into each other
   above the through). Put both numbers in the commit body.
2. **Windows merge.** `JunctionWindow` holds several mouths: `QVector<
   WindowMouth { int armChain; bool isArmStart; QVector<ZipCorner>
   mouthCorners; }`. `cutWindow` becomes two steps: each arm's
   `removedQuads` and mouth loop are computed as now; then windows on the
   same through whose `removedQuads` intersect are merged (union of quads,
   `windowBoundary` of the union) as long as their mouth polygons are
   disjoint. Mouth polygons that intersect keep today's fallback
   (`JunctionOverlap` warning, hidden cone) — the remaining limit.
3. **Zip a band with holes.** `windowBoundary` returns every boundary loop
   of the union (two when the window takes the whole circumference over
   some rings, front and back). The loops to join are the boundary loops
   and the mouth loops. Join them into one inner loop with bridges: start
   from one boundary loop as the outer loop; repeatedly pick the loop not
   yet joined whose closest vertex pair to the joined inner loop (in
   parameter space, the bridge segment crossing no loop edge) is nearest,
   and splice it in through that bridge (the bridge edge is traversed
   once in each direction, so it ends up shared by exactly two zip
   triangles, one on each side). Run the existing two-loop DP between the
   outer loop and the joined inner loop. When only one boundary loop and
   one mouth exist this reduces to today's `zipBand`; test that the
   symmetric T's zip is unchanged.
4. `addChainMesh` removes the union's quads; `addZipBands` emits the
   strips of every mouth of a window and the window's zip; the
   `triangleChain` of a merged window's zip stays the through.
5. `overlapsMouths` in the tests only exempts cases whose warning text
   says the mouths intersect; the warning for that case says so.

**Done when.**
- "merges the windows of opposite arms": `plan 90, profile 0, ratio 1, 2
  arms opposite, bend 0, same section` builds clean with no
  `JunctionOverlap` warning and one window holding two mouths.
- "zips a window that takes the whole circumference": a case whose union
  window leaves front and back boundary loops (two opposite arms larger
  than the through, `ratio 2.5`) builds closed and clean.
- "keeps the fallback for arms that run into each other": two arms 30°
  apart on the same side still warn and cap, with no `SelfIntersection`
  between the cone and the through wall.
- The sweep's `JunctionOverlap` count drops to the number of
  intersecting-mouth cases measured in step 1; "sweep subsample at a
  junction limit" and "full sweep at a junction limit" lose
  `[!mayfail]` or the cases left are named in the commit body by family.
- Every other `[PassageTube]` test passes.

**Commit subject.** `Zip every arm of a junction whose windows share through quads`

---

### T07p — No caps where the survey continues

**Goal.** A passage surveyed over several trips, or doubling back on
itself, reads as one tube. A real project shows flat and fan caps at
tie-in stations where the survey continues (ground rule: the project
only inspires the synthetic reproductions below and appears nowhere in
source or tests). A probe over its Debug (612 legs, 114 chains) found
150 chain ends at stations with two or more legs, from five mechanisms,
in order of how many caps each draws:

- *Hairpin breaks.* 30 stations where the two legs turn 100°–170° and
  `buildChains` ends both chains with a `Fan` (`kMaxChainTurnDegrees`,
  `isHairpin`, `classifyChainEnds`). Three are cross-trip tie-ins: a
  later trip starts at an older station and heads back along or across
  the old passage. The two tube ends do not cover each other: the
  adjacent shots are partly outside the mesh and both discs show.
- *Junctions no chain passes through.* A degree-3 station whose walk
  seeded itself (no chain reached it from a dead end, and `walkFrom`
  only walks backward through a `throughPair`) starts every arm there,
  so all three ends are `Fan` and the junction draws three discs.
- *Fan fallbacks with a mouth.* `JunctionOverlap` / `JunctionUnzipped`
  cones with 20–78 mouth points are not reliably hidden inside the
  through (T07n and T07o).
- *A re-shot tie-in.* `extractLegs` appends one `Leg` per shot. One
  station pair surveyed in two trips gives two coincident legs, a
  degree-5 station, an `ArmInsideThrough` arm, and a fan fallback.
- *Boundary discs.* `addTripBoundary` emits two coincident flat caps on
  the shared ring at every trip change inside a chain (28 in the
  project), so hiding a trip leaves the other closed. One stands in a
  junction window (its side quads removed, the disc is a wall across the
  opening); six poke partly out on elbow or grown rings.

**Read.** Research §3.3.1 (chains, hairpins), §3.6 rows "Open tube ends
when a trip is hidden" and "Caps at trip boundaries"; §4.2
(`tripIndexRanges`). Code: `extractLegs`, `buildChains` (`continuation`,
`isHairpin`, `walkFrom`, `throughPairs`), `classifyChainEnds`,
`addTripBoundary`, `MeshWriter` in `cwPassageTubeBuilder.cpp`;
`closeWithFan` in `cwPassageTubeJunction.cpp`; `tripIndexRanges` and its
consumer in the render object; `toSurvey(const QList<Trip>&)` in
`cwPassageTubeCases.cpp`; `caps each trip of a chain that spans two
trips` and the T07k hairpin tests in `test_cwPassageTubeBuilder.cpp`;
the `CenterlineOutside` rule from T07q.

**Modify.** `cwPassageTubeMeshCheck.h/.cpp`,
`test_cwPassageTubeBuilder.cpp` first (step 1); then
`cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeMesh.h`, the render object,
`cwPassageTubeCases.cpp`.

**Spec.**

1. **Reproduce before fixing.** Add a checker rule
   `Issue::Type::CapExposed` (needs the Debug): the writer tags cap
   triangles as `Source::Cap` the way zip triangles are tagged; a cap
   is an issue when any of its vertices (sampled over the disc, 32
   points) lies outside every other chain's mesh and its station has
   two or more legs, or when it coincides with another cap. A cap is
   allowed at a true dead end (one leg) and at a junction arm's hidden
   cone that lies inside the through. Then tests in
   `test_cwPassageTubeBuilder.cpp`, built with `toSurvey(QList<Trip>)`,
   all `[!mayfail]` until the later steps:
   - `continues a passage across a trip boundary without a cap`: a–b–c
     collinear, trip 1 `a→b`, trip 2 `b→c`, equal LRUD; and a variant
     where trip 2 reads b twice as large.
   - `hides a trip boundary disc inside a junction window`: the two-trip
     passage with a third trip's arm joining at b, so the window removes
     b's side quads; the disc must not show across the opening.
   - `covers a hairpin tie-in`: trip 1 `a→b→c` straight, trip 2 `c→d`
     turning 150° in plan back beside `b→c` with the same LRUD. Both
     stations and the sampled centerline are inside the mesh
     (`CenterlineOutside`), no `CapExposed`.
   - `chains through a junction no walk reaches first`: a Y whose three
     arms are each 12 m and whose walks all start at their own dead ends
     but reach J only after the junction's own seed; the test forces the
     order by making every dead end a lower rank than J. One chain passes
     J; the third arm has a mouth.
   - `treats a re-shot tie-in as one leg`: trip 1 `a→b→c`, trip 2 `b→c→d`
     with the `b→c` readings 5 % different. One chain of three legs, no
     junction, no `JunctionUnzipped` / `JunctionOverlap`, caps only at a
     and d, a `RepeatedShot` warning.
   - `keeps a cap at a true dead end` as the control.
   Run `CapExposed` in `checkMeshClean` and list what else it flags in
   the commit body. Commit this step on its own (`Report caps that show
   where the survey continues`).
2. **Merge re-shot legs.** In `extractLegs`, legs with the same canonical
   unordered station pair collapse into one: the first trip's leg owns
   the tube, readings are averaged per station when both trips have
   them, `Warning::RepeatedShot` names both trips. A pair whose
   positions differ by more than `kRepeatedShotTolerance` (0.1 m) stays
   two legs.
3. **Walk through a seeded junction.** `walkFrom` walks backward from a
   junction seed along its straightest continuation even without a
   `throughPair`, so a junction no walk reached first still has a chain
   passing it and only the remaining leg is an arm.
4. **Cover hairpins.** At a hairpin break each chain's end ring moves
   past the station along its leg by the station's reading in that
   direction (the T07h dead-end offset), and closes with a dome (fan
   over a half ring of the end's radii) instead of a flat disc, so the
   two ends overlap and the station lies inside both; the `Fan` type
   gains `EndCap::Dome`. The research §3.6 row says how the domes
   overlap (done).
5. **Boundary discs only when needed.** `addTripBoundary` emits each
   trip's disc into its own `cwPassageTubeMesh::tripBoundaryRanges` entry
   keyed by (trip, neighbor trip), clipped to the ring's kept columns
   when a window removed some; the render object draws a boundary disc
   only while its neighbor trip is hidden. With all trips visible the
   mesh has no triangles at the boundary.
6. Add a catalog case `20. Passage surveyed over two trips with a
   hairpin tie-in and a re-shot leg` so the gallery shows the fixed
   shape.

**Done when.**
- Step 1 lands first with the rule, the six tests (five `[!mayfail]`,
  the control passing).
- After steps 2–6 the five tests pass without `[!mayfail]`: no
  `CapExposed` anywhere in `[PassageTube]`, the two-trip mesh has no
  boundary triangles while both trips are visible and trip 1 is closed
  at b when trip 2 is hidden, the hairpin tie-in and the seeded junction
  keep their centerlines inside, the re-shot survey builds one chain
  with a `RepeatedShot` warning.
- Every other `[PassageTube]` test passes.

**Commit subjects.** Step 1: `Report caps that show where the survey
continues`. Steps 2–6: `Continue passages across trips, hairpins and
re-shot tie-ins`.

---

### T07q — Cut a junction arm where its axis enters the through

**Goal.** Keep an arm's rings up to the last ring whose center is
outside the grown through, so the strip to the mouth never spans a bend
or more than one ring spacing. Today `trimArm` keeps rings only up to the
first column that touches the through anywhere along the arm
(`keptRings = min over columns of lastOutside + 1`, from the far end).
In a real project (which appears nowhere in source or tests) a 3–5 m
passage running 15 m beside a 16 m wide room through two 45° bends is
cut 15 m short of the junction, and a default-radius passage bending 74°
inside a 6 m through's reach loses its elbow and its last leg: one
straight strip from the kept ring to a 35–43 point mouth replaces them,
and four stations and their shots lie outside the tube. The checker has
no rule for that, which is why the catalog never showed it.

**Read.** Research §3.3.3 steps 3–4. Code: `ringsTowardJunction`,
`trimArm` (`findEntry`, `entry`, `ontoThroughWall`, `closestToWall`,
`betweenColumns`, `isAtMouth`), `cutArm`, `growAndTrim`, `leaveOutArm`,
`junctionSolids` in `cwPassageTubeJunction.cpp`; `addChainMesh`'s
`addCap` mouth branch and `MeshWriter::addMouthStrip` in
`cwPassageTubeBuilder.cpp`; `puts every mouth vertex on the through
mesh` in `test_cwPassageTubeBuilder.cpp`.

**Modify.** `cwPassageTubeMeshCheck.h/.cpp`, `cwPassageTubeCases.cpp`,
`test_cwPassageTubeCases.cpp` first (step 1); then
`cwPassageTubeJunction.cpp`, `test_cwPassageTubeJunction.cpp`.

**Spec.**

1. **Reproduce before fixing.** Add the checker rule
   `Issue::Type::CenterlineOutside` and `static QVector<Issue>
   centerlineInside(const cwPassageTubeMesh&, const Debug&, const
   Options&)`: for every leg of every chain that has rings, both
   stations and points every `kCenterlineSampleMeters` (0.25) along the
   leg must be inside the mesh (parity vote of three skewed rays over
   the whole mesh), excluding legs of a dropped arm (`ArmInsideThrough`)
   and points within `kWallTolerance` of a triangle. `check()` runs it
   when the Debug has chains. Add two gallery cases:
   - `21. Small arm along the wall of a big room`: through `{8, 8, 6,
     1.5}`, an arm `{3, 2, 3.5, 2}` leaving at plan 70 and climbing 20°,
     with a second chunk so the arm has three legs turning 45° then 80°
     in plan, 5 m and 12 m from J, all outside the room.
   - `22. Default-radius arm bending inside a big through's reach`:
     through `{6, 6, 6, 1.5}`, an arm without LRUD (default radius) that
     turns 74° in 3D 4.9 m from J.
   Both fail the new rule today: the arm's bends are cut away and its
   stations sit outside the strip. Add `CenterlineOutside` to
   `checkMeshClean` and the catalog clean tests, list what else it flags
   in the commit body, and commit this step on its own (`Report
   centerline points outside the tube`), with the two cases in a
   `[!mayfail]` test `keeps the centerline inside a junction arm`.
2. **Axis cut.** `trimArm` first finds `axisKept`: walking the arm's
   ring centers from the far end toward the junction, the index after
   the last center outside the grown through (`RingTube::inside` with
   `kJunctionTolerance`, within `solids.isNear`). `keptRings` is at
   least `axisKept`; a column's earlier entry lowers it only within the
   last ring spacing before `axisKept` (the `isAtMouth` sliver rule
   stays).
3. **Columns already inside at the kept ring.** A whole column whose
   vertex on the kept end ring is inside the through takes its mouth
   point from `ontoThroughWall(vertex)` with the surface parameter of
   that hit; `betweenColumns` does the same from the kept ring on. Those
   strips are short and lie inside the through.
4. **Entry search starts at the kept ring.** `findEntry` walks the
   polyline from `axisKept − 1` to the junction, so a wall grazing the
   through far up the arm is ignored.
5. `leaveOutArm` keeps its meaning: an arm is dropped only when no ring
   center is outside the through.

**Done when.**
- Step 1 lands first: the rule exists, gallery 21 and 22 fail it under
  `[!mayfail]`, every existing catalog case passes it (or the commit
  body lists which do not and why).
- After steps 2–5: 21 and 22 pass without `[!mayfail]`; the arm of 21
  keeps rings past both bends and its mouth points lie within 1.5 ·
  through spacing of the room wall near J; the arm of 22 keeps its
  elbow rings and its corner station is inside.
- Symmetric T, gallery 5, 11 and Phake Cave: mouth positions unchanged
  within 1e-3 (the axis cut equals today's cut whenever the first column
  entry is within one spacing of the axis entry).
- Every other `[PassageTube]` test passes.

**Commit subjects.** Step 1: `Report centerline points outside the
tube`. Steps 2–5: `Cut junction arms where their axis enters the
through`.

---

### T07r — Keep a corner station inside its elbow ring

**Goal.** The middle elbow ring sits `inset · tan(φ/2)` off its station
toward the bend's inside (`elbowSamples`, `stationOffsetMeters`) but
fits the station's walls as radii from the ring center
(`fitLrudRing`, `lrudAcross`), so a wall on the bend's outside read
shorter than that offset leaves the station, and the centerline corner,
outside the tube. A zero reading (`kMinRadiusMeters` 0.05) does it
directly; a passage with no readings borrows (`buildRings`,
`nearestFitted`, `borrowedFit`) a neighbor trip's zero and does it two
legs away. Two stations of a real project show it (0.07 m and 0.10 m
outside; the project appears nowhere in source or tests).

**Read.** Research §3.2 (rings), §3.3.2 (bends). Code: `elbowSamples`,
`lrudAcross`, `fitLrudRing`, `borrowedFit`, `buildRings`,
`blendElbowRings`, `fitSharpBends` in `cwPassageTubeBuilder.cpp`; the
T07j elbow-ring tests in `test_cwPassageTubeBuilder.cpp`; the
`CenterlineOutside` rule from T07q.

**Modify.** `cwPassageTubeCases.cpp`, `test_cwPassageTubeCases.cpp`
first; then `cwPassageTubeBuilder.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.**

1. **Reproduce before fixing.** Two catalog cases, `[!mayfail]` on
   `CenterlineOutside` until step 2:
   - `23. Sharp bend at a station reading zero on the outside wall`:
     three stations, legs 5 m and 1.7 m, 56° turn in 3D (the second leg
     climbing 51°), corner LRUD `{0, 1.07, 0.91, 0.15}` with the zero on
     the bend's outside.
   - `24. Sharp bend in an unread passage next to a zero reading`: four
     stations, LRUD only on the first (`{0, 0.6, 0.6, 1.2}`), a 72° turn
     two legs on.
   Commit this step on its own (`Reproduce corner stations outside their
   elbow ring`).
2. **Translate the readings.** `ChainSample` carries `stationOffset`
   (station − sample position) for the middle elbow sample. After
   `lrudAcross`, each wall `w` with in-plane direction `d` (the ring's
   ±right, ±up) becomes `w + dot(offset, d)`, clamped to
   `kMinRadiusMeters`: the wall stays where it was measured and the
   station is inside by at least its reading on each side. The same
   applies to a borrowed fit on an elbow middle ring.
3. **Borrow from the same passage.** `nearestFitted` prefers a fitted
   sample on the same or an adjacent leg over a nearer one across a trip
   boundary (skip samples whose record trip differs when a same-trip
   fitted sample exists within two legs).
4. `fitSharpBends`' `BendClamped` warning then reports only real clamps.

**Done when.**
- Step 1 lands first with 23 and 24 failing `CenterlineOutside` under
  `[!mayfail]`.
- After steps 2–4: 23 and 24 pass without `[!mayfail]`; the middle
  elbow ring's radius toward the station exceeds its in-plane offset;
  `rounds a sharp bend with elbow rings` (T07j) still passes and wall
  points at the corner station stay on the mesh within `kWallTolerance`.
- Every other `[PassageTube]` test passes.

**Commit subjects.** Step 1: `Reproduce corner stations outside their
elbow ring`. Steps 2–4: `Keep a corner station inside its elbow ring`.

---

### T07v — Keep a bend's rings within the station's readings

**Goal.** T07r fits the middle elbow ring to the *union* of the walls'
ellipse around the ring center and the same ellipse around the station
(`LrudSection::reach`). The union keeps the station inside, but at a
sharp bend the ring center sits `inset · tan(φ/2)` off the station, so
the ring reaches that far past the readings on the bend's inside and
the tube bulges into a knob around the corner. Gallery case 23 shows
it (`cavewhere-tube-gallery --case "23." --grab <dir>`), and Phake
Cave shows it at a3 (`{1.5, 3, 4.5, 1}`, a 90° plan turn), a2 and a1
(`{0, 5, 2, 1.3}`, the dead end: its plumb end ring and the first
elbow-free ring disagree by the 0/5 asymmetry). No checker rule reports
a surface wider than the readings, so the T07r review's design call
("union vs translation") was settled by tests that could not see the
difference.

**Read.** Research §3.2 (rings), §3.3.2 (bends), §3.6 "Corner station
outside its elbow ring". Code: `LrudSection`, `enclosingCenter`,
`reachFromPoint`, `fitLrudRing`, `reachPastStation`, `insideWallShift`,
`fitSharpBends` in `cwPassageTubeBuilder.cpp`; `ringIssues` and
`wallsOnSurface` in `cwPassageTubeMeshCheck.cpp`; T07r's attempts at
`/Users/cave/.claude/jobs/ab1fa6ed/tmp/T07r-fix-attempt-*.cpp` (notes
only; those files are not part of the repository).

**Modify.** `cwPassageTubeMeshCheck.h/.cpp`, `test_cwPassageTubeMeshCheck.cpp`,
`test_cwPassageTubeCases.cpp` first; then `cwPassageTubeBuilder.cpp`,
`test_cwPassageTubeBuilder.cpp`.

**Spec.**

1. **Reproduce before fixing.** Checker rule `WallBeyondReading` in
   `ringIssues`: for every ring fitted to a station's own or borrowed
   readings (grown junction rings excluded), each vertex projected
   into the frame of the nearer leg at that station lies inside the
   station's quadrant ellipse (the `lrudAcross` section for that leg)
   by at most `kWallTolerance` beyond it. One issue per ring, at the
   farthest vertex, with the overshoot in meters. The rule flags
   gallery 23, Phake Cave a1 and a3, and whichever random seeds share
   the shape; list them in the commit body. Gallery 23 moves to a
   `[!mayfail]` test for this rule; Phake Cave a1/a3 are accepted in
   `kPhakeCaveIssues` under a comment naming this task. Commit on its
   own (`Report tube walls beyond the station's readings`).

   The T07z probe (Oct 13, `tmp/section-probe.log`) shows why the rule
   has to be measured from the station: `Section::stationRadii` is the
   readings' ellipse carried to the ring *center*, so T07z's whisker
   (`radii[k] − stationRadii[k]`) is zero on the bend's inside even
   though that is where the knob is. At a3 the middle ring sits 1.67 m
   inset; its right (inside) wall is 3.25 m from the center, 4.92 m
   from the station where the reading is 3.0; its up wall 4.67 from
   the center against 4.5 read at the station. At a2 (0.39 m inset,
   L 2.5 read) the left wall lands 3.0 m from the station. The rule
   measures each vertex against the ellipse anchored at the station,
   and the gallery's "Overshoot" layer switches to the checker's
   issues (the T07z report lines go).

   The same probe shows a4's elbow is 13 rings whose left radius
   climbs 2.2 → 6.96 m: `widestReadings` merges a4's two records
   (`{0, 2, 2, 1.7}` on a3–a5, `{5, 0, 3, 23}` on a4–a6) into
   L 5.93. Which record a corner ring should take is a separate
   question; note it in the rule's commit body and leave it.
2. **Fit the bend without the union.** The middle elbow ring's section
   is the station's ellipse seen from the ring center
   (`reachFromPoint(m_wallsAroundStation, −m_station, angle)` alone,
   with `enclosingCenter` growing it only as far as holds the center by
   `kMinRadiusMeters`); with T07s's station ring in place the skin
   follows the legs, so the chord problem that made T07r choose the
   union no longer applies. Where T07r's attempts saw slivers (case 23,
   random seeds 12 and 33) the fold rule from T07j pulls the offending
   vertices onto the previous ring's plane; where the pit lip collapsed
   (`keeps the inner wall of a sharp bend clean`), keep the lip's
   measured wall by reading the inside walls from the station, which is
   what `insideWallShift` already measures. A dead end's plumb end ring
   (T07h) takes the same rule, so a1's end ring stays inside its
   readings.

**Done when.** Step 1 lands first; after step 2 gallery 23, Phake Cave
and the T07r cases pass `WallBeyondReading` and `CenterlineOutside`
with the a1/a3 rows removed; the T07j, T07r and T07s elbow tests pass;
every other `[PassageTube]` test passes. The gallery grabs of case 23
before and after go in the commit body's description (no knob at the
corner).

**Commit subjects.** Step 1: `Report tube walls beyond the station's
readings`. Step 2: `Fit a bend's rings within the station's readings`.

---

### T07w — Open a CaveWhere project in the gallery

**Goal.** The gallery is the debugging view: it draws the checker's
issues as markers, frames junctions and arms, and grabs the four
presets. It only knows the synthetic catalog and random surveys, so a
problem seen in a real project (Phake Cave a9, a1, a3) has to be
re-derived by hand before anyone can look at it with markers. The
gallery opens a `.cw` / `.cwproj` file as one more row, built with the
same builder and checked by the same rules; CaveWhere itself stays as
it is (no markers in the app). Projects stay out of the repository:
this is a loader, not a fixture (section 0's ground rule).

**Read.** `tube-gallery/main.cpp` (options), `cwPassageTubeGalleryModel`
(rows, `build`, `locateJunction`, `issuePositions`, `framingBox`),
`cwProject::loadOrConvert` and `waitLoadToFinish`, `cwLinePlotManager`
(station positions), `cwPassageTubeManager.cpp` (how the app gathers
`cwCavingRegionData` and `positionsByCave` for the builder), the scratch
pattern in `test_cwGeometryItersecter_phakeCave.cpp` for loading a
project in a test.

**Modify.** `tube-gallery/main.cpp`, `cwPassageTubeGalleryModel.h/.cpp`,
the gallery QML (row label, station field), `test_cwPassageTubeGalleryModel.cpp`
(a project written by the test into a `QTemporaryDir`, never a checked-in
one).

**Spec.**

- `--project <file>` (repeatable) and a File menu "Open project…" in the
  gallery. The file is copied into a `QTemporaryDir` and loaded from the
  copy with `cwProject::loadOrConvert`, so a legacy `.cw` conversion or a
  Git-backed `.cwproj` never writes to the user's file. Station positions
  come from `cwLinePlotManager` on the loaded region (`waitToFinish`), as
  the app does.
- A row per cave in the project, named `<project>: <cave>`, tagged
  `project`, with the builder's default sample spacing (overridable with
  `--spacing <m>`). `Row` holds either a `Cases::Case` or a loaded
  survey (`cwCavingRegionData` plus `positionsByCave`); `build` takes the
  survey form, and `Cases::toSurvey` feeds the catalog rows into it.
- `--station <name>`: frame the camera on that station (the framing box
  is the station's legs plus their readings), and the along-arm preset
  looks down its longest leg. Issue texts and the report gain the
  nearest station's name, as the Phake Cave test prints it
  (`nearestStation`), so a marker reads "Crease at a10 …".
- Grabs work as for catalog rows: `--project P.cwproj --station a9 --grab
  <dir>` writes the four presets of that station.
- The project is read once; the gallery keeps no reference to the path
  in settings, the model or the grabs' file names beyond the row label.

**Done when.** A test writes a small project to a `QTemporaryDir`
(three stations, one bend), opens it through the model, sees one row
per cave, a mesh, and the same report `cwPassageTubeMeshCheck::check`
gives for the equivalent catalog survey; `--station` frames the named
station (framing box contains it). Manual: `cavewhere-tube-gallery
--project "<Phake Cave 3000.cwproj>" --station a9` shows the a9 leak
marker from T07s step 1. Both suites pass.

**Commit subject.** `Open a CaveWhere project in the tube gallery`

---

### T07y — Draw the builder's rings in the gallery

**Goal.** Every tube problem so far (the T07r knob, the a9 chord, the
case 25 frame roll) was diagnosed by printing ring centers and frames
from a scratch test and sketching them by hand. The gallery already has
`cwPassageTubeBuilder::Debug` for the current row — chains, legs, and
every `Ring` with its `center`, `frame`, `radii`, `binAngles`,
`vertices`, `measured`, `isElbow`, `isGrown` — so it can draw the rings
themselves. With the rings on screen the decision for T07s step 2
(frame roll vs. sagitta guard) and the T07v bend fit can be made by
looking.

**Read.** `cwPassageTubeGalleryModel` (`build`, `m_linePlot`,
`issuePositions`), `cwRenderLinePlot` (point pairs; `setGeometry`,
splay ranges select the second color), `TubeGalleryWindow.qml` (the
checkboxes around line 407 and the marker `Repeater`s around line 455),
`cwTubeGalleryGrabber`, `cwPassageTubeBuilder.h` (`Ring`, `Frame`,
`Chain`).

**Modify.** `cwPassageTubeGalleryModel.h/.cpp`, `TubeGalleryWindow.qml`,
`tube-gallery/main.cpp`, `cwRenderLinePlot` only if a third color is
needed, `test_cwPassageTubeGalleryModel.cpp`.

**Spec.**

- A second `cwRenderLinePlot` on the gallery scene, `ringPlot`, fed by
  the model's `ringGeometry()`: for every ring of every chain, the loop
  through its `vertices` (closed), and from its `center` a short frame
  cross: `right` and `up` each drawn to `kFrameAxisMeters = 0.5`, with
  `up` as the "splay" type so it reads in the second color and the roll
  of the frame is visible at a glance.
- Ring kinds are told apart by how they are drawn, within the two
  colors the line plot has: station and sample rings as plain loops;
  elbow rings (`isElbow`) with their frame cross doubled in length;
  grown rings (`isGrown`) drawn as a dashed loop (every other edge).
  If that is too subtle, add a third color to `cwRenderLinePlot`
  (`segmentTypes` already carries a type per segment) rather than a new
  render object.
- Checkboxes "Rings" and "Frames" beside the existing "Issues" and
  "Warnings" toggles; `--rings` on the command line turns both on for
  grabs, so a `--grab` sheet can include them.
- Hovering a ring (or the nearest ring to the issue marker clicked)
  shows a line in the report panel: chain index, ring index, leg,
  distance to its station, `frame.up` and `inclinationWeight`, min/max
  radius, elbow/grown flags — the `ringLine` the scratch probes print.
- `reportText` gains one line per elbow: the station, the turn angle,
  the middle ring's offset and the roll between its frame and its
  neighbors' (`acos(dot(up_i, up_{i±1}))`), so frame roll is a number
  in the report, not only a picture.

**Done when.** A model test builds a catalog case (the T07j elbow) and
checks `ringGeometry()` has one closed loop per ring with the expected
vertex count, that elbow rings carry the long frame cross, and that the
report lists the elbow with its roll; `cavewhere-tube-gallery --case
"25." --rings --grab <dir>` shows the rings at the foot of the shaft;
both suites pass.

**Commit subject.** `Draw the passage tube rings in the gallery`

---

### T07z — Draw the readings each ring was fitted to

**Goal.** T07y shows the rings, but at a bulging station (Phake Cave
a2 and a3, gallery 23) the ring alone cannot say *why* it is that
size: whether the readings were read wide, `lrudAcross` rotated them
into the ring's frame, `widestReadings` took the wider of two
records, `filledReadings` filled a missing side, or `LrudSection::reach`
unioned the ellipse around the ring center with the one around the
station. `fitLrudRing` computes every one of those and keeps only
`radii`; `RingFit::stationRadii` is dropped before the `Ring` is built.
The gallery should draw the readings as surveyed, the section the ring
was fitted to, and the overshoot between that section and the ring, so
the T07v fix can be designed by looking at a2/a3 instead of guessing.

**Read.** `cwPassageTubeBuilder.h` (`Leg`: `fromLrud`, `toLrud`,
`fromPassageLrud`, `fromFacing`; `Ring`, `Frame`), `fitLrudRing`,
`lrudAcross`, `widestReadings`, `filledReadings`, `LrudSection`,
`enclosingCenter`, `reachFromPoint`, `RingFit` and `ChainSample`
(`records`, `stationOffset`) in `cwPassageTubeBuilder.cpp`; the T07y
gallery code (`ringGeometry`, `ringLine`, `elbowLines`, the "Rings"
and "Frames" checkboxes and the hover handler in
`TubeGalleryWindow.qml`); T07v's `WallBeyondReading` rule, which this
task previews in the report.

**Modify.** `cwPassageTubeBuilder.h/.cpp` (diagnostics only — no
change to the mesh), `cwPassageTubeGalleryModel.h/.cpp`,
`TubeGalleryWindow.qml`, `tube-gallery/main.cpp`,
`test_cwPassageTubeBuilder.cpp`, `test_cwPassageTubeGalleryModel.cpp`.

**Spec.**

1. **Keep the section on the ring.** `Ring` gains
   `std::optional<Section> section`, set for every ring `fitLrudRing`
   fitted from readings (unset for steep rings, borrowed rings, rings
   left at zero and grown rings):
   ```cpp
   struct Section {
       // The quadrant readings the ring was fitted to, in the ring's
       // (right, up) plane: lrudAcross → widestReadings → filledReadings.
       cwStationLrud readings;
       // The readings' quadrant ellipse around the station, per bin —
       // RingFit::stationRadii. Equal to radii except on an elbow.
       QVector<double> stationRadii;
       // The station the readings sit around; the ring center except
       // for an elbow's middle ring, which sits inset from it.
       QVector3D station;
   };
   ```
   `build()` output is byte-identical before and after (the mesh test
   on gallery 23 and case 25 checks the vertex buffers match).

2. **Three layers in the gallery, each its own checkbox and CLI flag,
   drawn through `ringPlot` (add a third `cwRenderLinePlot` only if
   the two colors run out):**
   - **"Readings"** (`--readings`): the survey's LRUD as spokes from
     each station in the world, from `Debug::legs`: left and right
     perpendicular to `fromFacing`/`toFacing` (right clockwise of the
     facing), up and down along world z, each as long as the reading.
     The passage LRUD (`fromPassageLrud`, shaft depths replaced) in
     the second color where it differs from the raw reading. This is
     what the surveyor measured, before any frame.
   - **"Sections"** (`--sections`): for every ring with a `section`,
     the loop of `stationRadii` at `binAngles` around
     `section.station` in the ring's frame (the readings' ellipse the
     ring should stay inside), and the segment
     `section.station → center` on elbow rings (the inset the union
     reaches past).
   - **"Overshoot"** (`--overshoot`): for every bin whose
     `radii[k] − stationRadii[k]` exceeds `kWallTolerance`
     (`cwPassageTubeMeshCheck`), a whisker from the section loop to
     the ring vertex, in the second color. At a2/a3 this is the knob,
     drawn as the lines that make it.
   `--rings` keeps turning on only rings and frames; grabs take any
   combination.

3. **Hover and report.** The T07y `ringLine` gains the section's
   L/R/U/D, the station it sits around, and the largest overshoot
   with its bin angle. `reportText` gains one line per ring whose
   largest overshoot exceeds `kWallTolerance`: `ring i at <station>
   (chain c): reaches 0.42 m past the readings at 135°`. These lines
   are the gallery's preview of T07v's `WallBeyondReading`; when T07v
   lands, the report uses the checker's issues instead and this
   code goes.

4. **Station framing.** `--station` (T07w) with `--sections` frames
   the station's rings and their sections, so
   `cavewhere-tube-gallery --project <copy of Phake Cave> --station a3
   --rings --readings --sections --overshoot --grab <dir>` is the a3
   sheet.

**Done when.** A builder test on gallery 23 checks that the elbow's
middle ring has a `section` whose `station` is the corner station,
whose `stationRadii` are at most `radii` in every bin, and whose
largest overshoot is positive (the knob), while a straight sample ring
has `stationRadii == radii`; the mesh is unchanged. A model test checks
the readings layer has four spokes per positioned station of the
survey, the sections layer has one loop per sectioned ring, and the
report names the gallery 23 corner with its overshoot. Grabs of gallery
23 and Phake Cave a3 with all three layers; both suites pass.

**Commit subject.** `Draw the readings each ring was fitted to`

---

### T07aa — One color per overlay layer in the gallery

**Goal.** The gallery's ring plot draws seven kinds of line (ring loops,
frame right and up axes, station readings, passage readings, section
loops and insets, overshoot whiskers) with the two colors
`cwRenderLinePlot` has: centerline red and splay pink. With Rings,
Sections, Readings and Overshoot on at once nothing can be told apart.
Each layer gets its own color: Rings, Frames, Readings, Sections,
Overshoot.

**Read.** Code: `cwRenderLinePlot.h/.cpp` (`Data::segmentTypes`,
`kCenterlineSegment`, `kSplaySegment`, `setGeometry`),
`cwRHILinePlot.cpp` (the per-instance type attribute),
`cavewherelib/shaders/LinePlot.vert` and `LinePlot.frag` (`iType`,
`kCenterlineColor`, `kSplayColor`), `RingGeometry` and `pushToScene` /
`applyRingVisibility` in `cwPassageTubeGalleryModel.cpp`, the layer
checkboxes in `tube-gallery/qml/TubeGalleryWindow.qml`.

**Modify.** `cwRenderLinePlot.h/.cpp`, `LinePlot.vert`, `LinePlot.frag`,
`cwPassageTubeGalleryModel.h/.cpp`, `TubeGalleryWindow.qml`,
`test_cwRenderLinePlot.cpp` (create if there is none),
`test_cwPassageTubeGalleryModel.cpp`.

**Spec.**

1. **Segment types are a palette index.** `cwRenderLinePlot` keeps
   `kCenterlineSegment = 0` and `kSplaySegment = 1` and adds
   `kRingSegment`, `kFrameSegment`, `kReadingSegment`,
   `kSectionSegment`, `kOvershootSegment` (2–6) and
   `kSegmentTypeCount`. `setGeometry` takes a second form,
   `setGeometry(points, QVector<TypedRange>)` with
   `TypedRange {VertexRange range; quint32 type;}`, filling
   `segmentTypes` per range (the existing splay-range form becomes a
   call to it). Any type at or past `kSegmentTypeCount` is a
   `Q_ASSERT`. The vertex shader keeps the width rule (centerline 2 px,
   everything else 1 px); the fragment shader indexes a
   `const vec4 kTypeColors[7]` by `vType` instead of branching on zero.
   Colors: centerline and splay unchanged; rings a light gray, frames
   blue, readings green, sections yellow, overshoot magenta, all opaque,
   chosen to read against the tube's shaded gray and against each other
   in the translucent view.
2. **One table.** `cwRenderLinePlot::segmentColor(quint32 type)` returns
   the same palette as a `QColor` from a `k`-constant table in the
   `.cpp`, and a test reads `LinePlot.frag` from the Qt resource and
   checks each shader constant against `segmentColor`, so the two copies
   cannot drift. CaveWhere's own line plot keeps types 0 and 1 and looks
   the same.
3. **Gallery.** `pushToScene` assigns the ring plot's ranges their
   types: loops → ring, rights and ups → frame, readings and
   passageReadings → reading, sectionLoops and sectionInsets → section,
   overshoots → overshoot. The QML layer checkboxes show their layer's
   color as a small swatch (a `QQ.Rectangle` beside the label) read from
   `segmentColor` through a `Q_INVOKABLE QColor layerColor(const
   QString& layer)` on the model, so the QML has no color literals.

**Done when.**
- A `cwRenderLinePlot` test builds geometry with three typed ranges and
  reads back `segmentTypes` per segment; an out-of-range type asserts
  (test under `Q_ASSERT` only where the test harness supports it,
  otherwise skip that check).
- The palette test passes against `LinePlot.frag`.
- A gallery-model test checks each layer's range has the type named
  above after `pushToScene` on a catalog case.
- A grab of case 23 with `--rings --sections --readings --overshoot
  --translucent` goes in the commit body's description; every layer is
  a different color.
- Every `[PassageTube]` test passes; `cavewhere-qml-test` for the
  gallery window if it has one.

**Commit subject.** `Color each gallery overlay layer on its own`.

---

### T07ac — Fit the section between two stations from both stations' readings

**Goal.** Between two stations the tube should run from the first
station's section to the second's, each wall interpolated by distance
along the leg. It does for the ceiling and floor; for the side walls
it does not, and at every sharp corner the tube bulges at the station
and necks down just past it. Phake Cave a2 → a3 shows it on the left
wall, measured as the tube wall's distance from the leg's axis after
T07v (`29dbcab1`, probe of Oct 14): 2.13 m at the last ring before a2
(from the a1 → a2 axis) → **2.31** at a2's elbow ring → 2.00 → **1.82**
(1.5 m past a2) → 1.75 → 1.63 → 1.50 → 1.39 at 5 m → 2.06 at the elbow
blend before a3, where a2 reads `L = 2.5` and a3 `L = 1.5`; the section
a2's elbow ring was fitted to has `L = 2.64`. Interpolating the two
stations' readings would put the wall at 2.5 at a2 and 1.95 at 5 m.
The right wall does the reverse: a3 reads `R = 3`, the leg rings reach
1.19 at 5 m toward a3's 1.79, and a3's elbow ring is fitted to
`R = 3.25`.

How the section between stations is computed today
(`cwPassageTubeBuilder.cpp`):

1. `chainSamples` places a sample every `sampleSpacingMeters` along the
   leg with `lerpRecord(leg.startRing, leg.endRing, t)`: left, right,
   up, down and the *facing* are each linear in `t` between the two
   stations' records. The facing is T07c's chord direction through the
   station's neighbors in chunk order (`chunkFacings`), so at a2 it is
   the a1 → a3 chord `(0.46, 0.89)`, 21° off the a1 → a2 leg and 35°
   off the a2 → a3 leg; at a3 (a 90° plan turn) the chord is about 45°
   off both legs.
2. `fitLrudRing` turns each sample's record into readings across the
   ring's frame with `lrudAcross`. Without a miter (every ring except
   an elbow's middle ring) a side reading becomes
   `|L · right(facing) · frame.right|`, the reading *projected* onto
   the ring's right axis: a2's `L = 2.5` counts as 2.33 on the a1 → a2
   leg and 2.04 on the a2 → a3 leg; a3's `L = 1.5` counts as 0.89 and
   its `R = 3` as 1.79 on the a2 → a3 leg. Up and down are projected
   the same way onto `frame.up`, which on a sloped leg is the
   foreshortening T07h added on purpose (a plumb reading above a 17°
   leg meets the ring at `U cos 17°`). So the leg interpolates, but
   between the *projected* values: along a2 → a3 the left wall runs
   2.04 → 0.89 instead of 2.5 → 1.5.
3. The elbow's middle ring (`elbowSamples`, the station's own record
   with `isElbow`) gets a `Miter`: `lrudAcross` slides the wall point
   along the leg into the ring's plane and keeps its stretched length
   (up to `kMaxMiterStretch` 3×), so a2's 2.5 becomes 2.61 and the
   ring is wider than the reading. The two elbow rings beside it are
   blends (`blendElbowRings`) between the station ring and the first
   leg ring on either side; `LrudSection::binAngle` then spreads the
   bins by eccentric anomaly over the section's four axis reaches, and
   `reachFromPoint` measures the station's ellipse from the ring's
   inset center (T07v).
4. The bulge is the difference between 2 and 3: the leg rings take the
   readings shrunk by the cosine of the facing's angle to the leg, the
   station ring takes them stretched by the miter. At a 90° corner the
   leg rings lose 30% of every side reading while the corner ring keeps
   it all.

Case 23 (a 90° bend of equal sections) shows the same neck on both
legs; the sweep's bends do too, which is why `WallMissed` is quiet
(the ring sits inside the reading, which no rule reports) and
`WallBeyondReading` is quiet (T07v fit the corner ring to the station's
ellipse).

**Decision (made Oct 14).** Side readings are taken at **face value**:
`L` is the half-width of the passage at the station, perpendicular to
the passage, as every survey convention defines it and as the surveyor
measured it, so a leg runs from one station's `L` to the next
station's `L`. The side a reading lands on is set by the **chain's
direction**, not the chord facing: looking along the chain (from a2 to
a3 on that leg), left is `−frame.right` and right is `+frame.right`
(`right = tangent × up`). A record whose facing runs against the chain
(`facing · tangent < 0`: the chunk was surveyed the other way) has its
left and right swapped; a facing square to the chain keeps the record's
own sides. The chord facing is otherwise unused by the fit.

**Read.** Research §3.2.1, §3.3.2, §3.6 rows "Sloped walls off the
ring", "Corner station outside its elbow ring", "Knob at a sharp bend".
Code: `lerpRecord`, `chainSamples` (the `appendAlongLeg` lambda),
`elbowSamples`, `lrudAcross`, `widestReadings`, `filledReadings`,
`LrudSection`, `fitLrudRing`, `blendElbowRings`, `fitSharpBends`,
`insideWallShift` in `cwPassageTubeBuilder.cpp`; `wallReaches` in
`cwPassageTubeMeshCheck.cpp`; the chain cases (20–26) in
`cwPassageTubeCases.cpp` and their tests; the probe numbers above come
from `cwPassageTubeGalleryModel::ringLine` on Phake Cave (gallery
`--project … --station a2 --rings --sections --translucent`).

**Modify.** `cwPassageTubeCases.cpp`, `test_cwPassageTubeCases.cpp`,
`test_cwPassageTubeBuilder.cpp` first; then `cwPassageTubeBuilder.cpp`.

**Spec.**

1. **Reproduce before fixing.** Catalog case `30. S-bend from a small
   to a large to a medium section`: four stations on legs of 10 m in
   the plan, bearings 0°, 60°, 0° (an S: +60° at s1, −60° at s2), level,
   with sections s0 `{0.5, 0.5, 0.5, 0.5}`, s1 `{3, 3, 2, 1}`, s2
   `{1.5, 1.5, 1.5, 0.8}`, s3 `{1.5, 1.5, 1.5, 0.8}`. Add it to the
   chain cases (bump the catalog count test). Builder test
   `cwPassageTubeBuilder runs each wall from one station's reading to
   the next` over case 30 and Phake Cave's a1 → a4 chain: for every
   ring of the chain, each of the four walls' reach along the ring's
   ±right and ±up (`polarRadius` at the axis angles) lies within
   `kWallTolerance` of the two bounding stations' readings
   interpolated by the ring's distance along the leg, where an elbow
   ring counts as at its station and up/down on a sloped leg compare
   against the foreshortened `U cos θ`. `[!mayfail]` until step 2. Grab
   case 30 and Phake a2 with `--rings --sections --translucent` for the
   commit body. Commit on its own (`Reproduce the neck past a corner
   station`).
2. **Face value along the leg.** `lrudAcross` without a miter returns
   the side readings as radii along `±frame.right` with no cosine: left
   at `−frame.right`, right at `+frame.right`, swapped when
   `facing · frame.tangent < 0`; up and down keep the projection onto
   `frame.up`. The elbow's middle ring
   takes the station's readings the same way (no miter slide or
   stretch in `lrudAcross`; `fitSharpBends` and `insideWallShift` keep
   their miter for the inside-wall reshaping), then `reachFromPoint`
   measures that ellipse from the inset center as T07v does. A corner
   gentler than one elbow step (a single mitered station ring) takes
   face value too; the stretch it loses is under 1%. `kMaxMiterStretch`
   goes if nothing reads it. Remove `[!mayfail]` from the step 1 test.
3. **Check the fallout.** `WallBeyondReading` stays clean on the
   catalog, Phake Cave and the random seeds (wider leg rings at a
   corner reach exactly the station's ellipse); the T07j fold rule and
   the T07d0 inner-bend rules may fire on more sharp bends now that the
   inside wall is wider — list the catalog and sweep cases whose issue
   counts change in the commit body, and fix regressions in the builder
   rather than accepting them.

**Done when.**
- Step 1 lands first with the test failing under `[!mayfail]` and the
  grabs in the commit body.
- After step 2 the test passes as a hard test on case 30 and on Phake
  Cave a1 → a4; Phake Cave a2 → a3's left wall decreases monotonically
  from 2.5 to 1.5 (assert it in the same test); `WallBeyondReading`
  and `CenterlineOutside` are clean on the catalog, Phake Cave and the
  random seeds; the T07j, T07r, T07s and T07v elbow tests pass; every
  other `[PassageTube]` test passes.
- Before/after grabs of case 30 and Phake a2 in the commit body: no
  bulge at the corner, the wall runs straight from one section to the
  next.

**Commit subjects.** Step 1: `Reproduce the neck past a corner
station`. Step 2: `Fit each leg's section from both stations' readings`.

---

---

### T07ad — A corner's rings sample the two legs' walls

**Goal.** After T07ac (`70c7a4f0`) every leg ring runs its walls
linearly from one station's reading to the next, but the rings of a
sharp corner's elbow still put a bulge, a notch or a shelf at the
corner. Probe of Oct 14 after T07ac (`tmp/probe-after-T07ac.log`,
`plans/TUBE_CORNER_FOLLOWUPS.html` fig. 1–2), wall distances
perpendicular to the leg's centerline:

- Case 30 at s1 (60° turn, reading 3 m each side, the inset rings
  2.52 m before and 2.71 m after): the outside wall runs 2.52 →
  **2.82** (first elbow ring) → 2.60 at the middle ring → 2.71, a
  0.3 m bulge then a cut corner; the inside wall runs 2.52 → 2.52 →
  2.60 → **2.63 at a center 0.05 m inside the second leg's axis** →
  2.71, a 0.1 m shelf half a meter long.
- Phake a2 (57° turn, inside reading `L = 2.5`): the inside wall
  runs 2.24 (inset) → 2.20 → **2.20** (middle ring) → 2.26 → 2.33
  (inset) where the legs interpolate toward 2.5: a 0.1–0.2 m dip
  where the wall should peak, and the 2.5 m section spoke next to it
  looks wider than the rings.

Two causes, both in how the elbow rings are fitted:

1. **The middle ring reads the station along the bisector.** It lies
   in the bisector plane and `lrudAcross` puts `L` at face value along
   its `−right`, i.e. `L` meters from the station *along the
   bisector*, which is only `L cos(halfTurn)` from either leg's
   centerline (2.6 m for 3 m at 60°; 2.2 m for 2.5 m at a2; at a3's
   90° turn `R = 3` becomes 2.12). The legs interpolate toward `L`
   perpendicular, so they and the corner disagree by
   `L (1 − cos halfTurn)`.
2. **`blendElbowRings` lerps radii between rings whose centers sit in
   different places.** The rings beside the middle one take
   `lerp(inset ring radius, middle ring radius)` per bin, but the
   middle ring's radii are measured from a center `stationOffset`
   (0.51 m at s1, 0.39 m at a2, 1.67 m at a3, 1.03 m at a4) inside
   the corner while the inset ring's center is on the centerline; the
   blended ring's own center is somewhere between. Outside bins get a
   radius that is too long (the bulge), inside bins one that is too
   short (the shelf).

**Decision (made Oct 15).** The station's reading is a width of the
legs, as T07ac made it: each leg's walls run at their interpolated
width up to the station and hold the station's reading past it. A
corner's wall surface is the union of the two leg tubes, **each run
on past the station as far as it stays inside the other leg's tube**
(the other leg's section extruded along its axis both ways). On the
outside the two walls meet in a miter (3.46 m from s1 for 3 m at
60°); on the inside they meet in a square corner — the lead-in floor
of case 25 runs level to the shaft's wall, and the shaft's foot has a
flat floor at f2 (the exit tube's floor, 0.05 m under its axis,
carried back under the shaft, so it follows the exit's dip). The
bisector-plane cut, which leaves a chamfer across the inside corner
(1 m at f1, 1.74 m at f2; `plans/TUBE_SHAFT_PROFILE.html`), was
turned down. Every elbow ring, the middle one included, reaches from
its own center to that envelope; the reading enters only through the
legs. `kMaxMiterStretch` and the miter-slide in `lrudAcross` stay
gone.

**Read.** Research §3.1 (frame), §3.2 (cross-section sources), §3.3
(joining rings, sharp bends), §3.6 rows *Neck past a corner station*
and *Corner rings off the legs' walls*; `chainSamples` (`elbowSamples`,
`stationOffset`), `lrudAcross`, `LrudSection`, `reachFromPoint`,
`fitLrudRing`, `blendElbowRings`, `fitSharpBends`, `reshapeInside` and
`insideWallShift` in `cwPassageTubeBuilder.cpp`; the T07ac test
`cwPassageTubeBuilder runs each wall from one station's reading to the
next` and the T07j/T07r/T07s/T07v elbow tests in
`test_cwPassageTubeBuilder.cpp`; case 30 and `phakeCave()` in
`cwPassageTubeCases.cpp`.

**Modify.** `test_cwPassageTubeBuilder.cpp` first, then
`cwPassageTubeBuilder.cpp`.

**Spec.**

1. **Reproduce.** Extend the T07ac test (or add `cwPassageTubeBuilder
   keeps a corner's walls on the legs' walls`) so that *every* ring of
   the chain is checked, elbow rings included, against the envelope of
   step 2: for an elbow ring, each measured bin's vertex must lie
   within `kWallTolerance` of the nearer leg's interpolated wall (the
   leg whose extended axis the vertex projects onto with the smaller
   perpendicular distance; both legs' widths hold the station's reading
   beyond the station). Over case 30 (s1 and s2) and Phake a1 → a4 (a2
   and a3). Assert the three numbers above as named regressions: s1's
   outside wall never exceeds 2.75 before the station, a2's inside wall
   reaches 2.5 ± `kWallTolerance` at the middle ring, s1's inside wall
   is monotone between the inset rings. `[!mayfail]` until step 2.
2. **The corner envelope.** A `CornerWalls` (name free) built per sharp
   corner from the two `WalkedLeg`s: for a direction from a point, the
   distance to the boundary of the union of the two leg tubes, each
   tube being the leg's quadrant ellipse (`lerpRecord` along the leg,
   clamped to the station's record beyond it) extruded along that
   leg's axis: on the leg's own side of the station without limit, past
   the station only where the point also lies inside the other leg's
   prism (its section extruded along its axis without limit). For a
   ring with center `c` and bin direction `d`, the radius is the first
   exit from the union along `c + s d`. Both sections are quadrant
   ellipses, so each leg's prism gives an interval of `s` from
   `reachFromPoint` in that leg's frame after projecting `c` and `d`
   into it; a leg's region is its prism interval cut by its station
   half-space, extended by the overlap of its prism interval with the
   other leg's; the ring's radius is the end of the union of the two
   regions' intervals that contains `s = 0` (the middle ring's center
   lies in both tubes; a ring beside it may lie in one). Where a ring
   center lies outside both, warn `BendClamped` and keep today's fit.
   The elbow's middle ring and the rings beside it (`isElbow`) take
   their radii from it, replacing the `LrudSection`-with-`stationOffset`
   fit and `blendElbowRings`' radius lerp; `binAngles` come from the
   envelope's reach along the ring's ±right/±up as `LrudSection::binAngle`
   does today. `measured` is true for a bin whose ray exits through a
   wall that was read. `Section` on the middle ring keeps the station's
   readings and `stationRadii` so the gallery still draws it.
   `fitSharpBends`' caps and `insideWallShift` run after as today; note
   in the commit body which corners they still move.
3. **Fallout.** `WallBeyondReading`, `CenterlineOutside` and the fold
   checks stay clean on the catalog, Phake Cave and the random seeds;
   list every catalog and sweep case whose issue counts change in the
   commit body and fix regressions in the builder. Case 25's foot stays
   wrong until T07af fixes its frames; its top corner (f1) is this
   task's: today the first elbow ring's ceiling reaches 2.66 m over the
   lead-in whose ceiling is 2 m, and the third elbow ring's wall 12.66 m
   north where the shaft wall is at 12 m (`tmp/probe-c25.json`, fig. 1
   of `plans/TUBE_SHAFT_PROFILE.html`).

**Done when.**
- The step 1 test passes as a hard test on case 30 and Phake a1 → a4,
  with the three named regressions.
- The probe numbers read, for s1's outside wall: 2.52 → ≤ 2.75 →
  3.46 m from s1 on the bisector → 2.71; for a2's inside wall: 2.24 →
  2.5 at the corner → 2.33; at case 25's f1 every elbow ring's highest
  vertex is at most 2 + `kWallTolerance` above the lead-in's axis and
  its northernmost vertex at most 12 + `kWallTolerance` north (a
  named regression in the step 1 test, on the f0 → f3 chain), and the
  lead-in floor meets the shaft's south wall at (8, −1) in profile.
- Before/after grabs of case 30 at s1 and Phake a2 and a3 (`--rings
  --sections --translucent`) in the commit body: no diamond on the
  outside, no shelf inside.
- Every `[PassageTube]` test passes.

**Commit subject.** `Fit a corner's rings to the walls of its legs`.

---

### T07af — A shaft corner's rings lie in the plane of its legs

**Goal.** At the foot of case 25's shaft (f2: 12 m plumb onto a 6 m
leg at −8°, f2 reads `L2 R2 U20 D0`) the exit's right wall crosses the
elbow and the tube reaches 1.26 m under the station although the floor
is read at it (`tmp/probe-c25.json`, fig. 2 of
`plans/TUBE_SHAFT_PROFILE.html`). `chainFrames` carries the lead-in's
frame down the round shaft by parallel transport, where it arrives
pointing north, and unwinds the 120° difference to the exit leg's
horizontal frame over the next `kTwistDistributionSamples` (4) samples.
So the three elbow rings are rolled 120°, 92° and 60° about their
tangents and the first two exit rings 30° and 0°; the middle ring's
`D0` points sideways and a 2 m side reading points down. Phake a9 (the
foot of the a6 shaft, the T07s case) is the same mechanism.

**Decision (made Oct 15).** A sharp corner with a steep leg on either
side builds every elbow ring's frame in the plane of its two legs:
`right` is the bend axis (`incoming × outgoing`, signed to keep
`right · previous right > 0`) and `up = right × tangent`. The leg
leaving the corner starts on its own frame (horizontal `up` for a
gentle leg) at its first sample. The roll the transported frame picks
up along a steep run is spread over that run's samples, between the
frame the run enters with and the one the corner at its far end wants,
instead of after the run: the steep rings are round (`fitSteepRing`),
so rolling them changes nothing. A steep run that ends the chain keeps
the transported frame.

**Read.** Research §3.1 (frames, the twist distribution) and §3.6 rows
*Twisted segment*, *Centerline outside between elbow rings* and *Rolled
rings at a shaft foot*; `chainFrames`, `inclinationWeight`,
`doubleReflection`, `elbowSamples` (`isElbow`, `BendLimit::miter`) and
`fitSteepRing` in `cwPassageTubeBuilder.cpp`; the T07h/T07j twist
tests (`right_i · right_{i−1} > 0`) in `test_cwPassageTubeBuilder.cpp`;
case 25 in `cwPassageTubeCases.cpp` and its `[!mayfail]` test
`cwPassageTubeCases keeps the centerline inside between elbow rings`.

**Modify.** `test_cwPassageTubeBuilder.cpp` first, then
`cwPassageTubeBuilder.cpp`, then `test_cwPassageTubeCases.cpp`.

**Spec.**

1. **Reproduce.** `cwPassageTubeBuilder keeps a shaft corner's rings in
   the plane of its legs`: build case 25's chain and, at each of f1's
   and f2's elbow rings, assert `|right · axis| > cos(2°)` for the
   bend's axis, and that the first ring of the exit leg has
   `|right · worldUp| < sin(2°)`; assert the lowest vertex of f2's
   middle ring lies no more than `kMinRadiusMeters + kWallTolerance`
   under f2 (today 1.26 m). `[!mayfail]` until step 2.
2. **Frames.** In `chainFrames`, after the seed pass: for each elbow
   run whose corner joins a steep and a gentle leg, set the elbow
   frames from the bend axis as decided; for each steep run between
   two gentle samples (or between a gentle sample and such a corner),
   compute the transported frame along the run, measure the signed
   roll between its last frame and the frame the far end wants, and
   add `roll · (k / n)` about the tangent to the run's k-th of n
   samples. Remove the post-run distribution (`kTwistDistributionSamples`,
   `residualTwist`) if nothing else needs it; keep the angle unwrap.
3. **Fallout.** The twist assertions on the random seeds and the sweep
   stay clean. Rerun case 25's centerline test without `[!mayfail]`; if
   it passes, move it into the catalog clean test and note in T07s's
   status that its step 2 is down to Phake a9's sagitta guard. Grab
   case 25 (`--case "25." --rings --sections --translucent`) before and
   after; the exit's right wall must run inside the tube.

**Done when.**
- The step 1 test passes as a hard test.
- Case 25's f2 elbow rings read roll 0° in the gallery report, and the
  middle ring's lowest vertex is within tolerance of 0.05 m under f2.
- `[PassageTube]` passes; the centerline test's tag change, if any, is
  in the commit body.

**Commit subject.** `Keep a shaft corner's rings in the plane of its legs`.

---

---

### T07ag — A leg's walls run through its stations' reading points

**Goal.** After T07ad (`1eb19b22`) case 30's leg s1 → s2 is a clean
linear tube, but a wider one than the readings describe. Probe of
Oct 15 (`tmp/probe30/probe-c30.json`, figure `tmp/probe30/s1s2.png`):
s1 reads `L3 R3` and s2 `L1.5 R1.5`, both facing the bisector of their
legs (bearing 30°, the chord between their chunk neighbors), so the
points the surveyor read, S1 L = s1 + 3 m at bearing 300° and S2 L =
s2 + 1.5 m at 300°, lie 2.6 m and 1.3 m from the leg's centerline and
1.5 m and 0.75 m *behind* their stations along it. The rings square to
the leg take the readings at face value as perpendicular half-widths,
3 → 1.5 m: ring 13 (1.9 m past s1) reaches 2.71 m each side where the
line S1 L → S2 L passes 2.19 m from the centerline, ring 21 (1 m before
s2) 1.65 m against 1.33 m, and the tube comes back to the readings only
at s2's own section, with a 0.4 m step on the inside wall there. The
station sections end exactly on the reading points, so the leg looks
wider than its sections at both ends.

Two rules have been tried: before T07ac, `lrudAcross` shrank each side
reading by the cosine of the facing's angle to the leg, and the leg
necked in past the corner because the elbow's middle ring kept the
reading stretched across the miter (research §3.6 *Neck past a corner
station*); T07ac took the readings at face value along the leg, which
removed the neck by widening the leg.

**Decision (made Oct 15).** A side reading is a **point on the wall**:
the station plus the reading along the record's left or right
direction (`−R` or `R` of its facing, T07c). A leg's left wall is the
straight line between its two stations' left points, its right wall
the line between the right points. A ring square to the leg at
distance `d` along it takes its left half-width where the left wall
line crosses the ring's plane, measured along the ring's `−right`
(likewise the right), so the leg interpolates between the *points*, not
between the readings: for case 30's leg s1 → s2 the half-widths run
2.6 − 1.3 · (d + 1.5) / 10.75, 2.42 m at s1 and 1.3 m at s2. Up and
down keep T07ac's rule (face value, with T07h's slope foreshortening).
A corner whose two legs read the same record then has both legs' walls
pass through the same two points, so T07ad's union of the leg tubes
meets at the reading points on both sides: the outside corner is the
reading point itself (3 m from s1 on the bisector, where today's miter
is 3.46 m) and so is the inside one. The middle elbow ring and a plain
corner ring on the bisector both reach exactly the station's section,
and the elbow-ring/leg-ring disagreement that caused T07ac's neck is
gone by construction. A record whose facing lies more than
`kMaxFacingDegrees` (60°) off the leg — a hairpin's chord — puts its
points near the centerline, so such a leg end keeps T07ac's face-value
half-width square to the leg; say so in a comment. Steep legs' rings
are round (`fitSteepRing`) and are unchanged. T07ae's rule (one record
per leg end) chooses which record a leg end reads; this task says what
a record means.

**Read.** Research §3.2.1 (the *Facing* paragraph) and §3.6 rows *Neck
past a corner station*, *Corner rings off the legs' walls* and *Leg
wider than the line between its readings*; `LrudRecord`, `lerpRecord`,
`lrudAcross` (`sideAcross`, `facesAgainst`), `WalkedLeg`, `LegTube`
(`sectionAt`, `span`), `CornerWalls`, `ringOnWalls` and
`fitElbowsToLegWalls` in `cwPassageTubeBuilder.cpp`; the T07ac test
`cwPassageTubeBuilder runs each wall from one station's reading to the
next` and the T07ad test `cwPassageTubeBuilder keeps a corner's walls
on the legs' walls` in `test_cwPassageTubeBuilder.cpp`; case 30, the
hairpin cases and `phakeCave()` in `cwPassageTubeCases.cpp`; the
mesh check's leg sections in `cwPassageTubeMeshCheck.cpp` (they run
from one station's readings toward the other and must follow the same
rule).

**Modify.** `test_cwPassageTubeBuilder.cpp` first, then
`cwPassageTubeBuilder.cpp` and `cwPassageTubeMeshCheck.cpp`.

**Spec.**

1. **Reproduce.** `cwPassageTubeBuilder runs each wall through the
   stations' reading points`: build case 30 and, for every ring on the
   s1 → s2 leg that is square to it (`!isElbow`, not the station
   rings), assert its leftmost vertex (the bin at angle π) lies within
   `kWallTolerance` of the segment S1 L → S2 L and its rightmost vertex
   (the bin at angle 0) within `kWallTolerance` of S1 R → S2 R, the
   points computed from the readings and the facings as the decision
   says. Name two regressions: the first such ring past s1 reaches
   between 2.0 and 2.4 m left (today 2.71), the last before s2 between
   1.15 and 1.5 m (today 1.65). Assert the same on the s0 → s1 and
   s2 → s3 legs and on Phake a1 → a4 (a2 and a3 read facing their
   chunk neighbors' chord). Assert s1's elbow middle ring's leftmost
   vertex lies within `kWallTolerance` of S1 L (today 3.46 m out on the
   bisector; S1 L is 3 m). `[!mayfail]` until step 2.
2. **Wall lines.** Give `WalkedLeg` (or `LegTube`) each end's left and
   right wall points, computed once from the record and its facing;
   `LegTube::sectionAt` and the leg samples' records (`lerpRecord` for
   the side readings) take the half-widths from the wall lines at the
   sample's distance along the leg, as the decision says, with the
   `kMaxFacingDegrees` fallback. `CornerWalls` needs no change beyond
   what `LegTube` gives it; confirm with the probe that s1's outside
   corner lands on S1 L and that there is no step at s2. Update the
   T07ac and T07ad tests' expected walls to the wall lines (they encode
   face-value widths today) and the mesh check's leg sections to the
   same rule, so `WallBeyondReading` measures against the lines. Remove
   `[!mayfail]`.
3. **Fallout.** Catalog, sweep, Phake Cave and the random seeds: list
   every case whose issue counts change in the commit body, and fix
   regressions in the builder. Phake a2 → a3 must stay free of its neck
   (the T07ac regression: a2's inside wall peaks at the corner); the
   hairpin cases (T07k) must keep their width through the fallback.
   Grab case 30 at s1 and s2 and Phake a2 (`--rings --sections
   --translucent`) before and after.

**Done when.**
- The step 1 test passes as a hard test with its named regressions.
- The probe (`tmp/probe30/probe30`) shows case 30's s1 → s2 ring
  half-widths within `kWallTolerance` of the S1 L → S2 L and S1 R →
  S2 R lines, the s1 elbow's outside vertex on S1 L, and the s2 ring's
  neighbors within tolerance of the lines (no step).
- Every `[PassageTube]` test passes; the T07ac and T07ad tests' new
  expectations are explained in the commit body.

**Commit subject.** `Run each wall through its stations' reading points`.

---

### T07ah1 — Loft each leg between its station sections and drop the elbows

**Read.** First the parent section `### T07ah` above (grep -n
'^### T07ah ' and read to its `---`) for the goal and decision rules
1–5. Then in `cwPassageTubeBuilder.cpp`: `LrudRecord`, `lerpRecord`,
`lrudAcross`, `WalkedLeg`, `chainSamples` (the `SharpBend`/
`elbowSamples` branch), `chainFrames`/`spreadRoll`/`steepCornerFrame`/
`hasRoundRings`, `fitSteepRing`, `fitLrudRing`, `LegTube`,
`CornerWalls`, `ringOnWalls`, `fitElbowsToLegWalls`,
`blendElbowRings`, `fitSharpBends`, `reshapeInside`,
`insideWallShift`, `stretchAcrossMiter`, `widestReadings`,
`largestReading`, `Ring` (`isElbow`, `stationOffset`, `section`) and
`MeshWriter`; `phakeCave()` and case 30 in `cwPassageTubeCases.cpp`;
the T07ag test and the T07ac/T07ad/T07af/T07h/T07j/T07r/T07s/T07v
tests in `test_cwPassageTubeBuilder.cpp`. Research §3.2.1 (*Facing*
and *Shaft readings*), §3.6 row *Question-mark pit and inflated
corners*.

**Modify.** `test_cwPassageTubeBuilder.cpp` first; then
`cwPassageTubeBuilder.h/.cpp`; `cwPassageTubeMeshCheck.cpp`, the
gallery model and `test_cwPassageTubeCases.cpp` only as far as step 4
says.

**Scope.** Rules 1, 4 and 5 of the parent. Rule 2 (the plumb section
shape) is T07ah2; rule 3's hull cap is T07ah3; the checker's loft and
hull rules are T07ah4. This task's station behavior is an interim:
where a chain's two legs at a station have different sections, the
chain holds both rings at the station (the incoming loft's end ring
then the outgoing loft's start ring, both centered on the station) and
`MeshWriter` stitches them like any two consecutive rings. The mesh
stays closed; the wedge between the two sections is a stitched
zero-length segment that may self-intersect (a4, a6 in Phake). Do not
fix that here.

**Spec.**

1. **Reproduce** (`[!mayfail]` until step 3; the test computes the
   sections from the catalog readings and facings with a
   `sectionThrough(station, record, facing)` helper the test owns: a
   quad-ellipse through the reading points, left/right along the
   facing's sides, up/down plumb, at face value):
   - `cwPassageTubeBuilder lofts each leg between its station sections`:
     case 30; for every ring on s1 → s2 at fraction `t` of the leg,
     every vertex `k` lies within `kWallTolerance` of
     `lerp(S1[k], S2[k], t)`; no ring of the build has `isElbow` (or the
     flag no longer exists); the ring at s1 *is* s1's section (its
     outside vertex at S1 L, 3 m from s1 on the bisector). Same on
     s0 → s1 (s0 round 0.5 m) and Phake a1 → a4. Named regression:
     today case 30's elbow rings at s1 reach beyond the loft by the
     numbers the test prints.
   - `cwPassageTubeBuilder reads one record per leg end`: Phake a4; the
     a3 → a4 loft ends on `L0.05 R2 U2 D1.7` facing a5 and the a4 → a6
     loft starts on `L5 R0.05 U3 D23` facing a6 (a vertex 23 m below
     a4 — the drop drawn at face value); today `widestReadings` merges
     the two records.
2. **Sections and lofts.** `WalkedLeg` carries its two end sections
   (points per bin, built once from the leg's own record at each end,
   rule 4, with the `kMaxFacingDegrees` fallback). A gentle leg's
   section is the quad-ellipse of rule 1. A steep leg (today's
   `kSteepLegDegrees` test) gets, *for this task only*, a horizontal
   circle of radius `mean(L, R)` centered on the station — today's
   `fitSteepRing` shape without its frame; T07ah2 replaces it. Every
   ring between the stations is the point-wise lerp of the two
   sections; a `Ring` stores its vertices and derives center (lerp of
   the stations), `radii` and `binAngles` for the gallery and the
   existing tests. Rings carry no `isElbow`/`stationOffset`.
3. **Delete** `SharpBend`, `elbowSamples`, `sharpBendAfter`, the inset
   constants (`kCornerInset*`, `kElbowStepDegrees`, `kMaxMiterStretch`),
   `chainFrames`, `spreadRoll`, `steepCornerFrame`, `hasRoundRings`,
   `kSteepWeight`, `fitSteepRing`, T07h's slope foreshortening in
   `lrudAcross`, `LegTube`, `CornerWalls`, `ringOnWalls`,
   `fitElbowsToLegWalls`, `blendElbowRings`, `fitSharpBends`,
   `reshapeInside`, `insideWallShift`, `stretchAcrossMiter`,
   `BendLimit`, `BendChange`, `Miter`, `widestReadings`,
   `largestReading`, the BendClamped warning, and the tests that pinned
   them (T07c's facing test stays; the T07h/T07j/T07r/T07s/T07v/T07ac/
   T07ad/T07af/T07ag elbow, corner, frame-twist and steep-ring tests
   go, replaced by step 1's). No new constant replaces the inset. Drop
   `[!mayfail]` from step 1's tests.
4. **Keep the build green, nothing more.** `cwPassageTubeMeshCheck.cpp`:
   remove only the rules that read the deleted fields (elbow and
   corner rules); the loft and hull rules are T07ah4. Gallery: remove
   only the elbow layer and report lines that read `isElbow`. In
   `test_cwPassageTubeCases.cpp`, update `kOpenSweepCases`,
   `kUnzippedSweepCases`, `kJunctionLimitRandomSeeds` and
   `kOpenJunctionFreeCases` so the catalog, sweep and seed tests pass,
   and list in the commit body every case or seed whose issue counts
   changed and by how much (the interim stitch of the Scope paragraph
   is the expected reason for new SelfIntersection/Sliver at
   two-record stations; say so per case). Do not tune thresholds to
   move those counts.

**Done when.**
- Step 1's two tests pass as hard tests with the named regressions.
- The probe (`tmp/probe30/probe30 "Phake" …` with `cones.py`) shows
  case 30's left panel matching its right panel on s0 → s1 → s2.
- `cwPassageTubeBuilder.cpp` has lost every function in step 3 (expect
  roughly a third of the file).
- Every `[PassageTube]` C++ test passes.

**Commit subject.** `Loft each leg between its station sections and
drop the elbows`.

---

### T07ah2 — A plumb leg's section is horizontal through its left and right readings

**Read.** The parent `### T07ah` section (rule 2 and
`plumb-options.svg`'s option (a)); in `cwPassageTubeBuilder.cpp` after
T07ah1: the section construction in `WalkedLeg`, the steep-leg circle
T07ah1 left, `kSteepLegDegrees`; Research §3.2.1 *Shaft readings*;
Phake a6 → a9 → a10 in `cwPassageTubeCases.cpp`.

**Modify.** `test_cwPassageTubeBuilder.cpp` first; then
`cwPassageTubeBuilder.cpp` only.

**Spec.**

1. **Reproduce** (`[!mayfail]` until step 2):
   `cwPassageTubeBuilder lofts a plumb leg between its horizontal
   sections`: Phake a6 → a9; every ring between them has its leftmost
   and rightmost vertices within `kWallTolerance` of the lines
   a6 L → a9 L and a6 R → a9 R (a6: 1 m left, 5 m right of the shot
   along facing 230°; a9: 2.3 m and 2 m), and its front/back vertices
   at `lerp(3, 2.15, t)` along `±facing`. Named regression: after
   T07ah1 the shaft rings are circles of radius `mean(L, R)` centered
   on the shot — 3 m at a6 where the lines pass 1 m and 5 m.
2. **Plumb sections.** A steep leg's end section is horizontal: left
   and right along the facing's sides at `L` and `R`, front and back
   along `±facing` at `mean(L, R)`, interpolated around the ring as the
   quad-ellipse does (the record's up and down shape nothing on a
   plumb leg). Replace T07ah1's circle with it; no other builder
   change. Drop `[!mayfail]`.

**Done when.**
- Step 1's test passes as a hard test with the regression.
- `tmp/probe30/pit_profile.py` (after rebuilding Release2 and the
  probe) shows every a6 → a9 ring on the blue cone lines and none
  beyond them.
- Every `[PassageTube]` C++ test passes; list any catalog/seed count
  change in the commit body (pit and aven cases only are expected to
  move).

**Commit subject.** `Read a plumb leg's section through its left and
right readings`.
