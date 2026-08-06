---
title: Directions and Coordinate Systems
summary: The three norths (magnetic, true, and grid), how declination and grid convergence relate them, and what georeferencing gives a cave.
problem: Understand why a compass reading isn't yet a map bearing, and what it takes to place a cave on the real-world grid.
keywords: [coordinate system, magnetic north, true north, grid north, declination, grid convergence, georeferencing, fixed station, projection, utm, datum]
related: [glossary.md, ../survey-data/declination.md, ../survey-data/calibration.md]
---

# Directions and Coordinate Systems

## Why / when you need this

A compass reading is not yet a map bearing, and a cave you have surveyed does not
yet sit anywhere in particular. Both facts trip people up, and both come down to
one question: which "north" do you mean, and where on Earth does the cave sit?

You can survey, plot, and close loops without answering either, because the cave's
*shape* holds up regardless. The answers start to matter the moment the cave has
to meet the outside world: a surface map you overlay it on, a LiDAR point cloud
you combine it with, a neighboring survey that needs coordinates matching yours.

## Three norths

Cave surveying's biggest source of confusion: "north" names 3 different
directions, and they disagree.

| North | What it is | Points at |
|-------|-----------|-----------|
| **Magnetic** | Where a compass needle settles | The magnetic pole, which moves year to year |
| **True** | The direction of the geographic North Pole | The Earth's axis |
| **Grid** | "Up" on a flat map projection's grid | Straight up the grid columns |

Your instrument reads the first, the world references the second, and the map you
overlay onto uses the third.

## Magnetic to true: declination

A compass points at **magnetic north**, which sits some angle away from **true
north**. That angle, the **[declination](glossary.md#declination)**, varies with
where you stand and what year you stand there, and in some parts of the world it
may reach tens of degrees.

Correct for it and your map overlays the surface world. Skip it and the map comes
out rotated off true by that angle: every shape right, every length right, the
whole cave pointing the wrong way. CaveWhere adds the declination to each magnetic
bearing, and because the field drifts, you set the correction per trip against the
trip's date. A declination in the United States might read `-3.2°`. The
[declination page](../survey-data/declination.md) covers typing that in by hand or
pulling it from IGRF-14, the magnetic-field model that ships inside
[Survex](glossary.md#survex).

## True to grid: grid convergence

Once your bearings point at true north, one smaller step remains before they match
a **projected map**. A flat map has to pretend the round Earth lies flat, and every
projection does that by picking a grid whose columns run "up". That direction,
**grid north**, matches true north along one line only, and it leans further away
as you move east or west of that line. The lean is the
**[grid convergence](glossary.md#grid-convergence)**, shown below.

![A map graticule with two families of north lines: orange true-north meridians that fan out and converge toward the pole, and a blue square UTM grid. Two points on the same orange meridian sit due true north of each other, yet the meridian leans relative to the grid, so the northern point falls left of the vertical grid line and takes a smaller easting.](../images/illustrations/utm-grid-convergence.svg)
*True north fans toward the pole; the projected grid stays square. A UTM zone
spans 6° of longitude, and the lean grows the further you sit from its center
line. See [Grid Convergence](../georeferencing/grid-convergence.md).*

Grid convergence only shows up once a cave gets georeferenced, because until then
no projection exists to converge on. The cave page reports the angle it computed,
reading something like `-1.08° at a1`.

Whether the solve *uses* that angle depends on the trip. On **Auto** declination
the correction comes to *(declination − grid convergence)*, so bearings land on
**grid** north and match the projected coordinates you work against. On Manual
they carry the declination alone and the reported convergence changes nothing.
[Auto](../survey-data/declination.md#let-cavewhere-work-it-out-auto) is the
default on new trips.

## Local vs. georeferenced

Every cave starts in a **local** frame. The survey holds together internally, with
every station in the right place relative to every other, but the whole thing
floats. Its absolute position on Earth and its rotation to true north stay unset,
because nothing has told CaveWhere where it sits.

**Georeferencing** gives it a place. You **[fix](glossary.md#fixed-station)** one
or more stations to known real-world coordinates in a chosen **coordinate system**,
which pins the floating survey to the ground: position, and, together with
declination and convergence, orientation.

A coordinate system bundles 2 choices. A **datum** decides which model of the
Earth's shape the coordinates measure against, and a **projection** decides how
that curved surface flattens into a grid of eastings and northings. I recommend
picking whatever coordinate system your other data already uses, because matching
it is what makes everything line up.

## What georeferencing buys you

Fixing a cave to the world stays optional, and plenty of projects never need it. It
earns its keep when the cave has to meet things outside itself:

- **Keep multiple caves apart.** Every unfixed cave starts at the *same* local
  origin. Add several caves to one project without georeferencing them and they
  pile up on that shared origin, overlapping in the 3D view. Fixing each cave to
  its real coordinates pushes them apart into their true relative positions, so a
  project full of caves lays out like the map it should be.
- **Combine with point clouds and LiDAR.** A georeferenced survey and a
  georeferenced scan share one coordinate space, so they drop into place together.
- **Auto declination.** IGRF-14 has to know where on Earth the cave sits, so
  [Auto mode](../survey-data/declination.md#let-cavewhere-work-it-out-auto)
  unlocks only after you fix a station.
- **Coordinates others can use.** A fixed survey hands real positions to rescue
  teams, landowners, and adjacent projects, instead of a shape floating in space.

[Georeference a Cave](../georeferencing/georeference-a-cave.md) covers fixing
stations and choosing a coordinate system.

## Next steps

- [Set the Declination](../survey-data/declination.md): the magnetic-to-true
  correction, in the app.
- [Calibrate the Instruments](../survey-data/calibration.md): the other
  corrections applied to a raw reading.
- [Glossary](glossary.md): [declination](glossary.md#declination),
  [grid convergence](glossary.md#grid-convergence),
  [georeferencing](glossary.md#georeferencing), and
  [fixed station](glossary.md#fixed-station) in brief.
