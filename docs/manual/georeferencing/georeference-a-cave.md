---
title: Georeference a Cave
summary: Fix a station to real-world coordinates, so the cave sits at its true place on Earth.
problem: Place a floating survey on the real-world map — so caves stop overlapping, overlays line up, and auto declination works.
keywords: [georeference, fix station, coordinate system, utm, epsg, wgs84, easting, northing, elevation, projection, datum, overlap]
related: [grid-convergence.md, ../concepts/coordinate-systems.md, ../survey-data/declination.md]
---

# Georeference a Cave

## Why / when you need this

A cave you've surveyed has the right *shape* but no *place*. Its stations sit in
CaveWhere's own local frame, floating: the survey is internally consistent, but
nothing has told CaveWhere where on Earth it is or which way is true north.

That's fine until the cave has to meet the outside world. Two things in
particular break until you place it:

- **Add a second cave and they overlap.** Every un-georeferenced cave is anchored
  at the *same* local origin, so a project with several caves piles them all on
  top of each other in the 3D view. Fixing each one to its real coordinates is
  what pushes them apart into their true relative positions.
- **[Auto declination](../survey-data/declination.md#let-cavewhere-work-it-out-auto)
  stays unavailable.** Computing declination from the IGRF model needs to know
  where the cave is; without a fixed station there's nothing to compute from.

Georeferencing also lets you overlay the cave on surface maps and imagery, drop
georeferenced [point clouds](../point-clouds/add-a-point-cloud.md) into the same
space, and hand real coordinates to anyone who needs them. This page is the
task; [Directions and Coordinate Systems](../concepts/coordinate-systems.md) is
the concept behind it.

## One thing to set

Georeferencing a project is one step: **fix at least one station** to known
coordinates. That is the anchor that pins the floating survey to the real world,
and it is per cave, on the cave's **Fix Stations** page.

There is no project coordinate system to choose. CaveWhere works out the frame
the project is drawn in from the first thing that says where the project is — a
fixed station, or a georeferenced [point
cloud](../point-clouds/add-a-point-cloud.md) — and centers that frame on it. The
frame follows your data instead of you having to pick a grid for it, which also
means you can never pick the wrong one.

## Fix a station

A **[fixed station](../concepts/glossary.md#fixed-station)** is a survey station
whose real-world coordinates you know — typically the entrance, tied in with a
GPS reading, a benchmark, or a surface survey.

Open a cave's page and find the **Fix stations** link (it shows the current count,
`0` on a cave that isn't yet georeferenced). Click it to open the **Fix Stations**
page, then click **Add Fix** to add a row. Each row has five fields:

- **Station** — the name of the real survey station you're fixing (for example
  `A1`). It has to match a station that exists in the cave, or the fix has nothing
  to anchor.
- **Input CS** — the coordinate system *the numbers you're typing* are in, and
  nothing else reads them: if your GPS gave you latitude and longitude, set this
  row to **Lat/Lon (WGS84)** and type those; if your numbers are already eastings
  and northings, leave it on **UTM** and pick the zone. Each row carries its own,
  so fixes entered in different systems live side by side.
- **Easting**, **Northing**, **Elevation** — the coordinates themselves, in
  meters. (With a Lat/Lon input CS these fields hold longitude, latitude, and
  elevation instead.)

![The Fix Stations page with one fixed station: station a1, its Input CS set to UTM, and easting, northing, and elevation values.](../images/georef-fix-station.png)
*One fixed station anchors the cave. The **Input CS** picker on the row lets you
enter each fix in whatever system you have the numbers in.*

Double-click a field to edit it; the change is applied when you finish. To remove
a fix, right-click its row. You can fix **more than one** station — the survey
then ties to every fixed point at once, and CaveWhere adjusts the network between
them the same way it closes any loop.

## What fixing does

The moment a cave has a valid fix, CaveWhere re-solves the survey anchored to
those real coordinates instead of the local origin. If the project had no frame
yet, that first fix is also what gives it one. You'll see it move:

- **The cave jumps to its true position**, and if the project holds several caves,
  they separate out of the pile at the origin into their real relative layout.
- **[Grid convergence](grid-convergence.md)** starts reporting a value on the cave
  page, and the bearing correction folds it in automatically.
- **[Auto declination](../survey-data/declination.md#let-cavewhere-work-it-out-auto)**
  becomes available, because the cave now has a location to compute from.

The frame CaveWhere derives is centered on that anchor, so the coordinates the
3D scene works in stay small no matter where on Earth the cave is — which is what
keeps the model precise hundreds of kilometers from any grid's zero. Refining the
anchor later moves the station, not the frame; only a correction big enough to
mean the project is somewhere else re-derives it. Exports still name a standard
system your reader can paste elsewhere.

## Read coordinates back out

Once a cave is georeferenced, you can read the real-world coordinates of any point
on the model. In the **3D view**, turn on the coordinate picker from the toolbar
and click a point: a small panel reports the point in **WGS84
latitude/longitude** and its **elevation**, each with a **Copy** button. It's a quick way to grab an entrance coordinate for a permit, a
callout, or a landowner — and a good check that a fix landed where you meant it
to.

![The 3D view with the Pick tool active: a marker on the cave and a "Picked coordinates" panel reading the point in WGS84 latitude and longitude and its elevation, each with a Copy button.](../images/georef-coordinate-picker.png)
*The coordinate picker reads one point back out as WGS84 lat/lon and elevation.*

## Next steps

- [Understand Grid Convergence](grid-convergence.md) — the readout georeferencing
  turns on, and why your bearing correction is more than just declination.
- [Set the Declination](../survey-data/declination.md) — with a fixed station,
  switch it to Auto and let CaveWhere compute it.
- [Directions and Coordinate Systems](../concepts/coordinate-systems.md) — the
  three norths, datums, and projections behind all of this.
