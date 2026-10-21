# Tube rendering redesign

Started Oct 19, 2026 after T07ah3 (vertical rings on steep legs) stopped at
the junction code: the junction and chain machinery assume rings advance
along a leg, and a steep leg's rings advance across it. Rather than teach the
junction about boxes, we are rebuilding the tube rule from a 2D view, plan
first, one step at a time, with the goal of one rule and no special cases.

Each step records the decision, the reasoning, and the drawing that settled
it. Prototype drawings live in `/Users/cave/.claude/jobs/ab1fa6ed/tmp/probe31/`
(`sweep_plan.py`, `sweep_plan_miter.py`, `junction_*.py`, `notch*.py`,
`sweep_profile*.py`, `pit_profile.py`); they stay out of the repo.

Status of the branch at the start: HEAD `8001628b`, with the uncommitted
T07ah3 attempt saved as `tmp/T07ah3-fix1-start.patch`.

## Step 1 — a plain chain in plan: the miter rule (decided Oct 19)

Three stations 1 → 2 → 3, leg 1→2 bearing 0, leg 2→3 sweeping θ from 0 to
180°. The question was where L and R sit at station 2.

### What the code did at HEAD

- A station faces the horizontal chord between its chunk neighbors
  (`chunkFacings`). Station 2 faces 1→3.
- A gentle leg end's right is the facing turned clockwise, so the ring at 2
  lies in the miter plane and L/R sit at face value in it (`sectionAxes`).
  Both legs read the same axes, so station 2 holds one ring and each leg
  lofts point-wise from its square end ring to it. The width measured
  square to the leg at the station is `(L+R)·cos(θ/2)`: a "modified miter"
  with face-value readings instead of a stretched section.
- Special case 1: when the facing lies more than `kMaxFacingDegrees` (60°)
  off a leg — bends past 120° — the leg falls back to its own square right.
  Station 2 then holds two rings, stitched (the double ring). In plan this
  is a wedge where the two square rings overlap: the inside wall is drawn
  twice and the outside wall is pulled short.
- Special case 2: a zero chord (equal legs doubling back) borrows the
  nearest neighbor's facing; `facesAgainst` then swaps L/R for the leg that
  walks against it.

### Decision

Keep the miter rule for every bend, 0 through 180°. A chain of legs is one
continuous passage and its tube stays one tube: one ring per station, every
leg lofting into it, no double ring.

```
facing(s) = normalize(d_in + d_out), or clockwiseOf(d_in) when that is zero
right(s)  = clockwiseOf(facing(s))
ring(s)   = quad-ellipse at face value: L, R along right; U, D plumb
leg       = point-wise loft between its two station rings
```

- The facing is the bisector of the unit leg directions rather than the
  chord: the chord is the bisector only for equal legs and otherwise tilts
  toward the longer leg. Same cost, independent of leg lengths.
- The zero-bisector fallback is the limit of the rule, square to the legs,
  so the facing is continuous through 180°. No neighbor borrowing.
- Deleted by this step: `kMaxFacingDegrees`, the square-to-leg fallback,
  the double ring and `isSameSection`, and `facesAgainst` for the chain case.

### The hairpin pinch, accepted

The across-leg width at a station is `(L+R)·cos(θ/2)`: full at a straight
station, half at 120°, zero at 180° where the ring lies along the
centerline and the tube passes the station as a sheet of height U+D and no
thickness. The pinch is that collapse, visible past roughly 150°.

Accepted as is, with no floor on the width. A tight azimuth reversal in real
data is either a steep leg (its rings come from the vertical-section rule,
so the plan pinch is irrelevant) or a traverse across a room with no inner
wall to get wrong; the lofted sheet sits inside the room's other legs. When
it matters, the fix is a survey one: a station at the apex.

One amendment from step 3: the ring keeps `kMinRadiusMeters` rather than
collapsing to zero width, so the mesh stays a valid boolean input.

## Step 2 — a junction is a union (decided Oct 19)

Three legs at J: through chain 1 → J → 3 and an arm J → 4, over a range of
through bends and arm angles. Drawings: `junction_plan.py`,
`junction_corners.py`, `junction_failures.py`, `junction_rays.py`,
`junction_zoom.py`, `junction_union.py` and their PNGs in probe31.

### What the code did at HEAD

- Each chunk has its own record of J and its own facing there
  (`chunkFacings` is per chunk). The through chunk reads J facing the
  bisector of its legs; the arm's chunk reads J facing along the arm.
- The through's ring at J ignores the arm. The arm's ring at J is square to
  the arm and centered on J, so it lies inside the through where its
  readings describe nothing.
