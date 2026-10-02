---
title: Understand Grid Convergence
summary: The correction georeferencing turns on — what grid convergence is, where the grid comes from, and why it changes your bearing correction.
problem: Understand why a georeferenced cave's bearing correction is more than its declination, and why correcting for grid convergence is what lets multiple fixed stations close and a cave align with aerial LiDAR.
keywords: [grid convergence, grid north, true north, bearing correction, declination, utm, projection, low-distortion projection, ldp, scale factor, transverse mercator, meridian, georeference, fixed station, loop closure, aerial lidar, point cloud, alignment, grid bearing]
related: [georeference-a-cave.md, ../concepts/coordinate-systems.md, ../survey-data/declination.md]
---

# Understand Grid Convergence

## Why / when you need this

Once you [georeference a cave](georeference-a-cave.md), a second correction
switches on: **grid convergence**. It's not something you set or see — CaveWhere
computes and applies it — but it's worth understanding, because it quietly
rotates every compass reading in the cave onto the grid, and that rotation is
what lets the cave line up with anything measured on the grid. Two cases make it
matter:

```
Grid convergence: -1.08° at a1
```

You cannot edit it, and it matters in 2 jobs. Fix 2 stations and the direction
between them is a grid bearing, while your survey measured a true one. Leave
that gap in and the traverse cannot reach both fixes at once, which surfaces in
the [loop closure report](../loop-closure/check-loop-closure.md) as an error no
re-measuring will clear. Aerial LiDAR and
[point clouds](../point-clouds/add-a-point-cloud.md) arrive oriented to grid
north, so a cave turned to true north sits rotated off them: at 1° of
convergence, a station 2 km in lands 35 m off.

## What it is

**Grid convergence** is the angle between **true north**, the direction to the
geographic pole, and **grid north**, the "up" of your projected coordinate
system. A projection lines its grid up with true north along one line only, its
**central meridian**. East or west of that, grid north leans away from true, and
that lean is the convergence. Positive means grid north lies east of true north.

