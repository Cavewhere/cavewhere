# Passage tube rendering — follow-up tasks

The T07-letter tasks: fixes and tools that came out of looking at the
built tubes after `TUBE_RENDERING_IMPLEMENTATION_PLAN.md`'s T07 landed.
The plan's §0 ground rules and §1 agent contracts apply to every task
here; the `tube-render` workflow points agents at this file for any task
whose id matches `T07` + a letter. **Read only your own task section**
(find it with `grep -n "^### <task>"` and read from there to the next
`---`): the other sections are not context you need, and finished tasks
have moved to `TUBE_RENDERING_DONE.md`.

## Status

| Task | Commit(s) | State |
|---|---|---|
| T07b LRUD ring shape | `c8123d70` | done |
| T07c LRUD facing, arm ends | `f4c24241` | done |
| T07e Mesh checks | `c80bdfca` | done |
| T07f Junction catalog, sweep | `a823539a` | done |
| T07g Gallery executable | `ddb1a09f` | done |
| T07d0 Inner-bend folds | `3218e0d5` | done |
| T07d Stitch pass | `f842bb87` | done |
| T07h Sloped, steep, thin sections | `2ed35aa5` | done |
| T07i Trim against the mesh | `6e714a7d` | done |
| T07j Elbow rings, through by size | `80c2e9c6` | done |
| T07k Hairpins, random surveys | `477cedbf` | done |
| T07l Overlaps and slivers | `0b70e0a7` | done |
| T07m Mouth on the through grid | `18bbfcb9` | done |
| T07n Shared windows | `5ac0bd4c` | done |
| T07q Cut arms where the axis enters | `137d160a`, `1211eebe` | done |
| T07r Corner station inside its ring | `0c83bb57`, `15f96d1b` | done |
| T07p Caps where the survey continues | `44ee82b0` | done |
| T07s Centerline between elbow rings | `b24136fa` (step 1) | **open**: step 2 down to Phake a9, see its status |
| T07o Unzipped arms | `6e18b2b4`, `b5ee2e08` (steps 1–3) | **open**: steps 4–5 and the a10 regression, see its status |
| T07y Rings in the gallery | `4de85913` | done |
| T07w Open a project in the gallery | `8c59b22a` | done |
| T07z Readings each ring was fitted to | `ce8633d3`, `ed2b02a4` | done |
| T07v Rings within the station's readings | `29dbcab1` | done |
| T07aa Layer colors in the gallery | `04ef8135` | done |
| T07ac Section between stations | `ae3de2e6`, `70c7a4f0` | done |
| T07ad Corner rings sample the legs' walls | `1eb19b22` | done |
| T07af Shaft corner rings in the plane of the legs | `d6dc315b` | done |
| T07ag Walls through the reading points | `fdfbbdfb` | done |
| T07ah Legs are lofts, stations are hulls, no elbows | — | split into T07ah1–T07ah4 (first monolithic run stopped Oct 15) |
| T07ah1 Lofts, drop the elbows | `d0d33219` | done |
| T07ah2 Plumb section through L/R | `8001628b` | done |
| T07ah3 Steep legs: vertical rings along the plan direction | — | blocked Oct 19: box rings vs. trim/zip; superseded by `TUBE_RENDERING_REDESIGN.md` steps 1–4 (junction by union) |
| T07ah4 Loft checker, overlap, catalog baseline, gallery | — | superseded by `TUBE_RENDERING_REDESIGN.md`; re-read before any run |
| T07ae A leg and its end station share a record | — | folded into T07ah |
| T07ab Gallery speed with rings on | — | open |
| T07t Unread corner inset | — | superseded by T07ah |
| T07u Bare hairpin dome | — | open |
| T07x Passages that run into each other | — | open |
| T07ai Interactive junction sandbox in the gallery | `06e8f36a` | done |
| T07aj Resizable gallery panels | `fe627784` | done |
| T07ak The union is the builder's output | `e838a9f3` | done |
| T07al One vertical ring per station, plan bisector | `d140b404` | done |
| T07am Rings vertical everywhere, padded along the plan run | `4cf0ac67` | done |
| T07an Delete the junction machinery | `5db8e15c` | done |
| T07ao Shafts: rings square to the leg, round caps, cutoff from the readings | `04dcabe5` | done |

Order for the next runs: the builder rewrite (T07ak–T07an) landed Oct 20;
next, evaluate it in the gallery and sandbox against
`TUBE_RENDERING_ISSUES.md` (known and suspected issues, I01–I13), then re-read T07ab → T07o (rest) → T07u → T07x against the
redesign (T07x is now the mechanism, T07o and T07u mostly vanish with the
zip), then the plan's T08–T12. T07s step 2 and T07t
are superseded by T07ah (there are no elbow rings or insets after it).
`plans/TUBE_CORNER_FOLLOWUPS.html` draws what T07ad and T07ae change
and `plans/TUBE_SHAFT_PROFILE.html` what T07ad and T07af change at the
two corners of case 25's shaft (open them in a browser; they are local
only). The deferred
review findings that belong to no task are listed at the end of this
file.

---

### T07o — Arms the stitch leaves unzipped

**Goal.** Join the arms that today end in a fan with a `JunctionUnzipped`
warning. The sweep has 32 of them, all with an arm 2.5 times the through
or two small arms in a 90° bend: `ratio 2.5, 3 arms` at every bend (14),
`ratio 2.5, 1 arm, bend 90` at plan 20 and 30 (4), `ratio 2.5, 2 arms`
at bend 40 and 90 (6), `ratio 0.3, 2 arms opposite` and `3 arms` at bend
90 (7), and one `ratio 1, 3 arms, bend 40`. On screen the arm is a tube
ending in a flat cap with the through running into it, like a pipe into
a tin can (`plan 30, profile 0, ratio 2.5, 1 arm, bend 90, same
section`). Two shapes are behind the warning:

- *Full wrap.* An arm larger than its through has a mouth that loops the
  whole circumference, so `mouthPolygon` returns nullopt (closing gap
  over `resolution / 2`). T07j makes the biggest leg the through, but
  only when a leg continues it within `kMaxChainTurnDegrees` and it is
  wider and taller than every other leg: three equal big arms, or two big
  arms more than 100° apart, leave the small passage as the through.
- *Rechained grazing.* When T07j does make the big arm the through, the
  through's own leg on the arm's side becomes an arm leaving 20–30° from
  it (plan 20/30 at bend 90): its columns run inside the big wall and
  miss, `mouthPolygon` sees a point off the through, and the leg ends in
  a fan. This is the grazing limit of `joinsAtGrazingAngle` in a shape
  the case parameters do not show.

**Read.** Research §3.3.1 (chains, big passage), §3.3.3 steps 3–4, §3.6
rows "Full-wrap arm" and "Sparse mouth loop". Code: `buildChains`
(`throughPairs`, `isBigPassage`, `continuation`) in
`cwPassageTubeBuilder.cpp`; `mouthPolygon`, `cutWindow`, the
`JunctionUnzipped` branches of `stitchJunctions`, `closeWithFan` in
`cwPassageTubeJunction.cpp`; `galleryCases` in `cwPassageTubeCases.cpp`;
`joinsAtGrazingAngle`, `overlapsMouths`, `checkOpenSweepCaseBuildsClean`
in `test_cwPassageTubeCases.cpp`.

**Modify.** `cwPassageTubeCases.cpp`, `test_cwPassageTubeCases.cpp`
first (the reproduction, see step 1); then `cwPassageTubeBuilder.cpp`,
`cwPassageTubeJunction.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.**

1. **Reproduce before fixing.** Add two gallery cases to `galleryCases`:
   - `18. Three big arms into a small passage`: through `{0.8, 0.8, 1,
     0.5}` (gallery 3's), arms `{2, 2, 4, 1}` at plan 45, 135 and 270.
     No leg is wider and taller than every other, so the small passage
     stays the through and every arm wraps it.
   - `19. Through doubling back beside a big arm`: the sweep's `plan 30,
     profile 0, ratio 2.5, 1 arm, bend 90, same section` as a gallery
     case (`kSweepThroughSection`, `throughBendDegrees = 90`, arm at plan
     30 scaled 2.5). The big arm becomes the through and the through's
     back leg is left 30° from it.
   Add `warningCount(debug, JunctionUnzipped)` to the builder test
   helpers if it is not shared already, then two tests in
   `test_cwPassageTubeCases.cpp`:
   - a hard test `cwPassageTubeCases reports the arms it leaves unzipped`
     that builds 18 and 19 and checks `JunctionUnzipped` fires for every
     big arm of 18 and the back leg of 19, and that the meshes are
     closed. It pins the reproduction and is rewritten by step 4.
   - a `[!mayfail]` test `cwPassageTubeCases joins every arm` over the
     gallery cases and the sweep junction cases (`kSmokeStride`
     subsample under `[PassageTube]`, full set under `[Sweep]`) that
     skips cases without a `JunctionUnzipped` warning and checks zero
     such warnings and a clean report for the rest.
   Grab 18 and 19 with the gallery (`--grab`) and commit this step on
   its own (`Reproduce the arms the junction stitch leaves unzipped`)
   before touching the builder, so the workflow's review can see the
   before state.
2. **Measure.** Count the 32 sweep warnings by text ("could not be
   joined" with the mouth polygon's reason: wraps around / point off the
   through / "runs backward") in the commit body of step 4.
3. **Through pair by size.** In `buildChains`, the through pair at a
   junction is the pair of legs, among those at least as wide and tall
   as every leg outside the pair, with the smallest turn; ties in size
   no longer veto it (`isBigPassage` compares against legs outside the
   pair only). When the biggest leg has no partner within
   `kMaxChainTurnDegrees`, it still claims the through: its chain ends
   at J with a plumb end ring (T07h) pushed past J by the arm reach
   (`reach` in `findJunctions`) so every other leg's mouth lands on it,
   and the other legs are arms into that end. Gallery 18 then chains the
   two big arms 90° apart as the through (elbow rings) with the third
   big arm and both small legs as arms.
4. **Full wrap that remains.** An arm as large as its through whose mouth
   still wraps (equal sizes at a steep profile) is cut as a window that
   takes the whole circumference over its ring span: `windowBoundary`
   returns the front and back loops (T07n step 3 already joins several
   loops) and the band is zipped between them with the mouth in the
   middle. Rewrite the step 1 hard test to assert zero `JunctionUnzipped`
   warnings for 18.
5. **Rechained grazing.** For 19 and the plan 20/30 `ratio 2.5, bend 90`
   cases, trim the grazing leg as a grazing arm (long mouth along the
   wall, as `zips a mouth that runs along the through's ceiling` does for
   plan 20) instead of failing on columns that miss: a column whose
   polyline misses the through takes the parameter of its nearest point
   on the through surface (`ontoThroughWall`) when that point is within
   `kJunctionTolerance` of the column, else the arm is reported as
   grazing with `joinsAtGrazingAngle` extended to read the built chains
   (angle between the arm's last leg and the through's leg at J), not
   the case parameters. The tip crease of a grazing mouth stays accepted.

**Done when.**
- Step 1 lands first as its own commit with the hard test passing (the
  warnings fire) and the `[!mayfail]` test failing on 18, 19 and the 32
  sweep cases.
- "chains two big arms as the through": gallery 18 builds with a chain
  whose legs are two of the big arms, no `JunctionUnzipped` warning, and
  a clean report.
- "ends a big dead-end through past the junction": a case with one big
  arm doubling back 150° from the through (plan 30, bend 0, ratio 2.5)
  builds closed, with the big arm's chain ending past J and both through
  legs zipped into it.
- Gallery 19 builds closed with the back leg either zipped or reported
  as grazing, and no issue other than the tip crease.
- The sweep's `JunctionUnzipped` count drops from 32 to the cases named
  by family in the commit body; `cwPassageTubeCases joins every arm`
  loses `[!mayfail]` or those cases are listed in it.
- Every other `[PassageTube]` test passes.

**Commit subjects.** Step 1: `Reproduce the arms the junction stitch
leaves unzipped`. Steps 2–5: `Chain the biggest passage through every
junction and zip full-wrap mouths`.

**Status (Oct 14).** Steps 1–3 are committed (`6e18b2b4`, `b5ee2e08`):
the sweep's `JunctionUnzipped` cases went 34 → 19 (5 wrap around, 8 off
the wall, 6 run backward), listed by family in `kUnzippedSweepCases`.
Still to do, as one more commit (`Zip full-wrap mouths and trim grazing
legs`):

1. Step 4 (whole-circumference window) and step 5 (grazing leg) as
   specified; `kUnzippedSweepCases` shrinks to whatever remains, named
   by family in the commit body.
