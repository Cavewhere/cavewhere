---
title: Understand Grid Convergence
summary: The per-cave readout that appears once a cave is georeferenced, what its four "n/a" states mean, and the one setting that decides whether the solve uses it.
problem: Read the grid convergence on the cave page, check the number yourself, and find out whether your trips are actually corrected for it.
keywords: [grid convergence, grid north, true north, bearing correction, declination, auto declination, utm, projection, central meridian, georeference, fixed station, cavern, aerial lidar, point cloud, grid bearing]
related: [georeference-a-cave.md, ../concepts/coordinate-systems.md, ../survey-data/declination.md, ../loop-closure/check-loop-closure.md]
---

# Understand Grid Convergence

## Why / when you need this

[Georeference a cave](georeference-a-cave.md) and a line you never asked for
appears in its stat block, under **Fix stations**:

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

![A map graticule with two families of north lines: orange true-north meridians fanning out toward the pole, and a blue square UTM grid. Points A and B sit on the same orange meridian, both at longitude 108°E, so a sight from A to B is due true north; but the meridian leans against the grid, so B falls left of the vertical grid line through A and gets a smaller UTM easting.](../images/illustrations/utm-grid-convergence.svg)
*The lean is drawn much larger than life. At A, 108°E and 3° east of UTM zone
48N's central meridian, the real angle is 1.4°. Based on a diagram by
**Mike Futrell**.*

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

## Reading the value

Clicking the **Grid convergence:** label opens a help panel; hovering the value
names the coordinate system the way PROJ spells it,
`-1.08° at a1 (WGS 84 / UTM zone 13N)`. CaveWhere computes at the **first** fix
station, using that fix's own **Input CS** and falling back to the project's.

![The cave page for Phake Cave 3000: Length 152.23 m, Depth 36.41 m, Leads 5, Fix stations 1, and the Grid convergence row ringed in orange reading -1.08° at a1. Beside it one trip, Release 0.08, dated 2020-02-26, stations A 1-14, 152.54 m, declination 0° manual.](../images/georef-grid-convergence.png)
*The readout, ringed. The **Decl** column beside it reads `0°`, tagged `manual`,
which matters more than the convergence does.*

| Reading | Meaning |
|---------|---------|
| `-1.08° at a1` | The angle CaveWhere computed, at that station. A fix with no station name reads `at fix station`. |
| `n/a (no fix station)` | No [fixed station](georeference-a-cave.md#fix-a-station), so no location to compute at. |
| `n/a (no coordinate system)` | There is a fix, but neither it nor the [project](georeference-a-cave.md#choose-the-projects-coordinate-system) supplies a system. |
| `n/a (geographic CS)` | The **fix's** system is latitude and longitude, which has no grid. Watch this one: a Lat/Lon fix inside a UTM project reads `n/a` here while the solve still applies the project grid's convergence. |
| `n/a (Failed to transform from 'EPSG:999999' to WGS84: …)` | PROJ rejected the system it was handed, and the parenthesis carries its complaint. |

The fix station is tested first, so a cave missing both a fix and a coordinate
system reports `n/a (no fix station)` and says nothing about the second gap.

## It only applies on Auto

**CaveWhere applies the convergence only when the trip's declination is on
Auto**, which is the default on new trips.

On Auto it writes survex a `*declination auto` directive at the fix's
coordinates. Survex computes the IGRF declination for the trip's date, subtracts
its own convergence, and applies both to every compass reading:

> **grid bearing = magnetic reading + declination − grid convergence**

That grid is the project coordinate system itself, so in plan view north on
screen *is* grid north. On Manual, CaveWhere writes a plain
`*calibrate DECLINATION` line, which survex applies verbatim and never pairs
with a convergence; a manual `0` writes no line at all. So the cave shown above,
tagged `0° manual`, reports a −1.08° convergence that nothing in its plot uses.

[Set the Declination](../survey-data/declination.md) gives 3 reasons to sit in
Manual. The first, no fixed station, costs nothing, since a cave without a fix
has no convergence to lose. The other 2 cost you this correction silently. The
plan-view [scrap](../scraps/digitize-a-scrap.md) fit reads the same switch.

When the alignment matters **I read the cavern log rather than the cave page**,
since the log reports what cavern applied. Open **Cavern Output** and look for
`Declination: … , grid convergence: …`, printed to 1 decimal, so a1's `-1.08°`
reads `-1.1°`. A missing line means no trip date, not necessarily no
convergence.

## Next steps

- [Georeference a Cave](georeference-a-cave.md): the coordinate system and fixed
  station that turn this readout on.
- [Set the Declination](../survey-data/declination.md): the other half of the
  correction, and the Auto switch it depends on.
- [Check Loop Closure](../loop-closure/check-loop-closure.md): where a missing
  convergence correction shows up as an error between fixed stations.
- [Directions and Coordinate Systems](../concepts/coordinate-systems.md): the
  three norths in full.