- `trimArm` walks the arm's rings out from J and drops those inside the
  through; `zipWindows` cuts a window of (ring, column) cells in the
  through's wall around the arm's cut edge and stitches the two with a band
  of triangles. Where the arm's edge pokes outside the through the band
  folds out (the mouth notch); where it is inside, the band pulls in.
- Which chain is "the through" is a data accident: whichever chunk the
  survey continued in.

### Considered and dropped

**Shared corners (the n-leg miter).** Legs at J sorted by bearing; one
corner per wedge between angular neighbors, on the wedge's bisector; each
leg's ring at J runs corner to corner so neighbors share an edge. Two
placements: (a) at face value, the mean of the two readings that claim the
corner; (b) where the two offset walls meet, `reading / sin(wedge/2)`,
capped. (a) was chosen over (b) for consistency with step 1, and then its
failures were drawn (`junction-failures.png`):

1. every arm necks to `sin(wedge/2)` of its width at the mouth (59% at a
   T);
2. a narrow wedge makes a throat at J;
3. legs a few degrees apart share a knife edge;
4. a hub station becomes a pinched star;
5. a reading into an opening (the through's `L8` into the arm) is meaned
   with the arm's `R1.5` and both mouth corners land at 4.75 m;
6. a crawl off a chamber pulls the chamber's wall in to the mean;
7. a blank record at a tie-in sets the arm's mouth from the through alone.

Only 3 looked bad enough to need a rule. The rest were accepted, until the
union made them moot.

**The ray rule.** A reading is a ray from J (`L` at facing − 90°, `R` at
facing + 90°, length the reading); a ray belongs to the wedge its bearing
falls in; a ray pointing down another chunk's leg is a reading into an
opening and is dropped. A ray is exactly a splay, which is the rule's
strength: real splays would join the same construction. It fails on the
arm's own readings at a tie-in, which always point along the through, and
`L8` into a side passage is the same geometry as `R4` to a chamber wall
where a crawl leaves; only a length threshold separates them, and a
threshold in the geometry is a special case. Dropped for geometry; kept as
the model for splay support.

**Trim and zip.** Deleted with the junction: all of it exists to recover a
mouth from a ring drawn in the wrong place.

### Decision

Every chain is its own capped tube by the step-1 rule, with its own records
at its stations. Chains that share a station overlap there, and the mesh is
the **union** of the tubes. There is no rule at J: no through, no arm, no
facing for a station with three neighbors, no shared corner, no mean.

`junction-union.png` shows the seven cases in plan under the union:

1. T junction: the arm leaves at its full width; the mouth is where its
   walls cross the through's wall.
2. 30° fork: the dividing wall starts where the two walls cross, 4 m out,
   both passages full width (the (b) corner, without a cap).
3. legs 4° apart: the arm lies inside the through and vanishes into it,
   showing only where it is wider or leaves vertically.
4. hub: a 6 m room at J with shallow notches at the 60° bisectors.
5. `L8` into the arm: a 16 m diamond bulge west of J with the arm at its
   tip. Drawn as the surveyor wrote it; the checker names it
   (`JunctionReadingConflict`, a warning with a ratio threshold, which is
   fine in a warning).
6. crawl off a chamber: the crawl runs inside the chamber and leaves
   through its wall at 1 m. Exactly the cave.
7. blank record at a tie-in: `borrowWalls` as today, within the arm's own
   chain.

Steep legs fall under the same decision: a plumb box is a capped tube that
overlaps whatever it meets, which is what T07ah3 could not do with trim and
zip.

The cost moves to 3D. Seen from outside, overlapping closed tubes drawn
opaque already look like the union. The zip existed for the inside view:
standing in J, the arm's capped end is a box through the through's wall and
the way into the arm is blocked. A real mesh boolean removes the interior
faces and opens the mouths. That is a library problem: `manifold`
(Apache 2.0, C++, `manifold/3.5.1` on Conan Center) or CGAL
(`cgal/5.5.2`). manifold needs closed, consistently wound inputs, which a
capped loft is.

Rings-only output stays as the overlap: gallery, checker and mesh tests run
on the raw tubes, with `PassageOverlap` the expected state. The union is a
second stage over the finished mesh, per connected component.

## Step 3 — end caps slide into what they meet (decided Oct 19)

The union's one flaw is the **end-cap notch**. A tube's cap at a shared
station is square to its own axis and centered on the station, so a cap
vertex can lie outside the other tube. In plan, the arm's right corner at a
fork is `R_arm · cos φ` from the through's centerline and `R_arm · sin φ`
behind J; when `R_arm · cos φ > R_through` it stands outside, behind the
mouth, and the union outline steps out along the cap edge before turning up
the arm's wall (`notch.png`, left column). The same on the left side, above
J, when `L_arm · cos φ > L_through`, and on the ceiling or floor when the
arm is taller. No notch at a T (`cos 90° = 0`); the notch grows as the
angle closes.

**Detector:** a cap vertex outside every other tube at its station — a
point-in-mesh test, which manifold also provides.

**Rule:** each cap vertex slides along its tube's axis, forward or back,
whichever is the shorter move that puts it inside another tube at the
station, limited to the leg's length. A vertex that cannot get inside stays
put. The cap becomes a skewed ring inside the other tube and the union
removes it; each wall then emerges exactly where it crosses the other
tube's surface.

In plan the slide is closed-form, `(R_arm · cos φ − R_through) / sin φ`:
0.6 m at 30°, 1.7 m at 15°, 5.7 m at 5° (`notch.png`, right column). A
planar slide of the whole cap is wrong — moving back fixes the right corner
and pushes the left one further out (`notch-left.png`, middle) — which is
why the rule is per vertex (`notch-left.png`, right: right corner back
0.6 m, left corner forward 1.2 m to where the arm's wall enters the
through's west wall; the left wall then crosses the through and exits its
east wall 7.7 m up, at the fork's cusp). In 3D the closed form goes and the
operation is a line along the axis through the vertex against the other
tubes' meshes, nearest inside point in either direction.

The vertex that stays put is the honest case: an arm wider or taller than
the through along its whole axis is a bigger passage, and its step is real.
The leg-length limit is data, not a constant.

The same rule opens a pit: a plumb box's top cap sits in the passage it
leaves from; if the box is wider than that passage the cap slides up, and
the union opens the hole in the floor.

The two 3D questions the shared-corner rule raised — non-planar rings at J
and one U/D profile per shared corner — vanish, since nothing at J is
shared.

## Step 4 — the union spike (done Oct 19)

Prove the one unknown before any builder work: manifold on our tubes.

1. Add `manifold/3.5.1` to `conanfile.py`.
2. Build closed tubes for: the T junction; the 30° fork with the wide arm
   (through `L2 R1`, arm `L3 R1.5`); Phake a6 (slot `D23`, the plumb shaft
   to a9, the floor `U24`) as three overlapping boxes.
3. Union them, run the mesh checker's manifold, winding, degenerate and
   sliver rules on the result, and view them in the gallery from outside
   and from inside J.
4. Then the end-cap slide on the fork, and the T from inside with the mouth
   open.

Watch for: degenerate triangles at a hairpin pinch (step 1 amendment,
`kMinRadiusMeters`), coincident faces where two chains share a leg
(`mergeRepeatedShots` keeps those out) and at a pit mouth (step 5), and
the cost of the boolean per component on the full Phake and Blankenship
meshes.

Once it holds, the deletion list: `cwPassageTubeJunction.cpp` (through,
arms, `trimArm`, `growAndTrim`, `RingTube`, `zipWindows`, `zippedWindow`,
`isZipBandManifold`, `leaveOutArm`), `JunctionUnzipped`, the zip-band
exemptions in the checker, `kMaxFacingDegrees`, the double ring and
`isSameSection`, `facesAgainst` for chains, `kOpenCatalogJunctionCases` and
`kUnzippedSweepCases` as lists (the cases stay, as union cases),
`kSteepLegDegrees` and the level section (step 5), and T07ah3 in its
current form. `PassageOverlap` becomes the expected state of the
rings-only output rather than a warning.

### Result

Commits `8677d4ce` (manifold dependency, `cwPassageTubeUnion`,
`cwPassageTubeBuilder::buildChainRings`, tests a–d), `94301677`
(`slideCaps`), `92eb59bb` (gallery Union and Slide caps toggles, `--union`,
`--slide-caps`, `--fork`, an "Add fork" button; the fork stays out of the
catalog). Grabs in `tmp/spike/`.

- manifold accepts every capped chain tube as is: `NoError` on the T, the
  fork before and after the slide, the three a6 boxes, and all 130 chain
  tubes of `random(1, 300)`. A chain tube is the builder's rings before the
  junction stage, lofted, fan-capped, winding fixed by signed volume. An
  input that fails is reported with its `Status` and left out (tested with
  an open box, `NotManifold`).
- The raw boolean output carries µm-scale slivers and specks: intersection
  points within float rounding of an input edge, and walls touching along
  a line (equal ceilings and floors at the fork, the coplanar `D0.05`
  floors at a9). manifold's `Simplify(1 mm)` and `MeshGL.tolerance` leave
  them. A cleanup pass (`removeSlivers`: collapse edges under 1 mm keeping
  the mesh manifold, flip slivers where both triangles get thicker, up to
  16 passes) clears them. After it the T, the fork, the slid fork and the
  a6 boxes are each one closed genus-0 component with no degenerate,
  sliver or self-intersecting triangles; the boxes come out at 18 m³
  exactly. `PassageOverlap` is reported on the inputs and absent on the
  union.
- `random(1, 300)`, 130 tubes in one component, is dirty because its
  inputs are: 21 of the rings-only tubes cross themselves (the folds), and
  manifold passes self-crossings through — 1508 self-intersections, 1585
  slivers, four specks and one inside-out void of −1.5 m³.
- Timings, Debug + ASAN, manifold built Debug single-threaded (upper
  bounds): T 16 ms (1000 triangles), fork 25 ms (974), a6 boxes 5 ms;
  `random(1, 300)` 6.4 s total = 0.8 s inputs + 2.9 s boolean + 0.09 s
  fetch + 2.7 s cleanup (8 passes), 111k triangles in, 121k out.
- From inside the arm looking at J, the mouth is open on the T and the
  fork. The HEAD rings-only mesh already has a zipped open mouth, so there
  is no "blocked cap" before shot.
- The fork's notch: `capNotch` finds 9 arm cap vertices outside the through
  — seven the plan notch (on the arm's `L3` side here, since the arm's
  chunk runs far → J), two exactly on the through's ceiling and floor.
  `slideCaps` moves the seven forward 1.3–3.3 m (closed form 3.2 m, slide
  3.24 m); the step is gone in plan and three-quarter views, volume
  191.1 → 195.9 m³, still one clean component. The rest margin after a
  slide is 5 cm: at 2 cm one 1.5 cm sliver stayed where the slid wall
  grazes the through; 3 cm and up are clean.

### What changes in steps 1–5

1. The union stage includes the sliver/speck cleanup after the boolean;
   manifold's own `Simplify` is insufficient.
2. Step 3 and step 5's hope — a vertex on another tube's surface counts as
   outside and slides until strictly inside — fails for equal ceilings or
   floors: slid along a level arm it never gets strictly inside (two such
   vertices on the T, two on the fork), and the union is clean anyway. The
   rule is: on the surface counts as inside, leave it. A coincidence rule
   waits for a visible artifact.
3. Inputs must not cross themselves. Step 1's single ring per station and
   step 5's vertical rings remove most of the current folds; the checker
   runs `SelfIntersection` on each chain tube before the union, and a tube
   that fails is reported.
4. A union of many passages can enclose a void: an inner shell with its
   normals facing in, a lump of rock the air surrounds on every side, which
   manifold reports as negative volume. A pillar or a loop around rock is a
   handle, not a void, and stays. Tubes only wrap a pocket when passages
   stack and overlap in 3D, so a void is an artifact of the tubes rather
   than a boulder. Decided Oct 19: drop components with negative volume
   and report them as a checker warning, `EnclosedVoid`, with volume and
   position, so a large one on a real cave gets a look.

Side findings from the full suite (tube code untouched by either):
`"Bundled save should exclude git-excluded files"` (`test_cwProject.cpp`)
hung at 0% CPU inside `~cwSaveLoad`'s nested event loop and was killed
(`tmp/spike/cavewhere-test-hung.log`); rerun without it: 2303 cases, 2293
passed, 1 failed, 1 skipped, 8 failed as expected. The failure moved
between `test_cwScrapManager.cpp:743` and `:1038` across runs. Both are
unconfirmed against a HEAD build.

## Step 5 — profile: rings stand vertical everywhere (decided Oct 19)

A chain 1 → 2 → 3 in profile, leg 1 → 2 level and leg 2 → 3 pitched down
by α, then a pitch with its real readings (lip `U2 D19`, bottom `U19 D1`).
Drawings: `sweep_profile.py`, `sweep_profile_miter.py`, `pit_profile.py`.

### The bisector rule turned vertical, rejected

The plan miter (step 1) applied in profile — the ring square to the
bisector of the two legs, U and D along the ring's tilted up — matches the
plumb ring through about 30° and breaks at a pitch: the lip ring tilts 45°
and `D19` runs back and down under the passage toward 1; the bottom ring is
level and `U19` runs 19 m forward as floor (`sweep_profile_miter.png`).

L and R are read square to the passage, so a ring square to the bisector
is their home at any bend. U and D are read plumb, and at a pitch they are
the shaft itself (`D19` is the drop, `U19` the shaft above), lying along
the steep leg rather than across it. Tilting them with the ring moves the
shaft. Kept plumb, the lip's `D19` and the bottom's `U19` trace the shaft.

### Decision

Every ring stands vertical: L and R level along the step-1 right (clockwise
of the plan bisector), U and D plumb. A leg's rings advance along its plan
run, padded at each end so a plumb leg still has thickness:

```
pad(s)    = mean(L, R) / 2
e         = 1 − min(1, planRun / (pad(start) + pad(end)))
rings at    plan(start) − e·pad(start)  and  plan(end) + e·pad(end)
ring(t)   = quad-ellipse of the sections lerped between the two stations
```

One rule, continuous in inclination: at 90° `e = 1` and the shaft is a box
2·pad deep; at 80° with a 3.5 m plan run `e = 0` and the shaft's thickness
comes from the two tall bars overlapping in height; in between the pad hands
over with no step. A plumb leg's plan direction is zero, so its rings take
the chain's incoming plan direction (the outgoing one at a chain start), the
same fallback step 1 uses for the zero bisector. `kSteepLegDegrees` and the
level section (front and back from `mean(L, R)`) go; the pad is what is
left of them.

Each leg between a gentle and a steep neighbor is its own capped tube
(step 2): a chain splits where it turns down a pitch, and the pieces meet by
union. The lip and the bottom each carry two rings in different planes — the
gentle tube's tall bar and the box's padded ring — which is a change of
regime, distinct from the plan double ring step 1 deleted.

### The wedge, accepted

Lofted as read, the gentle leg into a lip goes `D1 → D19`: a sloped slab
under the passage down to the shaft, and the leg out of the bottom goes
`U19 → U2` up from the floor (`pit_profile.png`). In the union they read as
the shaft's back and front walls, and at 90° they coincide with the box's
faces. The alternative — a lip station's D belonging to the shaft, with the
gentle leg borrowing its neighbor's floor — is a rule at the station, which
step 2 set out to remove. The wedge is what the data says.

A `U19` at the foot of a 45° ramp lofts up past the lip; that is the
synthetic reading lying rather than the rule, since a surveyor on a ramp
reads U to the ramp's ceiling.

### For the spike

The box's top ring reaches `U2`, exactly the passage ceiling, and its sides
span the lip ring: coincident faces at the mouth. The step-3 slide covers it
if a cap vertex *on* another tube's surface counts as outside and slides
until it is strictly inside. Watch for it in step 4 before adding a rule.

## Step 6 — the sandbox (done Oct 19)

The drawings settled steps 1–5 in 2D; the 3D result gets the same loop in
the gallery: an editable junction row with sliders for every angle and
reading, rebuilt on each change, so the user drags a slider and says what
is wrong. Spec: `TUBE_RENDERING_FOLLOWUPS.md` T07ai (`06e8f36a`), panes
made resizable by T07aj (`fe627784`).

## Step 7 — the builder rewrite (specified Oct 19)

Four tasks in `TUBE_RENDERING_FOLLOWUPS.md`, run in this order so the
tree builds clean after each: T07ak makes the union the builder's output
(steps 2 and 4, cap slide included, trip attribution through manifold's
original IDs, `EnclosedVoid` and `ChainSelfCrossing` warnings, checker
rules for the seam and for readings into openings); T07al is step 1 plus
the chain half of step 2 (a chain is one chunk's run of shots, one
vertical ring per station square to the plan bisector, Loch's rule, the
through, the dome and the double ring deleted, `JunctionReadingConflict`);
T07am is step 5 (the pad rule, chains split where a padded ring leaves its
station, `kSteepLegDegrees` and the level section deleted); T07an is the
deletion list. One reading of step 2 made explicit in T07al: a chain ends
where its chunk does, so a trip boundary or a continuation in another
chunk is two capped tubes meeting end to end, joined by the union, and a
record that differs between the two chunks shows as a step at the
station, as HEAD's double ring did.

All four landed Oct 20 (`e838a9f3`, `d140b404`, `4cf0ac67`, `5db8e15c`).
What the rewrite left open, and what we suspect but have not seen, is
tracked in `TUBE_RENDERING_ISSUES.md`.

## Where the old plans stand

`TUBE_RENDERING_FOLLOWUPS.md` T07ah3 and T07ah4 are superseded by steps
1–5; T07x (union of overlapping passages) becomes the mechanism rather than
a fallback. T07ab, T07o, T07u and T08–T12 are to be re-read against this
document before any run. The uncommitted T07ah3 attempt is in
`tmp/T07ah3-fix1-start.patch`; the working tree should be reset to
`8001628b` before the spike.