2. **The a10 regression from T07v.** `29dbcab1` refit the elbow rings
   along Phake Cave's a9 → a10 chain, which moved the a10 junction
   mouth; the arm strip now crosses itself and leaves a sliver beside
   the already-accepted a10 Crease, hidden by two new rows in
   `kPhakeCaveIssues` (`{a10, SelfIntersection}`,
   `{a10, SliverTriangle}`). Same junction mechanics as steps 4–5
   (an arm mouth that the window cannot take cleanly): fix it here and
   remove the two rows. The gallery shows it with
   `--project <Phake Cave 3000.cwproj> --station a10 --rings --translucent`
   (the project file stays outside the repository; the catalog's Phake
   copy in `cwPassageTubeCases.cpp` is what the test builds). The
   `[!mayfail]` test "cwPassageTubeJunction keeps every mouth step
   within twice the through's spacing" also lost seed 1 to `29dbcab1`
   (2.17 m against a 1.97 m limit); check whether the same fix returns
   it.

---

### T07s — Keep the centerline inside between elbow rings

**Superseded (Oct 15) by T07ah:** there are no elbow rings after it; the centerline rule itself stays in the checker.

**Goal.** T07r keeps a corner station inside its middle elbow ring, but
the skin between that ring and the next is a straight chord across the
bend. Where the corner reads zero on the bend's outside (the floor is
`kMinRadiusMeters` 0.05 under the station) the chord sags past that
floor, and the leg leaving the station runs outside the tube for about
a meter. Phake Cave shows it at a9, the foot of the 23.44 m shaft from
a6 (`down = 0`, a sharp elbow onto the −8.4° leg to a10): the station is
0.017 m outside, the a9 → a10 centerline up to 0.084 m outside at
0.81 m, back inside by 1.15 m. The checker misses it because
`centerlineInside` reuses `kWallTolerance` (0.15 m); with a 0.01 m
tolerance Phake Cave is the only `[PassageTube]` case that changes.

**Read.** Research §3.2 (rings), §3.3.2 (bends), §3.6 "Corner station
outside its elbow ring" and "Centerline outside between elbow rings".
Code: `elbowSamples`, `blendElbowRings`, `fitLrudRing`, `fitSharpBends`
in `cwPassageTubeBuilder.cpp`; `centerlineInside` in
`cwPassageTubeMeshCheck.cpp`; T07r's cases 23 and 24 and their tests.

**Modify.** `cwPassageTubeMeshCheck.h/.cpp`, `cwPassageTubeCases.cpp`,
`test_cwPassageTubeCases.cpp`, `test_cwPassageTubeMeshCheck.cpp` first;
then `cwPassageTubeBuilder.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.**

1. **Reproduce before fixing.**
   - `centerlineInside` gets its own `kCenterlineTolerance = 0.01`
     (float noise only; a zero reading legitimately puts the wall on the
     centerline, so "on the wall" still counts as inside) in place of
     `options.wallToleranceMeters`. A mesh-check unit test: a point
     0.05 m outside a box is reported, a point 0.005 m outside is not.
   - Phake Cave then fails `CenterlineOutside` at a9 (the a6 → a9 and
     a9 → a10 legs). Add both to `kPhakeCaveIssues` under a comment
     naming this task, so the catalog test keeps passing and the two
     rows are removed in step 2.
   - Catalog case `25. Zero floor reading at the foot of a shaft`: three
     stations, a vertical leg of 12 m down onto a leg of 6 m at −8°,
     corner LRUD `{2, 2, 20, 0}`, far stations `{2, 2, 2, 0}` and
     `{1, 1, 2, 1}`; it fails `CenterlineOutside` under `[!mayfail]`
     until step 2 and is left out of the catalog clean test. Bump the
     catalog count test.
   Commit this step on its own (`Report centerline points within the
   wall tolerance`).
2. **Hold the leg between the rings.** The elbow keeps a ring at the
   station itself, between the middle ring and the outgoing ring (and
   likewise on the incoming side when the bend is sharper than
   `kSharpBendDegrees`), fitted with the station's readings in the leg's
   own frame; the skin then follows the legs, which is where the
   centerline runs, and the chord problem goes away. If that ring folds
   through its neighbors on a hairpin (the T07j fold rule), fall back to
   growing the zero reading to the chord's sagitta
   `inset · (1 − cos(φ/2))` and warn `BendClamped`. Remove the two a9
   rows from `kPhakeCaveIssues`; case 25 drops `[!mayfail]` and joins
   the catalog clean test.

**Done when.**
- Step 1 lands first: the tightened tolerance, the unit test, case 25
  under `[!mayfail]`, Phake Cave a9 accepted.
- After step 2: case 25 and Phake Cave pass `CenterlineOutside` with no
  accepted a9 rows; the T07j and T07r elbow tests still pass; every
  other `[PassageTube]` test passes.

**Commit subjects.** Step 1: `Report centerline points within the wall
tolerance`. Step 2: `Keep the centerline inside between elbow rings`.

**Status (Oct 13).** Step 1 is committed (`b24136fa`); case 25 needed
four stations (a 10 m level lead-in before the shaft) to reproduce,
since the failure needs the frames rolled down the shaft. Step 2 is on
hold: the station ring cannot be built (the station lies in the middle
ring's plane, so half of such a ring folds behind it on every bend),
the sagitta guard fixes Phake Cave a9 but not case 25, and case 25's
cause is frame roll after the shaft (middle ring `up` = (0.48, 0.88, 0),
~30° between neighbors), which lives in `chainFrames`. T07y draws the
rings so the choice (fix the roll at a shaft foot + sagitta guard, or
guard only with case 25 left `[!mayfail]`) can be made by eye; the
spec's step 2 is rewritten after that.

**Status (Oct 15).** T07af (`d6dc315b`) fixed the frame roll at a shaft
foot: case 25 left `kOpenJunctionFreeCases`, the catalog clean test
covers it, and its `[!mayfail]` centerline test is removed. Step 2 is
down to Phake Cave a9's sagitta guard; rewrite the spec's step 2 for
that alone before running it.

---

### T07t — Inset an unread corner by its borrowed walls

**Superseded (Oct 15) by T07ah:** there is no corner inset after it.

**Goal.** Catalog case 24 (T07r) still creases at its corner: a passage
with no readings bends sharply and borrows its first station's section,
but the elbow inset (`elbowSamples`) uses the default radius, so the
rings beside the corner fold where the borrowed walls are wider than
that radius. The open-catalog `[!mayfail]` clean-mesh test carries it.

**Read.** Research §3.3.2 (bends), §3.6 "Corner station outside its
elbow ring". Code: `elbowSamples`, `buildRings`, `nearestFitted`,
`borrowedFit`, `fitSharpBends` in `cwPassageTubeBuilder.cpp`; case 24
in `cwPassageTubeCases.cpp` and its tests.

**Modify.** `cwPassageTubeBuilder.cpp`, `test_cwPassageTubeBuilder.cpp`,
`test_cwPassageTubeCases.cpp`.

**Spec.** The reproduction exists (case 24). Resolve the section each
station will use (fitted or borrowed) before `elbowSamples` runs, so
the inset at an unread corner comes from the borrowed walls exactly as
it would from measured ones. Case 24 then builds clean and moves from
the `[!mayfail]` open-catalog test to the catalog clean test.

**Done when.** Case 24 builds clean; a builder test shows an unread
corner's elbow inset equals the inset of the same bend with the
borrowed readings written at the corner; every other `[PassageTube]`
test passes.

**Commit subject.** `Inset an unread corner by its borrowed walls`

---

### T07u — Report a hairpin dome the other chain leaves uncovered

**Goal.** T07p closes hairpin ends with domes that overlap inside each
other, but `CapExposed` tags only flat caps, fans, mouth fans and
boundary discs. A dome whose apex the other chain's tube fails to cover
(a 150°+ hairpin, or two legs of very different width) draws as a bare
nose at a station where the survey continues, and nothing reports it.

**Read.** Research §3.6 "Caps where the survey continues". Code: the
dome writer and `Debug::caps` tagging in `cwPassageTubeBuilder.cpp`,
`capsExposed` in `cwPassageTubeMeshCheck.cpp`, catalog case 20 and the
hairpin tests from T07p.

**Modify.** `cwPassageTubeBuilder.cpp`, `cwPassageTubeMeshCheck.cpp`,
`cwPassageTubeCases.cpp`, `test_cwPassageTubeCases.cpp`,
`test_cwPassageTubeMeshCheck.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.**

1. **Reproduce before fixing.** Tag domes as caps. Catalog case
   `26. Hairpin between a wide and a narrow passage`: a 1.5 m wide leg
   doubling back 160° into a 0.3 m wide leg, so the wide dome's apex
   lies outside the narrow tube. `CapExposed` fires there under
   `[!mayfail]`. Case 20's domes must stay unreported: a dome counts as
   exposed only when one of its `kCapSamples` points lies outside every
   other chain **and** more than `kCenterlineTolerance` from them.
   Commit on its own (`Report hairpin domes the other chain leaves
   uncovered`).
2. **Cover it.** Extend the narrower end's dome offset so its tube
   reaches the wider dome's apex (offset = the wider end's half-width
   along the narrow leg), capped at the leg's length; case 26 then
   builds clean.

**Done when.** Step 1 lands first; after step 2 case 26 and case 20
pass `CapExposed`; every other `[PassageTube]` test passes.

**Commit subjects.** Step 1: `Report hairpin domes the other chain
leaves uncovered`. Step 2: `Cover a hairpin dome with the narrower
passage`.

---

### T07x — Union passages that run into each other

**Goal.** Two passages that pass through each other away from any
survey junction (parallel passages with generous LRUD, loop closures,
an upper level crossing a lower one) draw as interpenetrating tubes:
`PassageOverlap` reports them and nothing else happens. The user sees
one tube's wall inside the other. The fix is the junction machinery
applied between unconnected chains: cut each tube where it enters the
other and zip the two openings, giving one surface.

**Read.** Research §3.4 (junctions), §3.6 "Interpenetrating tubes";
`stitchJunctions`, `trimArm`, `cutArm`, `mouthPolygon`,
`windowBoundaries`, `zipBand` in `cwPassageTubeJunction.cpp`;
`passageOverlaps` in `cwPassageTubeMeshCheck.cpp`; the T07o full-wrap
zip (a crossing passage is a full-wrap arm on both sides).

**Modify.** `cwPassageTubeCases.cpp`, `test_cwPassageTubeCases.cpp`
first; then `cwPassageTubeJunction.cpp`, `cwPassageTubeBuilder.h/.cpp`,
`test_cwPassageTubeJunction.cpp`.

**Spec.**

1. **Reproduce before fixing.** Catalog cases, `[!mayfail]` on
   `PassageOverlap` until step 2:
   - `27. Two parallel passages whose walls overlap`: two 20 m legs 2 m
     apart, each reading 1.5 m toward the other.
   - `28. A passage crossing over another`: a level 20 m leg 1 m above a
     perpendicular one, each 1 m tall.
   - `29. A loop closing through its own wall`: a four-station square
     whose last leg's tube runs into the first's.
   Commit on its own (`Reproduce passages that run into each other`).
2. **Cut and zip.** After junction stitching, find chain pairs whose
   ring tubes intersect (`RingTube::inside` over the other chain's
   ring vertices, `kJunctionTolerance`); for each, the chain with the
   larger section at the crossing is the through and the other is cut
   on both sides of it with `trimArm`/`cutArm` (a full-wrap mouth on
   each side, from T07o), the windows are opened and zipped as at a
   junction. Parallel passages (case 27), where neither passes through
   the other, are joined along the overlap: the shared wall is removed
   from both and the two outlines zipped into one section. Warnings:
   `PassageUnion` naming both chains; fall back to the T07n fan close
   with `JunctionOverlap` where the zip fails.

**Done when.** Step 1 lands first; after step 2 cases 27–29 pass
`PassageOverlap` and the closed-mesh tests; the full `[Sweep]` tests
and every other `[PassageTube]` test pass; `PassageOverlap` counts on
the random seeds drop and the commit body lists before/after.

**Commit subjects.** Step 1: `Reproduce passages that run into each
other`. Step 2: `Union passages that run into each other`.

---



### T07ai — Interactive junction sandbox in the gallery

**Goal.** A gallery row the user edits live: add legs at a junction, drag
their angles, and change every station's LRUD with sliders, with the tube
rebuilt and re-checked on each change. The redesign (`TUBE_RENDERING_REDESIGN.md`
steps 1–5) was settled on 2D drawings; this is where the 3D result gets the
same rapid feedback — the user moves a slider and says what is wrong.

**Read.** `cwPassageTubeCases::JunctionCase`, `ArmSpec`, `ArmLeg`,
`Section`, `toTrip`/`toCase`/`toSurvey` in `cwPassageTubeCases.h/.cpp`;
`cwPassageTubeGalleryModel` (`setCurrentIndex`, the build-and-check path,
`addRandom`, `RingGeometry`); `TubeGalleryWindow.qml` (the list, the
overlay toggles, the presets); `cwTubeGalleryGrabber`.

