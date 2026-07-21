---
title: Directions and Coordinate Systems
summary: The three norths — magnetic, true, and grid — how declination and grid convergence relate them, and what georeferencing gives a cave.
problem: Understand why a compass reading isn't yet a map bearing, and what it takes to place a cave on the real-world grid.
keywords: [coordinate system, magnetic north, true north, grid north, declination, grid convergence, georeferencing, fixed station, projection, utm, datum]
related: [glossary.md, ../survey-data/declination.md, ../survey-data/calibration.md]
---

# Directions and Coordinate Systems

## Why / when you need this

A compass reading is not a map bearing, and a cave you've surveyed isn't yet
anywhere in particular. Both facts trip people up, and both come down to the same
thing: **which "north" are we talking about, and where on Earth is the cave?**

You can survey, plot, and close loops without answering either — the cave's
*shape* is correct regardless. But the moment you try to lay your map over a
surface map, combine it with a LiDAR point cloud, or hand a neighbouring survey
coordinates that line up with yours, the direction the cave points and the spot it
sits on start to matter. This page is the mental model behind
[declination](../survey-data/declination.md), grid convergence, and georeferencing,
so those pages read as adjustments to a picture you already hold rather than
isolated settings.

## Three norths

The single biggest source of confusion in cave surveying is that "north" means
three different directions, and they don't coincide:

| North | What it is | Points at |
|-------|-----------|-----------|
| **Magnetic** | Where a compass needle settles | The magnetic pole, which moves year to year |
| **True** | The direction of the geographic North Pole | The Earth's axis |
| **Grid** | "Up" on a flat map projection's grid | Straight up the grid columns |

Your instrument reads one of these (magnetic); the world references another (true);
the map you overlay onto uses the third (grid). Turning a raw reading into
something that lines up with a map means stepping across all three.

## Magnetic → true: declination

A compass points at **magnetic north**, which sits some angle away from **true
north**. That angle is the **[declination](glossary.md#declination)**, it depends
on where you're standing and what year it is, and it can be tens of degrees.

Correcting for it is the difference between a map that overlays the surface world
and one that's simply rotated off true by that angle — everything the right shape
and the right length, pointing the wrong way. CaveWhere adds the declination to
each magnetic bearing to get a true bearing, and because it drifts, the correction
is set per trip against the trip's date. The
[declination page](../survey-data/declination.md) covers how — automatically from
the IGRF model, or by hand.

## True → grid: grid convergence

Once your bearings point at true north, there's a second, smaller step before they
match a **projected map**. A flat map has to pretend the round Earth is flat, and
every projection (a UTM zone, say) does that by picking a grid whose columns run
"up" — **grid north** — which only exactly coincides with true north along one
line and leans away from it as you move east or west of that line. That lean is the
**[grid convergence](glossary.md#grid-convergence)**.

You only meet grid convergence once a cave is georeferenced — before that there's
no projection in play. When a cave *is* placed on a coordinate system, CaveWhere
folds convergence into the bearing correction automatically: the total applied to
each reading is *(declination − grid convergence)*, so corrected bearings land on
**grid** north and match the projected coordinates you're working against. You
don't compute it; you just need to know it's there, which is why a georeferenced
cave's bearing correction isn't simply its declination.

## Local vs. georeferenced

Every cave starts in a **local** frame. The survey is internally consistent — every
station in the right place relative to every other — but the whole thing floats:
its absolute position on Earth and its rotation to true north are unset, because
nothing has told CaveWhere where it is. A local cave is complete for studying its
own shape; it just isn't *placed*.

**Georeferencing** places it. You **[fix](glossary.md#fixed-station)** one or more
stations to known real-world coordinates in a chosen **coordinate system**, and
that pins the floating survey to the ground: position, and — together with
declination and convergence — orientation. A coordinate system is really two
choices bundled together: a **datum** (which model of the Earth's shape the
coordinates are measured against) and a **projection** (how that curved surface is
flattened into a flat grid of eastings and northings). Picking the same coordinate
system your other data uses is what makes everything line up.

## What georeferencing buys you

Fixing a cave to the world is optional, and plenty of projects never need it. It
earns its keep when the cave has to meet things outside itself:

- **Overlay on surface maps and imagery** — see what's above a passage, and whether
  a lead heads toward a sink or a neighbouring cave.
- **Keep multiple caves apart** — every unfixed cave starts at the *same* local
  origin, so if you add several caves to one project without georeferencing them,
  they all pile up on that shared origin and overlap in the 3D view. Fixing each
  cave to its real coordinates is what pushes them apart into their true relative
  positions, so a project full of caves lays out like the map it should be.
- **Combine with point clouds and LiDAR** — a georeferenced survey and a
  georeferenced scan share one coordinate space, so they drop into place together.
- **Auto declination** — computing declination from the IGRF model needs to know
  where on Earth the cave is, so [Auto mode](../survey-data/declination.md#let-cavewhere-work-it-out-auto)
  only becomes available once a station is fixed.
- **Coordinates others can use** — a fixed survey hands real positions to rescue
  teams, landowners, and adjacent projects, instead of a shape floating in space.

Georeferencing has its own chapter, still to be written — fixing stations, choosing
a coordinate system, and reading grid convergence in the editor. This page is the
concept behind it.

## Next steps

- [Set the Declination](../survey-data/declination.md) — the magnetic-to-true
  correction, in the app.
- [Calibrate the Instruments](../survey-data/calibration.md) — the other
  corrections applied to a raw reading.
- [Glossary](glossary.md) — [declination](glossary.md#declination),
  [grid convergence](glossary.md#grid-convergence),
  [georeferencing](glossary.md#georeferencing), and
  [fixed station](glossary.md#fixed-station) in brief.