Unlike [declination](../survey-data/declination.md), grid convergence has
nothing to do with the Earth's magnetic field, so it doesn't drift with time —
it depends only on location and projection. CaveWhere reads it at the cave's
first fixed station, in the
[projection it derived for your project](#the-grid-is-a-low-distortion-projection) —
and that choice of projection is what keeps the angle down to a fraction of a
degree instead of the couple of degrees a UTM zone would hand you.

So it takes two things: a projection for the project, *and* a fixed station that
says where this cave sits in it. Until both are there — nothing georeferenced
yet, or a cave with no [fixed station](georeference-a-cave.md#fix-a-station) of
its own — the convergence is zero and the cave is drawn to true north.

To check the readout by hand:

> **convergence ≈ sin(latitude) × (longitude − central meridian)**

Station **a1** shown below sits at easting **350000**, northing **4300000** in
**EPSG:32613**, UTM zone 13N. UTM puts every central meridian at easting 500000,
so a1 is 150 km west of zone 13's, which lands at 106.73°W, 38.84°N, or 1.73°
west of 105°W. That gives sin(38.84°) × 1.73° = 1.08°, negative because it lies
west. PROJ returns −1.08396°, which the cave page rounds to 2 decimals.

So convergence depends on where in a projection you sit, not only on which
projection you picked: inside zone 13N at latitude 40° it varies by 1.9° between
the central meridian and the zone edge. It has no magnetic component, so unlike
[declination](../survey-data/declination.md) it never drifts with time.

## The grid is a low-distortion projection

Which projection you land on decides how big the convergence gets, and CaveWhere
doesn't put you in a UTM zone. The first thing you georeference — a fixed station
or a point cloud — becomes the center of a **low-distortion projection** (LDP): a
transverse Mercator built for your project alone, running through your own cave.
Three things follow, and they're the reason the LDP exists:

- **Distances on the grid are the distances you surveyed.** A UTM zone shrinks
  everything by 400 ppm at its central meridian — 40 cm of every kilometer — and
  stretches it by about as much out at the zone edge, so a 1 km survey leg is
  never quite 1 km on the grid. CaveWhere's LDP sits at true scale where you are:
  even 50 km out from its center — about as far as a project ever reaches — the
  error is around 3 cm per kilometer, under the noise of the tape that measured
  it.
- **Grid north is essentially true north.** Convergence is exactly zero along the
  meridian running through the LDP's center, and it grows only with **east–west**
  distance from that line — a cave 50 km due *north* of the center has none at
  all. Due east or west, 50 km out, it reaches about 0.26° at 30°N, 0.38° at
  40°N, and 0.53° at 50°N. The same cave placed at the edge of a UTM zone would
  carry 1.93° at 40°N — five times as much. So the bearing correction stays close
  to the [declination](../survey-data/declination.md) alone. Small isn't zero,
  though, so CaveWhere still applies it: a third of a degree is the same order as
  the precision of the compass that took the reading, and it keeps growing the
  farther east or west the cave runs.
- **There are no zone seams.** A cave that straddles a UTM zone boundary would
  otherwise be split between two grids with different norths; the LDP has one
  center and no edges.

The trade is that the LDP belongs to this project — it isn't a published system
another program will recognize by name. That only matters on the way out, which
is why [exporting a survey](../import-export/export-surveys.md) writes a standard
coordinate system rather than the project's own frame.

## Reading the value

The **Grid convergence** cell sits next to the **Fix stations** link on the cave
page. When the cave is georeferenced it reads as an angle at a station — for
example `0.376° at a1` — and hovering it gives the value at full precision plus
the grid it's measured in.

The readout carries three decimals because the
[LDP](#the-grid-is-a-low-distortion-projection) keeps the angle small on
purpose. Convergence grows by roughly 0.008° per kilometer of east–west distance
from the projection's center at 40°N, so at coarser precision most caves in a
project would round to a flat zero — which would read as a correction that isn't
running, when in fact it's running and tiny. That's the point the cell is making.

The **first** thing you georeference always reads `0.000°`, and that's the right
answer rather than a missing one: it defines the projection's center, and grid
north and true north are the same direction along the meridian through it. The
tooltip says as much, so the zero doesn't look like a failure.

Before a cave is georeferenced, the cell explains *why* it has nothing to report:

| Reading | Meaning |
|---------|---------|
| `0.376° at a1` | The convergence CaveWhere is applying, measured at that fixed station. |
| `0.000° at a1` | The cave sits on the projection's central meridian — usually because it's the one that defined it. |
| `n/a (no fix station)` | Nothing places this cave: it has no [fixed station](georeference-a-cave.md#fix-a-station), or the ones it has name no [coordinate system](georeference-a-cave.md#fix-a-station) to read their numbers under. |
| `n/a (no coordinate system)` | Nothing in the project is georeferenced yet, so there is no projection to converge to and the model is drawn to true north. |

Both "n/a" readings are a checklist: convergence needs a projection for the
project *and* a fixed station that says where this cave sits in it, and the
readout tells you which one is still missing.

![The cave page showing the Fix stations count and the Grid convergence readout, with the help panel open explaining the value.](../images/georef-grid-convergence.png)
*The grid convergence readout on the cave page. The help panel (the **?**) spells
out what the value means and how it's applied.*

## Why it changes your correction

Here's what it does. When CaveWhere
solves the survey, it turns each magnetic compass reading into a **grid bearing** —
one that lines up with your projected coordinates. That's two corrections in turn:

1. **Correct to true north** — add the [declination](../survey-data/declination.md):
   *magnetic reading + declination = true bearing*.
2. **Correct for the grid** — subtract the grid convergence:
   *true bearing − grid convergence = grid bearing*.

Chained together, that's the whole correction at once:

> **grid bearing = magnetic reading + declination − grid convergence**

So the bearing CaveWhere ends up plotting is a *grid* bearing, not a true one —
and that grid isn't an abstract reference direction. **The grid is the projection
CaveWhere derived for the project** when you
[georeferenced the cave](georeference-a-cave.md#fix-a-station), and that same
frame is the one it lays the 3D model out in. Look at a georeferenced cave in plan view and north on the screen *is* grid
north; the model's eastings and northings are the grid's. So the second correction
isn't bending your survey toward some outside grid — it's aligning it to the map
CaveWhere already draws. On a local (un-georeferenced) cave there's no projection,
so convergence is zero, the correction stops at step 1, and the model is drawn to
true north instead. Georeference the cave and step 2 switches on automatically.

This is why you can't read a georeferenced cave's total bearing correction off
its declination alone, and why the same survey can point very slightly
differently once it's placed on a grid. The shift is usually small — a fraction
of a degree to a couple of degrees — but it's real, and it's what keeps the cave
aligned with everything else in its coordinate system.

It runs whichever way the trip's declination is set. On Auto, survex folds the
convergence in itself; on Manual, CaveWhere subtracts it from the declination it
writes out, and says so in a comment on the exported line. Either way the plotted
bearings are grid bearings, and the plan-view [scrap](../scraps/digitize-a-scrap.md)
fit matches.

When the alignment matters **I read the cavern log rather than the cave page**,
since the log reports what cavern applied. Open **Cavern Output** and look for
`Declination: … , grid convergence: …`, printed to 1 decimal, so a cave reading
`0.376°` here shows `0.4dg` there. A missing line means no trip date, not
necessarily no convergence.

## Next steps

- [Georeference a Cave](georeference-a-cave.md) — fix the station that turns this
  correction on.
- [Set the Declination](../survey-data/declination.md) — the other half of the
  bearing correction.
- [Check Loop Closure](../loop-closure/check-loop-closure.md) — where a missing
  convergence correction shows up as an error between fixed stations.
- [Directions and Coordinate Systems](../concepts/coordinate-systems.md) — the
  three norths in full.