**Modify.** `cwPassageTubeGalleryModel.h/.cpp` (or a sibling
`cwPassageTubeSandboxModel`), `TubeGalleryWindow.qml` plus a new
`TubeSandboxPanel.qml`, `test_cwPassageTubeGalleryModel.cpp`.

**Spec.**

1. **A sandbox row.** One row named "Sandbox" at the top of the list that
   holds an editable `JunctionCase`: the through's bend, profile, bend
   profile and its three sections; a list of arms, each with plan and
   profile angle, length, far section, junction section, and the flags for
   blank readings; `sampleSpacing`. "Add arm" appends an `ArmSpec` at the
   next free 90°; "Remove" drops the selected one; a "Load from row" copies
   the selected catalog or sweep case into the sandbox so any failure can be
   nudged.
2. **Sliders.** Angles in whole degrees (plan −180..180, profile −90..90,
   through bend 0..180), lengths 0.5..30 m, LRUD 0..10 m in 0.1 m steps with
   a text field beside each for exact values. Every change rebuilds the
   survey with `toSurvey(toCase(...))`, builds and checks it as a row
   selection does, and keeps the camera where it is (frame only on load).
   Rebuilds are debounced to the last change within one frame; a build
   that takes over `kSandboxBuildBudgetMilliseconds` lowers the sample
   spacing resolution for the sandbox only and reports it in the panel.
3. **The record at J.** The arm's own reading at J and the through's are
   separate sliders (they are separate records; step 2 of the redesign
   draws each chunk from its own); the panel shows both and the checker's
   `JunctionReadingConflict` when they disagree.
4. **Steep legs.** Profile sliders reach ±90 so a pit and a shaft are one
   drag away; the wedge (redesign step 5) and the box's pad are visible
   with Rings on.
5. **Export.** "Copy as case" puts the sandbox's `JunctionCase` on the
   clipboard as the C++ initializer `galleryCases` uses, so a found failure
   becomes a catalog case by paste. "Grab" writes the current view with
   `cwTubeGalleryGrabber`.
6. **Tests.** The sandbox model round-trips a catalog case (load, change
   nothing, same mesh); a slider change produces a different survey;
   "Copy as case" output parses back to an equal `JunctionCase`. QML: the
   panel shows one slider group per arm and "Add arm" adds one
   (`cavewhere-qml-test --platform offscreen -input test-qml/tst_TubeSandboxPanel.qml`
   if a QML test target exists for the gallery; else the C++ test covers
   the model only).

**Done when** the user can open the gallery, pick Sandbox, add two arms,
drag the second to 30° and its L to 3 m, and watch the end-cap notch from
redesign step 3 appear and the slide fix it, without restarting.

### T07aj — Resizable gallery panels that stay in their columns

**Goal.** The gallery window's left list bleeds past its column into the
toolbar and the 3D view, and neither the list nor the sandbox panel can be
resized. Put the three panes — case list, center (toolbar, view, report),
sandbox panel — in a `QC.SplitView` with draggable handles, and keep every
pane's content inside its pane.

**Read.** `tube-gallery/qml/TubeGalleryWindow.qml`: the top `RowLayout`
(three children: the list `ColumnLayout` at `listWidth`, the center
`ColumnLayout` with the `QQ.Flow` toolbar and `viewAreaId`, and
`TubeSandboxPanel` at `sandboxPanelWidth`), the `caseListId` delegate (a
`QQ.Rectangle` whose `RowLayout` anchors fill it; the badge spills), and
`rootId.grabAll`, which hides the side panes for screenshots.
`cavewherelib/qml/GitHistoryPage.qml` for the project's `QC.SplitView`
usage (`QC.SplitView.preferredWidth`, `QC.SplitView.minimumWidth`).
`tube-gallery/qml/TubeSandboxPanel.qml`. `cwTubeGalleryGrabber` grabs the
view item, so pane sizes change nothing in grabs.

**Modify.** `tube-gallery/qml/TubeGalleryWindow.qml`, `TubeSandboxPanel.qml`
if its root needs a width contract.

**Spec.**

1. **SplitView.** Replace the top `RowLayout` with a horizontal
   `QC.SplitView` filling the window. Panes: list (preferred
   `listWidth`, minimum `kListMinimumWidth` ≈ 200), center
   (`QC.SplitView.fillWidth: true`, minimum ≈ 400), sandbox panel
   (preferred `sandboxPanelWidth`, minimum ≈ 280). The sandbox pane stays
   `visible: galleryModelId.sandboxSelected && !rootId.grabAll`; a hidden
   pane gives its width to the center. `grabAll` hides the list pane as
   today.
2. **Contain each pane.** Each pane root is a `QQ.Item` with `clip: true`
   whose child layout anchors fill it (a `ColumnLayout` placed directly as
   a SplitView child reports its implicit size and overflows). The list
   delegate's width is `caseListId.width` so the badge sits inside the
   column, with the name label eliding first. The toolbar `QQ.Flow` wraps
   within the center pane's width. The sandbox panel's content scrolls
   vertically inside its pane (a `QC.ScrollView` or `QQ.Flickable` with
   `clip: true`) rather than growing the window.
3. **Remember the widths.** Store the list and sandbox pane widths in
   `QC.Settings` or `QtCore.Settings` under a gallery group, restored on
   open; the window size already follows `initialWidth`/`initialHeight`.
   If the gallery has no settings precedent, keep this to the session
   (skip persistence) and say so.
4. **No regression in grabs.** `--grab` and `--grab-all` output is pixel
   identical before and after for a catalog case (compare one grab of the
   T junction from each build with `cmp`); the grabber takes the view item,
   not the window.
5. **Tests.** The gallery has no QML test target; verify by running the
   gallery, dragging both handles, selecting Sandbox, and grabbing the
   window with `screencapture -l` or the grabber's window mode if one
   exists; put the screenshot in `/Users/cave/.claude/jobs/ab1fa6ed/tmp/`
   and name it in the commit body. Run the `[PassageTube]` suite as the
   workflow does.

**Done when** the list's badges and names stay inside the left column at
every width, the toolbar never sits under the list, both handles drag, and
the sandbox panel scrolls instead of pushing the view off screen.

---

### T07ak — The union is the builder's output

**Goal.** Redesign steps 2 and 4 (`plans/TUBE_RENDERING_REDESIGN.md`,
"Step 2 — a junction is a union", "Step 4 — the union spike", "What changes
in steps 1–5"): `cwPassageTubeBuilder::build` returns the boolean union of
the chains' capped tubes instead of the trimmed-and-zipped surface. The
spike (`cwPassageTubeUnion`, commits `8677d4ce`, `94301677`, `92eb59bb`)
proved the mechanism behind a gallery toggle; this task makes it the one
path CaveWhere, the gallery and the tests all use. The junction code is
bypassed here and deleted in T07an; the ring rules change in T07al and
T07am.

**Read.** `cwPassageTubeUnion.h/.cpp` in full (`unite`, `chainTubes`,
`chainComponents`, `merged`, `capNotch`, `slideCaps`, `removeSlivers`,
`contains`, `kSurfaceToleranceMeters`); `cwPassageTubeBuilder::buildDebug`
and `buildChainRings`, `MeshWriter` (`beginCap`, `beginTripBoundary`,
`setSource`, `finish`) and `addChainMesh` in `cwPassageTubeBuilder.cpp`;
`cwPassageTubeMesh.h` (`tripIndexRanges`, `tripUuids`,
`tripBoundaryRanges`, `Warning`); `cwPassageTubeMeshCheck::check`,
`selfIntersections`, `creases`, `wallsThroughReadings` (or whatever T07ag
named it), `capsCovered`, `allTripsVisible`; `cwPassageTubeGalleryModel::build`,
`buildUnion`, `unionVisible`, `slideCapsEnabled` (the spike's toggles);
`TubeGalleryWindow.qml` overlay toggles; `cwPassageTubeManager::build`;
`test_cwPassageTubeUnion.cpp` (its helpers build the T and the wide fork
inline); the manifold API for face provenance: `Manifold::AsOriginal()`,
`Manifold::OriginalID()`, `MeshGL::runOriginalID`, `MeshGL::faceID`
(`conan_deps` headers under `manifold/`).

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeUnion.h/.cpp`,
`cwPassageTubeMesh.h`, `cwPassageTubeMeshCheck.h/.cpp`,
`cwPassageTubeGalleryModel.h/.cpp`, `TubeGalleryWindow.qml`,
`test_cwPassageTubeBuilder.cpp`, `test_cwPassageTubeCases.cpp`,
`test_cwPassageTubeMeshCheck.cpp`, `test_cwPassageTubeUnion.cpp`,
`test_cwPassageTubeGalleryModel.cpp`.

**Spec.**

1. **Reproduce.** In `test_cwPassageTubeCases.cpp`, a test
   `"cwPassageTubeCases big arms build clean through the union"`,
   `[PassageTube][cwPassageTubeCases][!mayfail]`, that runs
   `checkCaseBuildsClean` on `"3. Big arm into a small passage"`,
   `"16. Arm with its own section at J larger than the through"` and
   `"18. Three big arms into a small passage"` through
   `cwPassageTubeBuilder::build`. They fail today: their zip leaves
   `Crease`, `SelfIntersection`, `FoldBack` issues (they are in
   `kOpenCatalogJunctionCases`). The failure output must show the issue
   types and counts.
2. **The pipeline.** `buildDebug` becomes: legs → chains → rings
   (`buildRings`, `placeChainVertices`, unchanged here) → `slideCaps` when
   `Options::slideCaps` (new, default true) → one closed tube per run of
   rings (`chainTubes`) → `unite` per connected component
   (`chainComponents`) → `merged`. `stitchJunctions` is no longer called;
   `addZipBands`, mouths and windows produce nothing (T07an deletes them).
   `build` returns the union. `buildChainRings` stays as the rings-only
   entry point.
3. **Gates and cleanup.** Before the union, each chain tube is checked with
   `cwPassageTubeMeshCheck::selfIntersections` on its own; a tube that
   crosses itself is still fed to the union (manifold accepts it) but
   warned as `Warning::ChainSelfCrossing` naming the chain's first station
   and the crossing count, so the gallery report and the tests can see
   which inputs are dirty. `unite` keeps the `removeSlivers` cleanup. A
   union component with negative volume is dropped from the mesh and
   warned as `Warning::EnclosedVoid` with its volume in m³ and the
   position of its centroid in the message (redesign step 4, item 4). An
   input manifold rejects stays warned as today (`Status` name in the
   message) and left out.
4. **Trip attribution.** The union keeps the trip of every triangle: each
   input tube is one trip (a chain's rings carry `tripId` per leg; a chain
   that crosses trips is cut into one tube per trip run, the cut ring
   shared, so the union also joins them), passed to manifold with
   `AsOriginal()` so `runOriginalID` on the result maps each triangle back
   to its tube; `tripIndexRanges`/`tripUuids` are rebuilt from that
   mapping, one range per trip, triangles reordered by trip. The trip
   boundary discs (`tripBoundaryRanges`, drawn only while a neighbor trip
   is hidden) are the cut rings' fans, emitted outside the union exactly as
   `beginTripBoundary` does today, so hiding a trip still closes its
   neighbor.
5. **Debug for the checker.** `Debug::triangleChain` is filled from the
   provenance (the chain of the input tube each triangle came from),
   `triangleCap`/`caps` from `faceID` (a triangle whose source triangle
   was a cap fan keeps its cap index; the fan center vertices that survive
   go in `intendedCreaseVertices`). `triangleInZipBand` is all false until
   T07an removes it. `Debug` also records the slide: the cap vertices the
   slide moved and the ones it left (`QVector<CapVertex>` moves into
   `cwPassageTubeBuilder` or a small struct of its own, so the header
   dependency runs one way), for the gallery report.
6. **Checker rules at a union.** `PassageOverlap` is impossible on the
   union and stays the expected state of the rings-only mesh
   (`test_cwPassageTubeUnion.cpp` already asserts both). A `Crease` whose
   two triangles come from different chains is the seam where two passages
   meet, which is what the data says, and is left out; creases inside one
   chain stay issues. A reading point that lies inside another chain's
   tube (`cwPassageTubeUnion::contains` over `chainTubes(debug)`) is an
   opening, so `WallMissed`/`WallBeyondReading`-style rules skip it; a
   reading point outside every other tube is checked as today. A cap
   covered by another tube is removed by the union, so `CapExposed` only
   looks at caps that survive. Keep the zip-band exemption code paths
   compiling; they are dead and T07an deletes them.
7. **On the surface counts as inside.** `cwPassageTubeUnion::contains`
   returns true for a point within `kSurfaceToleranceMeters` of the
   surface that the winding number puts on or inside it, so such a cap
   vertex is not in the notch and does not slide (redesign step 4, item 2:
   two such vertices on the T, two on the fork). Update the header comment
   and the fork test's counts (7 moved, 0 left).
8. **Gallery and CaveWhere.** The gallery's Union toggle goes: `build`
   is the union now. In its place a "Raw tubes" toggle (off by default,
   `rawTubesVisible`) draws `buildChainRings` + `chainTubes` merged, so the
   overlap and the union can be compared on one row; "Slide caps" stays
   and sets `Options::slideCaps`. The report keeps the union lines
   (components, volume, genus, rejected inputs, slide counts, time) for
   every build. `--union` becomes a no-op flag printing that the union is
   the default, `--raw-tubes` replaces it for grabs; the sandbox inherits
   the path. `cwPassageTubeManager` needs no change beyond the default
   options. Measure the Phake Cave build time before and after
   (`[PassageTube]` timing test or a one-off print) and put both in the
   commit body.
9. **Tests.** The step-1 test loses `[!mayfail]`. `kOpenCatalogJunctionCases`
   is rechecked case by case: every case that builds clean now moves to
   the clean test; a case that is still dirty stays listed with its issue
   types named in the comment (T07al/T07am own them; T07an deletes the
   list). The random-seed lists (`kJunctionLimitRandomSeeds`,
   `kOpenJunctionFreeRandomSeeds`, `kTwoSectionWildSurveys`) are rechecked
   the same way. Every catalog case that builds clean at HEAD builds clean
   through the union; a case that regresses is listed in a new
   `kUnionOpenCases` with the issue named and the commit body says why.
   New tests: trip ranges of a two-trip survey through a junction (each
   triangle's trip matches its chain; the boundary disc exists and sits in
   `tripBoundaryRanges`); `EnclosedVoid` on a hand-built survey that
   wraps a pocket (three stacked overlapping passages around an air gap,
   or a hollow box of tubes), asserting the warning carries the volume and
   the mesh has no negative-volume component; `ChainSelfCrossing` on a
   random seed that crosses itself today; the seam crease exemption on the
   30° fork (`check` reports no `Crease`); the reading-point exemption on
   case 5 of the redesign (through `L8` into the arm).

**Done when** `build` returns the union for every caller, cases 3, 16 and
18 build clean, no catalog case regresses (or each regression is listed
and named), trip hiding still closes the neighbor with a disc, the fork's
notch slides as in the spike, and the gallery shows the union by default
with "Raw tubes" as the comparison.

**Commit subject.** `Build passage tubes as the union of their chains`

---

### T07al — One vertical ring per station, square to the plan bisector

**Goal.** Redesign step 1 ("Step 1 — a plain chain in plan: the miter
rule") and the chain half of step 2. A chain is a run of consecutive shots
of one chunk, so every station on it has exactly one record; each station
holds one ring whose left and right lie along the right of the plan
bisector of its two legs; every bend from 0 to 180° is mitered; the
hairpin pinch is accepted down to `kMinRadiusMeters`. Chains that share a
station (a junction, a chunk boundary, a tie-in) overlap there and the
union of T07ak joins them. Loch's rule, the through, the dome, the double
ring and the facing rules all go.

**Read.** Redesign step 1 ("Decision", "The hairpin pinch, accepted") and
step 2 ("Decision", cases 1–7); `buildChains`, `classifyChainEnds`,
`chunkFacings`, `sectionAxes` (the gentle branch), `isSameSection`,
`buildRings`, `borrowWalls`, `endOffset`, `replaceShaftReadings`,
`mergeRepeatedShots` in `cwPassageTubeBuilder.cpp`; `Leg`, `Chain`,
`EndCap`, `Frame`, `Section`, `Ring` in `cwPassageTubeBuilder.h`;
`cwPassageTubeGeometry::setRingVertices`, `readingPoints` (T07ag);
`cwPassageTubeSandboxModel::junctionConflict` (what the sandbox already
calls a conflict); the catalog in `cwPassageTubeCases.cpp` and the ring
tests in `test_cwPassageTubeBuilder.cpp` and `test_cwPassageTubeCases.cpp`.
The prototype of the rule is `junction_union.py`'s `chain_tube` (local,
`/Users/cave/.claude/jobs/ab1fa6ed/tmp/probe31/`): read it if the doc
leaves a question.

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeGeometry.h/.cpp`,
`cwPassageTubeMesh.h`, `cwPassageTubeGalleryModel.cpp` (ring report
text), `cwPassageTubeCases.cpp`, `test_cwPassageTubeBuilder.cpp`,
`test_cwPassageTubeCases.cpp`, `test_cwPassageTubeGalleryModel.cpp`.

**Spec.**

1. **Reproduce.** `"cwPassageTubeBuilder miters every bend with one ring
   per station"`, `[PassageTube][cwPassageTubeBuilder][!mayfail]`, on an
   inline three-station survey 1 → 2 → 3 (leg 1→2 bearing 0, 10 m; leg 2→3
   10 m, all level, `L2 R1 U2 D1`), generated over bends θ of 0, 60, 120,
   150 and 170°: `buildChainRings` gives one chain with exactly one ring
   centered on station 2, its `frame.right` equal to
   `clockwiseOf(normalize(d_in + d_out))` within 1e-4, its left and right
   vertices 2 m and 1 m from the station along that right, and the leg's
   sample rings advancing from the end rings without a fold (each sample
   ring's `facingOf` within 90° of its leg's direction). Today θ = 150 and
   170 hold two rings at station 2 (`kMaxFacingDegrees`); the failure
   output shows the ring count and the right vectors.
2. **Chains are chunk runs.** `buildChains` returns one chain per maximal
   run of consecutive positioned shots of one chunk, in chunk order, with
   `mergeRepeatedShots` applied first as today. A chain whose last station
   is its first (a chunk that closes on itself) is a loop: no caps, the
   seam ring reading the first occurrence's record. Every other chain end
   is a `Fan`. `EndCap::Mouth`, `EndCap::Dome`, `startThroughReachMeters`,
   `endThroughReachMeters`, `startCapOffsetMeters`, `endCapOffsetMeters`,
   `kMaxChainTurnDegrees`, `kMinArmSpreadDegrees`, `kMinThroughReachDegrees`,
   `kThroughReachMarginMeters`, `kDomeHalfRingScale`, `kDomeHalfRingDepth`,
   `LegSection` and the passage LRUD (`fromPassageLrud`, `toPassageLrud`,
   `replaceShaftReadings`, `kShaftRatio`, `Warning::ShaftReading`) go:
   sections are read at face value and nothing sizes a through any more.
3. **The facing.** `Leg::fromFacing`/`toFacing` and `chunkFacings` go. In
   `buildRings`, station s of a chain has `facing(s) = normalize(plan(d_in)
   + plan(d_out))`, `clockwiseOf(plan(d_in))`'s facing (the leg direction
   itself) when the sum is shorter than `kMinHorizontalLength`, `plan(d_out)`
   at a chain start and `plan(d_in)` at a chain end; `right(s) =
   clockwiseOf(facing(s))`. The ring at s is the quadrant ellipse of the
   station's one record: L along −right, R along right, U and D plumb,
   centered on s, radii floored at `kMinRadiusMeters`. Both legs at s use
   that ring; `isSameSection`, the second ring, the `facesAgainst` swap
   and `kMaxFacingDegrees` go. A leg's sample rings are the point-wise
   lerps of its two station rings as today. `sectionAxes`' steep branch,
   `isSteep` and `kSteepLegDegrees` stay untouched here (T07am replaces
   them); its gentle branch becomes the rule above.
4. **Readings at a shared station.** Each chain reads its own record at a
   station other chains reach (redesign step 2). Where two chains' records
   at one station both read a side wall and the readings differ by more
   than `kReadingConflictRatio` (3.0) in either direction, the builder
   warns `Warning::JunctionReadingConflict` once per station, naming the
   station, the two trips and the two readings (case 5 of the redesign,
   `L8` against `R1.5`). `borrowWalls` stays as is within a chain.
5. **Catalog baseline.** Re-record `test_cwPassageTubeCases.cpp` against
   the new rule: tests of deleted behavior (through choice, reach, domes,
   mouths, the two-section stitch, `kUnzippedSweepCases`'s premise) are
   deleted with a one-line note in the commit body; tests of a case
   building clean stay and run through the union; the ring-level tests
   (`WallBeyondReading`, rings within readings, centerline inside) are
   re-checked case by case and any case that now fails is either fixed by
   this rule or named in the list T07ak made (`kUnionOpenCases`) with the
   reason. Add catalog cases for the step-1 sweep (the θ series of step 1
   as one `Case` per θ, or a `[Sweep]` entry) and for redesign step 2's
   cases 1–7 where the catalog lacks them (the T, the 30° fork, legs 4°
   apart, the six-leg hub, `L8` into the arm, the crawl off a chamber,
   the blank record at a tie-in). Phake Cave builds clean or its remaining
   issues are listed by type and count in the commit body.
6. **Gallery.** The ring report drops the facing and "two sections" text
   and prints the bisector right; "Readings" overlay unchanged. The
   sandbox's conflict label can point at the builder's
   `JunctionReadingConflict` instead of its own check if that is simpler;
   either way both agree on case 5.

**Done when** every station of every chain holds one ring, the θ sweep
passes without `[!mayfail]`, `kMaxFacingDegrees`, `isSameSection`,
`chunkFacings`, Loch's rule, the through and the dome are gone from the
builder, `JunctionReadingConflict` fires on `L8` against `R1.5` and stays
quiet on equal readings, the catalog baseline is re-recorded, and the
`[PassageTube]` suite passes.

**Commit subject.** `Miter every bend with one ring per station`

---

### T07am — Rings stand vertical everywhere, padded along the plan run

**Goal.** Redesign step 5 ("Step 5 — profile: rings stand vertical
everywhere", "Decision", "The wedge, accepted"). Every ring is vertical:
L and R level along the step-1 right, U and D plumb. A leg's rings advance
along its plan run, padded at each end so a plumb leg is a box with
thickness; the pad hands over continuously with inclination, so
`kSteepLegDegrees`, the level section and the inclination blend go. A
station where a leg's padded ring is displaced from the station splits the
chain, and the pieces meet by union.

**Read.** Redesign step 5 in full and step 4 "What changes in steps 1–5";
`sectionAxes` (steep branch), `isSteep`, `kSteepLegDegrees`,
`inclinationWeight`, `smoothstep`, `Options::blendLowDegrees/
blendHighDegrees`, `capOffset`, `Frame::inclinationWeight`,
`buildRings` after T07al, `borrowWalls`' steep handling, `endSection`'s
`hasOwnSection`; `cwPassageTubeGeometry::setRingVertices` (it takes the
inclination weight) and `placeChainVertices`; `cwPassageTubeSandboxModel`
profile sliders; the catalog's pit and shaft cases (10, 25 and the
`profile ±30` sweeps). The prototype is `pit_profile.py`'s `leg_tube`
(local, probe31): `PAD = 1`, `e = 1 − min(1, run / (2·PAD))`, rings at
`a.x − e·PAD` and `b.x + e·PAD`.

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeGeometry.h/.cpp`,
`cwPassageTubeGalleryModel.cpp`, `cwPassageTubeCases.cpp`,
`test_cwPassageTubeBuilder.cpp`, `test_cwPassageTubeCases.cpp`,
`test_cwPassageTubeGeometry.cpp` if one exists.

**Spec.**

1. **Reproduce.** `"cwPassageTubeBuilder stands a pitch's rings vertical
   along its plan run"`, `[PassageTube][cwPassageTubeBuilder][!mayfail]`:
   an inline survey 1 → 2 → 3, leg 1→2 level bearing 0, 10 m, `L2 R2 U2
   D1` at 1 and the lip 2 `L2 R2 U2 D19`; leg 2→3 pitched down α for 20 m,
   bottom 3 `L2 R2 U19 D1`; generated over α = 90, 80, 70, 45°. Assert for
   the steep leg's tube (the chain piece holding leg 2→3): every ring's
   `frame.up` is `kWorldUp` within 1e-4 and its `frame.right` horizontal;
   at α = 90 its two end rings are centered at `plan(2) ∓ 1 m` (pad =
   mean(L, R)/2 = 1) at the stations' heights, so the tube is a box 2 m
   deep; at α = 80 (`planRun = 3.47 m ≥ pad0 + pad1 = 2`) `e = 0` and the
   end rings are centered on the stations; at α = 70 and 45 likewise; and
   the lip's ring in the level leg's tube is the plumb bar `U2 D19`. Today
   the 90, 80 and 70° legs read level sections (`kSteepLegDegrees` 45°):
   the failure output shows the frame ups and the ring centers.
2. **The rule.** For each leg with plan run `r` between stations a and b,
   `pad(s) = mean(L, R)/2` of the station's drawn walls (after
   `borrowWalls`; the default radius when nothing is read), `e = 1 −
   min(1, r / (pad(a) + pad(b)))`. The leg's end rings are centered at
   `a − e·pad(a)·u` and `b + e·pad(b)·u`, where `u` is the leg's unit plan
   direction, or the chain's incoming unit plan direction when `r <
   kMinHorizontalLength` (the outgoing one at a chain start; the leg's own
   bearing from the shot when the chain is one plumb leg). Each end ring is
   the station's quadrant ellipse — L and R along the step-1 `right(s)`
   of T07al, U and D along `kWorldUp` — translated to that center. The
   leg's sample rings lerp the two end rings point-wise with centers on the
   segment between the two ring centers. `Frame::tangent` is `u`,
   `Frame::up` is `kWorldUp`, `Frame::right` the step-1 right; `Frame::
   inclinationWeight`, `inclinationWeight`, `smoothstep`,
   `Options::blendLowDegrees/blendHighDegrees`, `capOffset`, `isSteep`,
   `kSteepLegDegrees` and the level section (`sectionAxes`' steep branch,
   `borrowWalls`' halfDepth rewrite, `LegEnd::isSteep`, `hasOwnSection`)
   go. `setRingVertices` loses its inclination-weight parameter.
3. **Where a chain splits.** Two legs meeting at a station share its ring
   when both place their ring at the station (`e·pad < kMinRingSpacingMeters`
   on each side); otherwise the chain splits there into two capped tubes,
   each ending on its own padded ring (the lip holds the level tube's tall
   bar and the box's padded ring; redesign step 5, "a change of regime").
   `Chain` gains the piece boundaries (or `buildChains` emits one chain per
   piece after a pass over the legs, whichever is smaller) so `chainTubes`
   and `chainComponents` of T07ak see one tube per piece and join them by
   union; `slideCaps` then handles the box's top cap at the lip.
4. **The wedge, as read.** No rule at the lip or the bottom: the level leg
   lofts `D1 → D19` and the leg out of the bottom `U19 → U2`. Tests assert
   the loft is what the data says (the level tube's floor at station 2 is
   19 m down) and that the union of the pieces at α = 90 is one closed
   component whose volume is the box's 2 × 4 × 20 m³ plus the level tube's
   less the overlap, within 2%.
5. **Catalog baseline.** Re-record cases 10, 25, the `profile ±30`
   sweeps, Phake (a6 slot, a9 shaft, the floor) and the random seeds that
   hold a plumb leg: every one builds clean through the union or is named
   in `kUnionOpenCases` with its issue. Add the α sweep of step 1 as
   catalog cases (one per α). Watch the coincident faces at the pit mouth
   the redesign flags ("For the spike"): the box's top ring reaches `U2`,
   exactly the level tube's ceiling. If `removeSlivers` leaves slivers
   there, say so in the commit body with the sliver count rather than
   adding a rule.
6. **Gallery.** The ring report drops the inclination weight and prints
   the pad and `e` of the ring's leg; the sandbox's profile slider at ±90
   shows the box.

**Done when** every ring's up is plumb, the α sweep passes without
`[!mayfail]`, a plumb leg is a box `2·pad` deep along the incoming plan
direction, `kSteepLegDegrees`, the level section and the inclination blend
are gone, chains split only where a padded ring leaves its station, and
the `[PassageTube]` suite passes.

**Commit subject.** `Stand every ring vertical, padded along the plan run`

---

### T07an — Delete the junction machinery

**Goal.** The union (T07ak) replaced trim and zip, and T07al and T07am
replaced the ring rules, so the junction code, its warnings, its checker
exemptions and its test lists are dead. Remove them (redesign step 4,
"the deletion list"), re-baseline the checker against the union output,
and leave the builder's header describing the pipeline that exists.

**Read.** Redesign step 4 (the deletion list) and "What changes in steps
1–5"; `cwPassageTubeJunction.h/.cpp` (all of it: `stitchJunctions`,
`trimArm`, `growAndTrim`, `RingTube`, `zipWindows`, `zippedWindow`,
`isZipBandManifold`, `leaveOutArm`, `cutArm`, `mouthPolygon`,
`windowBoundaries`, `zipBand`); in `cwPassageTubeBuilder.h/.cpp`:
`MouthPoint`, `ZipCorner`, `WindowMouth`, `JunctionWindow`,
`Chain::startMouth/endMouth/startFanCenter/endFanCenter/windows`,
`Debug::triangleInZipBand`, `MeshWriter::setSource`'s `isZipBand`,
`addZipBands`, `addChainMesh`'s mouth strips, `kFanCenterBlend`,
`capTriangles`/`planarPolygon`/`isInteriorChord` (keep what the caps still
use); `cwPassageTubeMesh::Warning` (`ArmInsideThrough`, `JunctionOverlap`,
`JunctionUnzipped`); `cwPassageTubeMeshCheck` (the zip-band branch of
`selfIntersections`, `passageOverlaps`, header comments naming zip bands);
`cwPassageTubeGeometry.h` (the header comment names
`cwPassageTubeJunction`; `placeRingVertices`' "junction grew a column"
fold rule); `test_cwPassageTubeJunction.cpp`; in `test_cwPassageTubeCases.cpp`:
`kUnzippedSweepCases`, `kOpenCatalogJunctionCases`, `kJunctionLimitRandomSeeds`,
`kOpenJunctionFreeCases`, `kOpenJunctionFreeRandomSeeds`,
`kTwoSectionWildSurveys`, `checkJoinsEveryArm`, `isJoinedJunctionCase`;
`cwPassageTubeGalleryModel`'s report lines about mouths, windows and zip
bands; `cwPassageTubeManager::deliverMesh`'s warning text.

**Modify.** Delete `cwPassageTubeJunction.h/.cpp` and
`test_cwPassageTubeJunction.cpp`; edit `cwPassageTubeBuilder.h/.cpp`,
`cwPassageTubeGeometry.h/.cpp`, `cwPassageTubeMesh.h`,
`cwPassageTubeMeshCheck.h/.cpp`, `cwPassageTubeGalleryModel.h/.cpp`,
`cwPassageTubeManager.cpp`, `test_cwPassageTubeBuilder.cpp`,
`test_cwPassageTubeCases.cpp`, `test_cwPassageTubeMeshCheck.cpp`,
`test_cwPassageTubeGalleryModel.cpp`. Sources are globbed with
`CONFIGURE_DEPENDS`, so deleting a file needs no CMake edit; re-run cmake
once so the glob drops it.

**Spec.**

1. **Reproduce.** `"cwPassageTubeBuilder has no junction stage"`,
   `[PassageTube][cwPassageTubeBuilder][!mayfail]`, asserting behavior
   rather than a member that this task removes: for every catalog junction
   case (`hasJunction`), `buildDebug` emits none of `ArmInsideThrough`,
   `JunctionOverlap`, `JunctionUnzipped`, every chain's `startCap` and
   `endCap` is `Fan` or `None`, and every triangle's `triangleChain` is a
   real chain index (no zip-band triangles with no chain). It fails today
   only if T07ak left a warning or a `Mouth` cap in place; if it passes on
   today's code, say so in notes and keep it as the guard (the stage
   contract allows a passing reproduce test when the task is a deletion).
2. **Delete.** Everything the Read list names, with these keeps: fan caps
   (`addFan`, `addFlatCap`, `capTriangles` when a non-simple polygon still
   needs the flat cap, `kFanCenterBlend` if the flat cap still uses it),
   trip boundary discs, `mergeRepeatedShots`, `borrowWalls`,
   `Warning::RepeatedShot`, `Warning::NoPassageData`, `Warning::ChainTorn`,
   `Warning::LrudOutOfRange`, `Warning::SourceChange`,
   `Warning::ScrapOpening`, and T07ak's `EnclosedVoid`,
   `ChainSelfCrossing` and T07al's `JunctionReadingConflict`.
   `placeRingVertices`' fold rule for grown columns goes with `isGrown`;
   a ring that folds behind the previous one is now a chain self-crossing
   the gate reports. Remove the `cwPassageTubeJunction` mention from
   `cwPassageTubeGeometry.h` and rewrite `cwPassageTubeBuilder.h`'s class
   comment to the pipeline as it is: shots → chunk-run chains, split where
   a padded ring leaves its station → vertical rings mitered in plan →
   capped tubes → cap slide → union per component, with the checker's
   expected states.
3. **Checker.** `selfIntersections` takes no zip-band input: a crossing
   between two chains' triangles is `PassageOverlap`, every other crossing
   `SelfIntersection`; `Debug::triangleInZipBand` and the parameter paths
   that read it go. Header comments stop describing zip bands. `check`
   over the union output is the baseline: the catalog clean tests run
   through `build`, and the rings-only tests through `buildChainRings` +
   `chainTubes` with `PassageOverlap` expected (no test treats it as a
   failure).
4. **Test lists.** `kUnzippedSweepCases`, `kOpenCatalogJunctionCases`,
   `kJunctionLimitRandomSeeds`, `kOpenJunctionFreeCases`,
   `kOpenJunctionFreeRandomSeeds`, `kTwoSectionWildSurveys`,
   `checkJoinsEveryArm` and `isJoinedJunctionCase` go; the cases they
   named stay in the catalog and run in the clean tests. What is still
   dirty after T07ak–T07am lives in `kUnionOpenCases` alone, each entry
   with its issue types in a comment, under one `[!mayfail]` test. The
   commit body lists that list in full with counts.
5. **Gallery and manager.** Report lines and overlay code about mouths,
   windows, zip bands, through reach and grown rings go;
   `cwPassageTubeManager::deliverMesh`'s warning text covers the warnings
   that remain. Grab the T junction and the fork before and after
   (`--grab` into `/Users/cave/.claude/jobs/ab1fa6ed/tmp/T07an/`) and
   confirm with `cmp` that the mesh images are identical: this task
   changes no geometry.

**Done when** `cwPassageTubeJunction.*` and `test_cwPassageTubeJunction.cpp`
are gone, `git grep -i "zip\|mouth\|through reach\|isGrown\|JunctionUnzipped\|ArmInsideThrough"`
over `cavewherelib/src/cwPassageTube*` and `testcases/test_cwPassageTube*`
is empty (apart from `cwPassageTubeCases` case names), the gallery grabs
match pixel for pixel, `kUnionOpenCases` is the only open list, and the
`[PassageTube]` suite passes.

**Commit subject.** `Delete the junction trim and zip`

---

### T07ao — Shafts: rings square to the leg, round caps, cutoff from the readings

**Goal.** A steep leg is drawn as a shaft: a round tube standing on its
centerline with a dome at each end, instead of the slab the vertical-ring
rule makes of it. Which legs are shafts follows from the shot and its
readings, with the ratios exposed in the gallery so they can be tuned by
eye; `TUBE_RENDERING_ISSUES.md` I01 and I14 are what this fixes.

**What is wrong today (I14).** T07am stands every ring vertical and pads a
leg's end rings along its plan direction `u`: the start ring at
`a − e·pad(a)·u`, the end ring at `b + e·pad(b)·u`. On a plumb leg the two
rings are 2·pad apart in plan and the leg's length apart in height, and the
loft between them is a *slanted slab*, 2·pad deep, leaning across the
shaft: on the sandbox case below (horizontal lead-in, then 10 m straight
up, sections L=R=1, U=1.5, D=1) the slab covers only 2.5 m of the 10 m
centerline, which pokes out above and below it (`plans/TUBE_SHAFT_CAPS.svg`,
left panel, draws it). Without the pad the same leg is a plane. The pad
rule was papering over a leg that vertical rings cannot draw.

```
JunctionCase { .name = "Sandbox", .throughSection = {1, 1, 1.5, 1},
    .throughAtJunction = {1, 1, 1.5, 1}, .throughPastJunction = {1, 1, 1.5, 1},
    .throughBendDegrees = 0, .throughProfileDegrees = 0,
    .throughBendProfileDegrees = 90, .arms = {}, .sampleSpacing = 1 }
```

**The cutoff, from the readings.** For a leg of length `ℓ` at clino `θ`
with walls `L, R` across and `U, D` vertical (each the mean of the two
ends' drawn walls), the vertical-ring tube is the section swept along the
leg; any horizontal slice of it between the end rings is `(U+D)·cotθ` deep
along the shot and `L+R` wide across it, whatever `ℓ` is. A leg is a
**shaft** when both hold:

```
riseRatio = ℓ·sinθ / (U+D)              > Options::shaftRiseRatio      (default 1)
thinness  = tanθ · (L+R) / (U+D)        > Options::shaftThinnessRatio  (default 1)
```

The first says the leg climbs more than its section is tall (a leg that
rises less is a step or ramp inside one section height, and vertical rings
draw it fine at any angle). The second says the slab would be thinner along
the shot than it is wide. With the sandbox sections a 10 m leg is a shaft
above 51°, a 2 m leg never is, a 3 m leg above 56°; a tall narrow fissure
(`U+D = 6`, `L+R = 1`) only above 80°, a wide low bedding plane (`U+D = 1`,
`L+R = 8`) above 7°. `thinness` is +∞ for a plumb leg (`run = 0`) and 0 for
a level one. `LegPad` records `isShaft`, `riseRatio` and `thinness` per
leg for the ring report.

**The shaft.**
1. **Its own piece.** A shaft leg splits from its chain at both stations
   (like a padded leg does today), so it is one closed tube unioned with
   its neighbors; the neighbors keep their vertical ring at the station.
2. **Rings square to the leg.** No pad. Each end ring is centered on its
   station in the plane square to the leg's axis `d`: `L` and `R` along
   `right = clockwiseOf(planDirection)` (the leg's own plan direction, else
   the incoming leg's, else the outgoing one's, else the station's facing,
   as the pad already picks it), and `mean(L, R)` along `right × d` both
   ways, so a plumb shaft is as deep along the shot as it is wide. Sample
   rings lerp between the two as today. The ring's frame tangent is `d`.
   Only the side bins count as measured; the ring carries no `Section`
   (`U` and `D` are the shaft's run, not its walls, so `WallMissed` does
   not test them there).
3. **Round caps.** Past each end ring, `kDomeRings` (4) rings shrink toward
   an apex along the outward axis and the fan closes the apex ring. Height
   per `Options::shaftCapHeight`: `Reading` (default) — the `U` reading
   when the cap points up, `D` when it points down, so at the sandbox's J
   the dome reaches the passage floor and the shaft's lowest 1.5 m sits
   inside the passage, where the union removes it; `MeanWidth` —
   `mean(L, R)`, a hemisphere on a round shaft; `Flat` — no dome. Shape
   per `Options::shaftCapShape`: `Round` — scale `cos φ`, offset
   `h·sin φ`, `φ = k/(n+1)·π/2`; `Cone` — linear.
4. **Options off.** `Options::shaftRingsSquare = false` classifies no leg a
   shaft and the builder behaves as at HEAD apart from the pad scale.

**The pad on the legs that are not shafts.** `pad = padScale · (|dz|/ℓ) ·
mean(L, R)` with `Options::padScale` default 1 (HEAD: `mean(L, R)/2`
regardless of inclination). The inclination factor is I01's ruling: a
level leg stands on its stations whatever the room width; a padded leg's
footprint is `pad(a)+pad(b)`, so a steep leg that is not yet a shaft is as
deep as it is wide. `LegPad`'s header comment states this.

**Gallery.** `cwPassageTubeGalleryModel` holds a
`cwPassageTubeBuilder::Options` (sample spacing still per row) with
Q_PROPERTYs and plain setters for `capsSlid`, `padScale`,
`shaftRiseRatio`, `shaftThinnessRatio`, `shaftRingsSquare`,
`shaftCapHeight` (int) and `shaftCapShape` (int); every setter clears the
rows' cached issue counts and rebuilds the current case, as `setCapsSlid`
does today (one private helper). `TubeGalleryWindow.qml` gets a `Shaft
rings` checkbox beside `Slide caps`, and under the checkbox row a row of
labeled sliders with a value field — `Pad ×` 0–2, `Shaft rise` 0–3,
`Shaft thin` 0–5, all step 0.1 — and two combo boxes, cap height
`U/D reading | Mean L/R | Flat` and cap shape `Round | Cone`, following
`TubeSandboxPanel.qml`'s `SliderRow`. The ring report line appends
`rise %1, thin %2` and `(shaft)` when the leg is one. The sandbox's
profile slider is the lever to watch the flip.

**Tests** (`[PassageTube]`): a plumb leg with the sandbox sections is a
shaft, its rings' `frame.tangent` is `±kWorldUp`, `kDomeRings` rings sit
past each end ring with centers along the axis, and `check` reports no
`CenterlineOutside` on it; the sandbox case at profile 90 has none either.
A 2 m leg at 80° in that section is not a shaft (`riseRatio < 1`); a 10 m
leg at 80° in a `U+D = 6, L+R = 1` fissure is not (`thinness < 1`); the
same leg with `shaftThinnessRatio = 0` is. A level 2 m leg between `L = R
= 5` stations has `e = 0` and no split (I01). `shaftRingsSquare = false`
with `padScale` set to recover HEAD's pad (`0.5/slope` is not a constant,
so test with a plumb leg at `padScale = 0.5`) reproduces HEAD's ring
centers. Re-record `kUnionOpenCases`, `kPhakeCaveIssues`,
`kPinchedBendCases`, `kUnionOpenRandomSeeds` and `kDegenerateWildSurveys`
to what the shaft rule leaves; the commit body lists each entry that
moved and why. The Phake Cave catalog copy in `cwPassageTubeCases.cpp` is
the only survey data checked in; nothing from any other project.

**Starting point.** The working tree holds an uncommitted first cut of the
builder half (`git diff cavewherelib/src/cwPassageTubeBuilder.*`:
`Options` fields, `LegPad` fields, `shaftSection`, the dome loop, the
tangent parameter on `appendRing`); the gallery model and QML are
untouched. Keep what is right, fix what is not, and own the whole diff.

**Done when** the sandbox case at profile 90 shows a 2 × 2 m round shaft
standing on J with a dome at the top and no `CenterlineOutside`, the five
knobs change the build live in the gallery, the ring report shows the
ratios, the tests above pass, and the `[PassageTube]` suite passes with
the re-recorded lists.

**Commit subject.** `Draw steep legs as round shafts with domed caps`

---

### T07ab — Keep the gallery responsive with rings on for a large project

**Goal.** With Rings or Sections on, the gallery crawls on a large
project (hundreds of stations, thousands of rings): orbiting stutters
and toggles take seconds, while the same project with the overlays off
is smooth. Nothing in the overlay path is written for size: the ring
plot is one `cwRenderLinePlot` with 16 loop segments plus 2 frame
segments plus 16 section segments per ring, registered with the scene's
intersecter like the centerline, and `ringAt` walks every ring on every
pointer move.

**Read.** Code: `RingGeometry`, `ringGeometry`, `pushToScene`,
`applyRingVisibility`, `nearestRingIndex`, `ringAt`, `nearestRing`,
`ringLine` in `cwPassageTubeGalleryModel.cpp`; `cwRenderLinePlot::
setGeometry` (`registerPickable`, the sequential index list) and
`setRangeVisible` (copies the whole mask per call);
`cwRHILinePlot::updateVisibilityBuffer` (gated on the store version);
`cwRenderObject::registerPickable` and the intersecter it feeds;
`ringHoverId` in `TubeGalleryWindow.qml`. Research §4.2.

**Modify.** `cwPassageTubeGalleryModel.h/.cpp`, `cwRenderLinePlot.h/.cpp`,
`TubeGalleryWindow.qml`, `test_cwPassageTubeGalleryModel.cpp`,
`test_cwRenderLinePlot.cpp`.

**Spec.**

1. **Measure before fixing.** Build a large survey in the gallery
   (`addRandom` with a seed and 300 junctions, or `--project` on a real
   one; keep project paths out of source) and time, with
   `QElapsedTimer` in a scratch test or `qDebug` lines you remove
   before committing: `ringGeometry`; `m_ringPlot->setGeometry`
   including the intersecter registration until its pick-ready future
   finishes; the five `setRangeVisible` calls of `applyRingVisibility`;
   one `ringAt` call; and the frame time while orbiting with the
   overlays on and off (the viewer's frame counter, or Instruments).
   Put the numbers in the commit body. The suspects, in the order the
   author expects them to matter: the intersecter's sub-BVH build over
   a million segments on every `pushToScene` (and the picks the turn
   table makes against it), `ringAt`'s linear scan with a matrix
   multiply per ring on every pointer move, `setRangeVisible` copying
   and publishing a multi-megabyte mask five times per toggle, and the
   GPU drawing a million 1 px quads under 4× MSAA.
2. **Fix what the numbers show**, within these bounds:
   - The ring plot is never picked, so it stays out of the intersecter:
     `cwRenderLinePlot` gets `setPickable(bool)` (default true, so
     CaveWhere's centerline is unchanged) and `setGeometry` skips
     `registerPickable` (and unregisters) when it is false. The gallery
     sets it false on the ring plot.
   - `ringAt` runs at most once per frame: the hover handler records
     the point and a `Qt.callLater`/frame-synchronized call does the
     lookup; `nearestRingIndex` tests the ring's bounding sphere against
     the pointer ray in model space before projecting, or indexes ring
     centers in a uniform grid built once per `pushToScene`, whichever
     the measurement needs.
   - `applyRingVisibility` builds the mask once and publishes once:
     `cwRenderLinePlot::setRangesVisible(QVector<RangeVisibility>)`,
     with `setRangeVisible` calling it with one range.
   - Overlay geometry for a layer that is off is still built (toggles
     stay instant); only its visibility changes.
3. **Done when** is measured on the survey from step 1, numbers in the
   commit body: toggling Rings or Sections after the build takes under
   100 ms; orbiting with Rings, Frames and Sections on holds the frame
   rate the overlays-off view has within 20%; `ringAt` under 2 ms.

**Done when.**
- A `cwRenderLinePlot` test: a plot with `setPickable(false)` registers
  nothing with the scene's intersecter (no object under its id) and a
  pickable one does; `setRangesVisible` with three ranges produces the
  same mask as three `setRangeVisible` calls.
- A gallery-model test on a random survey with 50 junctions checks
  `ringAt` finds the ring under a projected center and returns −1 off
  all rings, as before.
- The measurements of step 3 are in the commit body.
- Every `[PassageTube]` test passes.

**Commit subject.** `Keep the tube gallery responsive with its overlays
on`.

---


### T07ah — Legs are lofts between their station sections, stations are hulls, no elbows

**Goal.** After T07af the Phake pit a6 → a9 seen in profile from
azimuth 54° is a question mark (probe of Oct 15,
`tmp/probe30/pit-profile.svg`, `cones-pit.svg`, `rec-pit.svg`,
`plumb-options.svg`; the numbers below are from
`tmp/probe30/probe-phake.json`). Two things make it one:

- The a6 corner is a 90° swept elbow: nine elbow rings run from 4.3 m
  before a6 on the a4 → a6 leg to 4.2 m below it, with radii up to
  5.8 m (the middle ring carries a6's `L5` and the clamped 23 m
  down), reaching 4.4 m to the screen left of the shot where the
  shaft's readings put the wall 1.1 m away.
- The shaft's rings are circles of radius `mean(L, R)` centered on the
  shot (`fitSteepRing`): 2.83 m at the first shaft ring where a6 reads
  `L1 R5`, so the shaft sits 1.5 m too far left and 1.6 m too narrow
  on the right, and converges on the readings only near a9 (`L2.3
  R2`).

The same mechanism inflates every corner: an elbow ring is a station
section swept around the bend from a center off the station, and
T07ad/T07af/T07s/T07t/T07r/T07v exist to fit that swept surface back
onto the legs. The elbows are the model's main source of surface no
reading supports.

**Decision (made Oct 15, after `smoothing.svg`, `rec-plan.svg` and
`plumb-options.svg`).**

1. **A leg is a loft.** Each end of a leg has a *section*: the
   quad-ellipse through its record's reading points — left and right
   along the record's facing (`−R`/`R` of the facing, T07c), up and
   down plumb, **at face value** (a station that reads `down 23` draws
   23 m of passage below it; nothing clamps a pit or aven reading).
   Every ring between the stations is the point-wise lerp of the two
   sections, bin `k` to bin `k`, at the ring's fraction along the leg.
   With LRUD only that makes each leg a cone between its sections;
   T08–T10 later move individual section and ring points for splays and
   scraps, and the loft follows. There is no frame transport along a
   chain and no square-to-the-leg ring: a sloped leg's rings are lerps
   of two plumb sections, so T07h's foreshortening goes too. A record
   whose facing lies more than `kMaxFacingDegrees` (60°, T07ag) off the
   leg (a hairpin's chord) uses the section square to the leg instead.
2. **A plumb leg's section is horizontal**: left and right along the
   facing's sides, front and back along `±facing` at `mean(L, R)`
   (option (a) of `plumb-options.svg`: it is the only rule that stays
   inside what was read and exists at every plumb station; `max`
   inflates one wide reading all round, the neighbor hull degenerates
   whenever the neighbor shares the facing). The record's up and down
   on a plumb leg are the shaft's own extent and shape nothing.
3. **A station is where lofts meet, nothing more.** When the legs at a
   station have the same section (one record, one facing — every Phake
   two-leg station, case 30's s1 and s2) the lofts share it and meet in
   a crease: outside corner and inside corner are the reading points
   themselves. When the sections differ (a4's two records; a6, where a
   gentle slot section meets the plumb horizontal one) the station is a
   junction and the **convex hull of all its sections** caps the
   wedge between them; each loft still ends on its own section. **No
   smoothing of any kind** — no elbow rings, no fillets, no averaged
   normals: `smoothing.svg` shows that any rounding either cuts off a
   reading point (outside) or adds passage nothing measured (inside);
   the smooth transitions come from scrap interpolation in T09/T10.
4. **One record per leg end** (T07ae's rule, folded in here): a leg's
   end section comes from the record of the leg's own chunk; a record
   that faces another passage belongs to that passage's leg and gives
   that station a second section (rule 3). `widestReadings` over a
   station's records is gone.
5. **Chains, junctions, hairpins and caps are unchanged** in what they
   decide: the through pair, arms zipped into the through (T07d–T07o),
   chain breaks at hairpins with domes (T07p). What changes is that a
   chain's rings at a station are its section(s), with the hull cap of
   rule 3 where they differ.

**Read.** Research §3.2.1 (*Facing* and *Shaft readings*), §3.3.2, §3.6
rows *Question-mark pit and inflated corners*, *Leg wider than the line
between its readings*, *Fat, skinny, fat between a3 and a4*; in
`cwPassageTubeBuilder.cpp`: `LrudRecord`, `lerpRecord`, `lrudAcross`,
`WalkedLeg`, `chainSamples` (the `SharpBend`/`elbowSamples` branch),
`chainFrames`/`spreadRoll`/`steepCornerFrame`, `fitSteepRing`,
`fitLrudRing`, `LegTube`, `CornerWalls`, `ringOnWalls`,
`fitElbowsToLegWalls`, `blendElbowRings`, `fitSharpBends`,
`reshapeInside`, `insideWallShift`, `stretchAcrossMiter`, `Ring`
(`isElbow`, `stationOffset`, `section`) and `MeshWriter`;
`cwPassageTubeJunction.cpp`'s hull and mouth code; every elbow rule in
`cwPassageTubeMeshCheck.cpp`; the gallery's elbow layer and report
(`cwPassageTubeGalleryModel`); `phakeCave()` and case 30 in
`cwPassageTubeCases.cpp`; the T07ag, T07ad, T07af, T07ac, T07h, T07j,
T07r, T07s and T07v tests in `test_cwPassageTubeBuilder.cpp`.

**Split (Oct 15).** The first run of T07ah as one task ran for 75
minutes, touched 14 files (+1762/−4334) and was stopped while chasing
seed failures in the junction, the checker and the mesher at once
(`tmp/t07ah-monolith.patch` keeps that diff for reference; nothing of
it is in the tree). The work is now four tasks, each with its own
reproduce test and commit, run in order: **T07ah1** (lofts, deletions)
→ **T07ah2** (plumb section shape) → **T07ah3** (hull caps) →
**T07ah4** (checker, catalog baseline, gallery). Every sub-task reads
this section first for the goal and rules; a sub-task changes only
what its own Spec names and leaves the rest of the rules to the later
ones, even where it sees the interim result is imperfect.

---



### T07ah3 — Stand a steep leg's rings vertical along its plan direction

**History.** Specified three times. (1) A convex-hull cap between a
station's differing sections — stopped: at a6 the slot and shaft lofts
interpenetrate and no cap removes that. (2) Vertical end sections
offset `∓ mean(L, R)/2` along the facing (`wf_fe5f6bba-8fe`, Oct 18,
`tmp/t07ah3-vertical-attempt.patch`) — not committed: a section is a
plane, so the offsets made a tilted slab with the stations outside it
(`CenterlineOutside`, arm mouths could not zip, catalog 8/9/10/23 and
*joins a passage to a shaft* regressed), consecutive plumb legs
zigzagged, and an L/R-only pit lofted into a 3 m ribbon. (3) This
spec, from `tmp/probe30/pit-vrings.svg` (Oct 19), approved by the user:
**rings stay vertical**, and a plumb leg's rings stand along its facing.

**Goal.** A pit's readings describe the pit between its stations: at
the lip p1 reads `U2 D19`, at the landing p2 `U19 D1`; p1's floor is
p2's floor and p1's ceiling p2's ceiling. A steep leg's tube therefore
runs along its **plan direction** with **vertical rings** square to
it, and the loft is the pit box: vertical end walls, a floor from
`start − D` to `end − D`, a ceiling from `start + U` to `end + U`,
left/right across. T07ah2's level sections throw the up and down away.
A plumb leg has no plan run, so its rings stand along the start
record's facing, each the outline of the whole sheet, giving the shaft
a `mean(L, R)` depth with the stations inside.

**Rule.** For a steep leg (`isSteep`, more than `kSteepLegDegrees` from
level) with end sections `S(0)` and `S(1)` (vertical quad-ellipses
through the records' reading points, left/right along the record's
facing, up/down plumb at face value — the same section a gentle leg
draws, with the `kMaxFacingDegrees` fallback left to gentle legs):
- `d` = the leg's plan direction; when the plan run is under
  `kMinRadiusMeters`, the start record's facing. `pad(t)` =
  `lerp(mean(L, R) at start, mean(L, R) at end, t) / 2`.
- `e = 1 − min(1, planRun / (pad(0) + pad(1)))`. Ring positions run
  along `d` from `plan(start) − e · pad(0)` to `plan(end) + e · pad(1)`
  at the chain's ring spacing, both end positions included.
- The ring at plan position `s` lies in the vertical plane square to
  `d` through `s`; it is the convex outline, in that plane, of every
  lerped section `S(t)` whose plan position `plan(t)` satisfies
  `|plan(t) − s| ≤ e · pad(t)`, each `S(t)` drawn in the plane as the
  lerp of the end sections' `(across, up)` coordinates with its center
  at `lerp(start, end, t)`'s height. When the window is a single `t`
  (every leg with `e = 0`), the ring is `S(t)` itself. The outline is
  the 2D convex hull of the sections' points, resampled at the bin
  directions around the window's mid-height on the station line.
- The ring's center is the point on `d` at `s` at the window's
  mid-height; `radii`/`binAngles` derive from the vertices as T07ah1
  set up; `right` is `d`'s clockwise, `up` is `kWorldUp`.
- A missing up or down on a steep leg comes from the leg before the
  default applies: the upper station's missing down is the drop plus
  the lower's down (default when that is missing too); the lower
  station's missing up is the rise plus the upper's up.

Consequences: a 70° pitch (`e = 0`) is the loft of its two vertical
sections — p1's section is its first ring, so the chain from the
gentle leg is seamless; a plumb leg (`e = 1`) is a box `pad(0) +
pad(1)` deep whose every ring is `hull(S(0), S(1))`, the stations
inside it, consecutive plumb legs stacking without a zigzag; a
plumb pit read only L/R is a full-height box. At a6 the slot's end
ring (across 147°) and the shaft's first ring (across 230°) differ, so
the station holds both, stitched, as every two-record station does;
their crossings are `PassageOverlap` after T07ah4.

**Read.** The parent `### T07ah` section (rules 1, 2 and 5); in
`cwPassageTubeBuilder.cpp`: `isSteep`, `kSteepLegDegrees`,
`SectionAxes`, `sectionAxes` (both branches), `sectionPoints`,
`drawnWalls`, `filledReadings`, `meanReading`, `LegEnd` (`isSteep`),
`borrowWalls` (the steep branch T07ah2 added), the `previousRight`
threading near line 1104, how a leg's rings between its stations are
placed (the lerp and ring spacing), `Ring` and `setRingVertices`/
`placeChainVertices` in `cwPassageTubeGeometry.h`, `MeshWriter`'s end
caps (`endOffset`, `pitOffset`); in `cwPassageTubeMeshCheck.cpp`:
`readingPoints`/`wallsAcross`/`legSection` and `isPit`;
`test_cwPassageTubeBuilder.cpp`: *lofts a plumb leg between its
horizontal sections* (T07ah2), *fits a pit from its own left and
right*, *places pit dead-end caps by up and down*, *keeps frames
continuous through a vertical pit*, *keeps a chain that is vertical
throughout finite*, *falls back to the next station's facing on a
vertical shot*; `test_cwPassageTubeJunction.cpp`: *joins a passage to a
shaft*, *lands a pit on the passage floor*; `tmp/probe30/pit_vrings.py`
(`steep_rings`, `hull2d`, `resample`, `fill_steep`) for the model;
Phake a6 → a9 and a9 → a10 in `cwPassageTubeCases.cpp`.

**Modify.** `test_cwPassageTubeBuilder.cpp` first; then
`cwPassageTubeBuilder.cpp` (`.h` only for a constant),
`cwPassageTubeGeometry.h` if the hull helper belongs there,
`cwPassageTubeMeshCheck.cpp` only as far as step 3 says,
`test_cwPassageTubeJunction.cpp` and `test_cwPassageTubeCases.cpp`'s
lists for fallout.

**Spec.**

1. **Reproduce** (`[!mayfail]` until step 2; the tests own a
   `verticalSection(station, record, facing)` helper and a 2D hull):
   - `cwPassageTubeBuilder lofts a steep leg between its vertical
     sections`: one chunk p0 (0,0,0) `L1.5 R1.5 U2 D1`, p1 (10,0,0)
     `L1.5 R1.5 U2 D19`, p2 (16.84, 0, −18.79) `L1.5 R1.5 U19 D1`, p3
     (26.84, 0, −18.79) `L1.5 R1.5 U2 D1`, every record facing +x. For
     every ring on p1 → p2 at fraction `t` of the plan run: every
     vertex within `kWallTolerance` of `lerp(S(0)[k], S(1)[k], t)`;
     the ring at p1 is p1's section; the highest vertex of the ring at
     `t` is at `z = lerp(2, 0.21, t)` and the lowest at
     `lerp(−19, −19.79, t)`. Named regression: today p1's ring is level,
     3 m across, every vertex at `z = 0`; the test prints the highest
     and lowest vertex of the first and last ring.
   - `cwPassageTubeBuilder stands a plumb leg's rings along its facing`:
     Phake a6 → a9 (facing 230° at both; a6 `L1 R5 U2 D1.2`, a9 `L2.3
     R2 U24 D0.05`). Expected: every ring of the leg lies in a vertical
     plane square to the facing; the rings' plan positions along the
     facing span `−1.5` (= `mean(1, 5)/2` behind a6) to `+1.075` (=
     `mean(2.3, 2)/2` beyond) within `kWallTolerance`, with a ring at
     each end; every ring is the hull of a6's and a9's vertical
     sections: its highest vertex at `z = a6 + 2 = −4.5`, lowest at
     `a9 − 0.05 = −29.99`, its left/right extremes within
     `kWallTolerance` of the hull's L and R edges (a6 L → a9 L, a6 R →
     a9 R); a6 and a9 both lie inside the box (between the first and
     last ring's planes and inside their outlines). Named regression:
     today every ring is level at its station's height, and the test
     prints the first ring's plane normal, its highest and lowest
     vertex. Replaces T07ah2's *lofts a plumb leg between its
     horizontal sections*.
   - `cwPassageTubeBuilder fills a steep leg's missing up and down from
     the leg`: the existing *fits a pit from its own left and right*
     survey (top (0,0,0) and bottom (0,0,−12), `L1.5 R1.5` only, 12 m
     plumb): the mesh's highest vertex is at `z = 0 +
     defaultRadius`, its lowest at `z = −12 − defaultRadius`, every
     ring 3 m across and 1.5 m deep (`mean(1.5, 1.5)/2` each way) within
     `kWallTolerance`, the mesh clean (`checkMeshClean`), no
     NoPassageData warning. Rewrite *fits a pit from its own left and
     right* into this test (its flat-cap-at-z=0 check goes: the box
     reaches above the station).
2. **Builder.** Implement the Rule. Delete the level-section branch of
   `sectionAxes`, `borrowWalls`' steep branch, the `previousRight`
   threading, `LegEnd::isSteep` if nothing reads it, and `MeshWriter`'s
   `pitOffset` if no ring has a steep tangent any more (say so in the
   commit body). The hull is a monotone chain over at most two
   sections' points — keep it small, in the builder or
   `cwPassageTubeGeometry.h`. Rewrite the four older steep tests named
   in Read to the new rings, keeping their intent (*keeps a chain that
   is vertical throughout finite*: three stacked plumb legs give three
   boxes centered on the station line, no zigzag, `checkMeshClean`;
   *places pit dead-end caps by up and down*: highest/lowest z and a
   clean mesh; *falls back to the next station's facing on a vertical
   shot*: the vertical shot's rings stand along the borrowed facing;
   *keeps frames continuous through a vertical pit*: the pit's rings
   share one plane normal). Drop `[!mayfail]`.
3. **Checker.** `legSection`/`isPit` already leave steep legs out of
   the side-wall lines; keep that and fix the stale `isSteep` comment.
   No other checker change.
4. **Fallout.** *joins a passage to a shaft* and *lands a pit on the
   passage floor* must stay hard tests: update their expected numbers
   to the box (an arm now meets a box face, so its mouth should zip;
   if it cannot, say why in the commit body and stop rather than
   demote the test). Update `test_cwPassageTubeCases.cpp`'s lists only
   for cases whose counts changed and list each in the commit body.

**Done when.**
- Step 1's three tests pass as hard tests with the named regressions.
- `tmp/probe30/pit_profile.py` on a rebuilt Release2 probe shows the
  a6 → a9 rings as vertical outlines whose tops lie at −4.5 and
  bottoms at −29.99 with the left/right extremes on the cone lines,
  and a plan grab of Phake `--station a6 --rings --sections` shows the
  shaft box 2.6 m deep along 230° with a6 inside it.
- No level section remains in the builder; *joins a passage to a
  shaft* is a hard test.
- Every `[PassageTube]` C++ test passes.

**Commit subject.** `Stand a steep leg's rings vertical along its plan
direction`.

---



### T07ah4 — Check lofts and hulls, record the catalog baseline, finish the gallery

**Read.** The parent `### T07ah` section; every remaining rule in
`cwPassageTubeMeshCheck.cpp` (`readingPoints`, `wallsAcross`,
`WallBeyondReading`, `CenterlineOutside`, `legSection`); the deferred
finding *T07ag review (confidence 40)* at the end of this file; the
gallery's ring and section layers and report
(`cwPassageTubeGalleryModel`); `test_cwPassageTubeMeshCheck.cpp`,
`test_cwPassageTubeGalleryModel.cpp`.

**Modify.** `test_cwPassageTubeMeshCheck.cpp` and
`test_cwPassageTubeBuilder.cpp` first; then `cwPassageTubeMeshCheck.h/
.cpp`, `cwPassageTubeGeometry.h` (shared section helpers), the gallery
model and its test.

**Spec.**

1. **Reproduce** (`[!mayfail]` until step 2):
   - `cwPassageTubeMeshCheck reports a vertex off its leg's loft`: a
     hand-built mesh for case 30 with one s1 → s2 vertex pushed 0.5 m
     off `lerp(S1[k], S2[k], t)` is reported, the unmoved mesh is
     clean.
   - `cwPassageTubeMeshCheck reports a vertex outside its station's
     hull`: a vertex at Phake a6 pushed 0.5 m outside the hull of a6's
     sections is reported, the unmoved mesh is clean.
   - `cwPassageTubeBuilder keeps each catalog case's issue count`:
     record every catalog case's, sweep case's and Phake's issue counts
     after T07ah3 as the baseline the test asserts.
2. **Checker.** Replace what is left of the elbow and corner rules
   with two: a vertex on a leg lies within `kWallTolerance` of the loft
   at its fraction; a vertex at a station lies within tolerance of the
   hull of the station's sections. The checker builds sections with
   the same helper the builder uses (move it to `cwPassageTubeGeometry.h`),
   which closes the *T07ag review (confidence 40)* finding (`readingPoints`
   vs `sideWallPoints`); delete `readingPoints`/`wallsAcross`/`legSection`
   where the loft rule subsumes them. `WallBeyondReading` and
   `CenterlineOutside` stay as written against these surfaces. Drop
   `[!mayfail]`.
3. **Overlap, not self-intersection (option (a), Oct 15).** A crossing
   between triangles of two different legs of one chain, neither in a
   zip band, is a `PassageOverlap` like a crossing between two chains:
   a pit's slot (`D23` on a4 → a6) and its shaft (a6 → a9) are two lofts
   of one void and T07x's union is what resolves them. Add a test with
   Phake a6: the crossings there are warnings, not issues, and the
   two-section stations leave the open lists in
   `test_cwPassageTubeCases.cpp`.
4. **Gallery and dead code.** Remove every elbow/corner remnant in the
   gallery report and layers; keep rings and sections. Sweep the
   builder, junction and geometry headers for helpers nothing calls
   after T07ah1–3.

**Done when.**
- Step 1's three tests pass as hard tests.
- `grep -n 'elbow\|Elbow\|inset' cavewherelib/src/cwPassageTube*` finds
  only comments that explain history, or nothing.
- Every `[PassageTube]` C++ test and the QML suite pass.

**Commit subject.** `Check lofts and station hulls in the mesh check`.

---

### T07ae — A leg and its end station share a record

**Folded into T07ah (Oct 15), rule 4.** Kept for its probe numbers.

**Goal.** Phake a4 is read in two chunks: `L0 R2 U2 D1.7` in a1–a5
(its facing continues toward a5) and `L5 R0 U3 D23` in a4–a6 (facing
a6). The chain a1 → … → a4 → a6 → … (probe of Oct 14,
`plans/TUBE_CORNER_FOLLOWUPS.html` fig. 3) interpolates the a3 → a4
leg toward the *first* record — the left wall runs 1.5 → 0.87 → 0.71
→ 0.58 → 0.51 at the inset ring — while the a4 station ring takes
`widestReadings` over both, `L = 5`, and since the largest reading
sets the elbow's inset (3.54 m) the elbow climbs 0.51 → 1.58 → 2.61 →
3.14 → 3.60 → 3.96 → 4.21 over the last 3.5 m of the leg: fat at a3,
skinny mid-leg, fat at a4. The merged record also feeds
`largestReading` and the a4 → a6 leg's start. This is the deferred
T07z/T07v a4 finding.

**Decision (made Oct 15).** A chain leg and the ring at its end use
the station record whose facing lies along that leg; a station ring
between two chain legs merges only the records those two legs chose.
A record that faces another passage belongs to that passage: a4's
`L5 R0 U3 D23` shows on a4 → a6 (and that arm's mouth), and only
there.

**Read.** Research §1.1 (LRUD per station and chunk), §3.2
(cross-section sources), §3.6 row *Fat, skinny, fat between a3 and
a4*; `Leg` (`fromPassageLrud`,
`toPassageLrud`, `fromFacing`, `toFacing`) and `chunkFacings` from
T07c, `walkedLeg`, `ringLrud`, `chainSamples`' station samples
(`endRecords`), `widestReadings`, `largestReading` and
`cwPassageTubeJunction`'s mouth readings in `cwPassageTubeBuilder.cpp`
/ `cwPassageTubeJunction.cpp`; `phakeCave()` and the multi-trip cases
in `cwPassageTubeCases.cpp`.

**Modify.** `cwPassageTubeCases.cpp`, `test_cwPassageTubeBuilder.cpp`,
then `cwPassageTubeBuilder.cpp` (and `cwPassageTubeJunction.cpp` if the
mouth reads the merged record).

**Spec.**

1. **Reproduce.** Catalog case `31. Junction station read once per
   chunk`: a level T — p0 → p1 → p2 straight (10 m legs) with p1 read
   `{1, 1, 1, 1}` in that chunk, and a second chunk p1 → p3 leaving p1
   at 90° with p1 read `{4, 0.5, 2, 1}` there, p3 `{4, 0.5, 2, 1}`.
   Builder test `cwPassageTubeBuilder reads a station once per leg`:
   on the p0 → p2 chain every ring's side walls lie within
   `kWallTolerance` of 1 m (p1's own-chunk reading), and on the p1 →
   p3 arm the first ring's left wall is 4 m. Also assert on Phake
   a1 → a4 that the left wall decreases monotonically from a3's 1.5 to
   `kMinRadiusMeters` at a4. `[!mayfail]` until step 2.
2. **One record per leg end.** Where the builder gathers a station's
   records for a chain leg (`walkedLeg` → `startRing`/`endRing`, the
   station sample's `endRecords`, `largestReading` for the inset), it
   takes the record of the chunk the leg belongs to (`Leg.chunkIndex`
   / the leg's own `fromPassageLrud`/`toPassageLrud`), and the station
   sample between two chain legs merges those two legs' records with
   `widestReadings` as before. A station read in a chunk but with no
   leg of that chunk on the chain contributes nothing to the chain.
   `ringLrud`'s dropping of along-leg up/down stays. The junction's
   mouth for an arm reads the arm's own chunk record. Remove
   `[!mayfail]`.
3. **Fallout.** Catalog, Phake Cave and random seeds: list every case
   whose issue counts change in the commit body; `WallBeyondReading`
   stays clean. Note in the body what a4's `D = 23` now does (the pit
   reading on a4 → a6).

**Done when.**
- The step 1 test passes as a hard test; Phake a3 → a4's left wall is
  monotone 1.5 → 0.05 and the a4 elbow's inset comes from the a1–a5
  record.
- Before/after grabs of Phake a4 (`--station a4 --rings --sections
  --translucent`) in the commit body.
- Every `[PassageTube]` test passes.

**Commit subject.** `Read a station once per leg of the chain`.

---

## Deferred review findings

Findings the workflow's review raised that no task owns yet. Each
names the task that should pick it up, or `—` for a new one.

- **T07v (confidence 70).** The checker's steep-leg section in
  `wallReaches` uses the station's *widest* side reading for the round
  section across a leg within 45° of plumb, while `fitSteepRing` fits
  the *mean* of left and right; the rule can under-flag a pit lip whose
  ring is wider than the mean. Owner: a checker pass after T07ac.
- **T07w.** The gallery's project rows read from a `QTemporaryDir`
  copy whose lifetime is the `Row`'s while the child `cwProject` is
  `deleteLater`'d; a row removed in the same event loop turn as the
  project's last save could leave the project writing into a removed
  directory. Owner: T07ab (it touches the gallery model's lifetimes).
- **T07z/T07v, a4.** `widestReadings` merges a4's two records
  (`{0, 2, 2, 1.7}` on a3–a5 and `{5, 0, 3, 23}` on a4–a6) into
  `L = 5.93`, so the a4 elbow balloons to 6.96 m on the left. Which
  record a corner ring should take (the chain's own leg's, the widest,
  or a per-leg pair) is open; T07ac's face-value fit makes the choice
  visible. Owner: T07ae (the chain's own leg's record).
- **T07o.** `kUnzippedSweepCases` (19 families) and the gallery 18/19
  hard reproduction test are to be recounted and rewritten when steps
  4–5 land (in T07o's status above).
- **T07s.** Case 25's frame roll after a shaft (`chainFrames`) — landed
  as T07af (`d6dc315b`); T07s's step 2 is down to Phake a9 (its status).
- **T07af review (confidence 45).** T07af removed the post-run twist
  distribution and spreads roll only along runs of *round* rings
  (`hasRoundRings`, weight > 0.95). A steep run that is not round
  (60°–72°, weight between `kSteepWeight` and 0.95) now hands its
  blend angle back to the horizontal up as the weight falls, with no
  distribution; a tight 70° helix of ~1 m radius that levels out could
  roll more than 90° between samples. No case exercises it. Owner: —
  (a catalog case with a steep helical incline, checked with the
  T07h/T07j `right_i · right_{i−1} > 0` assertion).
- **T07ag review (confidence 40).** `cwPassageTubeMeshCheck.cpp` `readingPoints` draws no side-wall line for a record whose facing is zero or exactly square to the leg (`wallsAcross` reads no side walls there), while the builder's `sideWallPoints` falls back to the leg's own right at face value; the two disagree only in that degenerate case, where `WallBeyondReading` measures against the face-value interpolation. Owner: T07ah replaces both with the loft rule; if a mesh-check case with a zero facing shows up before then, align `readingPoints` with `sideWallPoints`.
- **T07ah1 review (confidence 45).** `cwPassageTubeGeometry.h` `setRingVertices` clamps a bin's angle to the previous bin's when its projected angle falls behind but keeps the bin's own radius, and `placeChainVertices` rebuilds vertices from (angle, radius, offsetAlong), so a clamped bin lands off the point-wise lerp. The probe found every interior ring of case 30 and Phake a1 → a4 on the cone edges (0.000 m, 0.015 m on a2 → a3), so no visible effect yet. Owner: T07ah4 (store loft vertices directly).
- **T07ah2 review (confidence 45).** `cwPassageTubeBuilder.cpp` `borrowWalls`: a steep end that borrows walls keeps the lender's left and right, but those sit in the lender's section frame, which `facesAgainst`/`turnsAgainst` may have swapped; the borrower applies them along its own right, so the walls can come out mirrored when the two rights point opposite ways. Needs a synthetic test (a plumb leg with only up/down readings borrowing from a gentle leg walked against its facing). Owner: T07ah4.
