---
title: How a Project Is Organized
summary: The Region → Cave → Trip → Survey chunk → Shot / Note → Scrap hierarchy: what each level holds, what decision it encodes, and why an edit low in the tree ripples upward.
problem: Know where each kind of data lives and why editing one shot can re-close a loop and re-carpet a sketch.
keywords: [data model, region, cave, trip, survey chunk, shot, note, scrap, station, hierarchy, structure, keywords]
related: [glossary.md, ../getting-started/why-cavewhere.md, ../survey-data/caves-and-trips.md]
---

# How a Project Is Organized

## Why / when you need this

CaveWhere shows you tables, notes, and a 3D view. One tree sits underneath them
all, and its shape answers three questions the UI never spells out: where a piece
of data goes, and why calibration belongs to a whole trip instead of one shot. The
third one is the surprise. Correcting a single distance can shift a loop closure
and re-carpet a sketch you drew months ago.

You never build the tree by hand. It fills in as you survey. But the moment
something surprises you ("why did that scrap move?"), the answer usually lives in
the tree.

## The shape of a project

The tree runs 5 levels deep, from the whole project down to one digitized outline:

```
Region                  the whole project, holds every cave
└─ Cave                 one cave system
   └─ Trip              one team, one day, one set of instruments
      ├─ Survey chunk   a connected run of shots
      │  └─ Shot        a measured leg: from/to station, distance, compass, clino
      └─ Note           a drawing made in the cave (scan, PDF, SVG, or LiDAR)
         └─ Scrap       a digitized outline, carpeted onto the survey
```

That shape mirrors how cavers actually survey. Different teams map a cave on
different days with different instruments, sometimes across decades, and the map
has to come together out of all of it. Each level groups what those surveys share.

## What each level is for

### Region: the project's root

The **[region](glossary.md#region)** holds every **[cave](glossary.md#cave)** in
the project, and one project can carry a whole system's worth. Single-cave
projects usually give the region the cave's name, which is why the bold name at
the top of the [Data page](../survey-data/caves-and-trips.md#add-a-cave) looks
like it names the cave. It names the region.

### Cave: one system

A **[cave](glossary.md#cave)** covers one cave system and reports the 2 numbers
you watch, length and depth, recomputed as you type. A cave also owns its
**coordinate system**: georeferencing places it on the real-world map. See
[Directions and Coordinate Systems](coordinate-systems.md).

### Trip: where the corrections live

A **[trip](glossary.md#trip)** covers *one team, one day, one set of
instruments*. Of the 5 levels, I recommend learning this one first. A trip owns
the corrections: [declination](../survey-data/declination.md), a tape
[calibration](../survey-data/calibration.md), and compass and clino calibrations
for front sights. Record backsights and you get a second pair. The Calibration
panel, shown below, holds all of them.

![The Calibration panel from a trip: a Declination field, a Distance Calibration field with Units set to m, and a checked Front Sights box holding Compass calibration and Clino calibration, both 0, with Backwards Compass and Backwards Clino unchecked. Back Sights is unchecked.](../images/survey-calibration.png)
*Every correction on this panel applies to one trip. Back Sights stays unchecked
until the team records backsights, which is why the panel shows 4 numbers here and
6 when both boxes are ticked.*

That grouping records a measurement fact. Everything surveyed on one day with one
tape and one compass needs the same correction, and a different day with different
instruments needs a different one. Push these settings up to the cave and two
teams could no longer get corrected apart. Push them down to the shot and you
would retype the same numbers on every leg.

A trip also owns its **team**, and each member's name becomes a **Caver**
[keyword](glossary.md#keyword) you can filter the whole cave by. See
[Organize Caves and Trips](../survey-data/caves-and-trips.md#record-the-team).

### Survey chunk: a connected run

Within a trip, shots group into **[survey chunks](glossary.md#survey-chunk)**, the
blocks of rows you type in the
[survey table](../survey-data/enter-survey-data.md). A chunk cannot branch, so a
side passage leaving the main line starts its own chunk. That constraint explains
why entering a branch means pressing Space for a new block instead of typing on:
the split makes the model tell the truth about the cave's shape.

### Shot: the skeleton

A **[shot](glossary.md#shot)** measures one leg between two
**[stations](glossary.md#station)** and carries up to 5 readings: distance, front
and back compass, front and back clino. Shots form the skeleton the 3D model hangs
on. CaveWhere derives every station's position by following shots out from a fixed
point, then adjusting the whole network so the loops close.

### Note and scrap: the drawing side

A **[note](glossary.md#note)** holds a drawing made in the cave: a scanned sketch,
a PDF, an SVG from an app like TopoDroid, or a LiDAR scan. Notes attach to the trip
that drew them. See [Add Notes to a Trip](../notes/add-a-note.md).

A **[scrap](glossary.md#scrap)** covers one piece of that drawing,
[digitized](../scraps/digitize-a-scrap.md) into an outline you trace and tie to the
stations inside it. One note usually splits into several scraps. The scrap is where
the drawing meets the survey: CaveWhere [carpets](glossary.md#carpeting) it onto
the 3D shots so the sketch follows the real passage. Each scrap also carries
its own [type](../scraps/scrap-types.md), because the projection describes that
drawing and not the whole trip. CaveWhere handles 3 of them today (plan, running
profile, projected profile); cross-sections remain a known limitation.

## Stations: the thread through both sides

A trip branches two ways: **shots** on the measurement side, **notes and scraps**
on the drawing side. **[Stations](glossary.md#station)** stitch the two back
together. A station names a point (`A1`, `A2`). A shot connects two of them, and
you anchor a scrap by placing those same names on the sketch.

CaveWhere matches station names across the whole cave, and it matches them
case-insensitively, so `a1` and `A1` reach the same point. Consistent naming is
what lets separate trips join into one connected survey. If I could enforce one
habit on a survey team from day one, it would be this one. A mistyped name may be
the most common reason a leg fails to connect, or a scrap lands somewhere strange.

## Why an edit ripples

Knowing the tree pays off here. CaveWhere solves the survey instead of merely
storing it. It runs [Survex](glossary.md#survex) 1.4.21 over the whole shot
network, finds each station's most likely position, and spreads loop-closure error
across every leg. Correct one shot's distance and the solve re-runs, so stations
*elsewhere* shift. That movement means loop closure did its job.

Scraps anchor to stations, so when a station moves, every scrap tied to it
re-carpets to follow. You fix a reading in a table, and a sketch you drew months
ago re-carpets to match with nothing to redraw. That cascade, from one shot up
through the solve and back down through the scraps, keeps the model honest as the
survey grows.

The re-carpet does not always land clean. A plan scrap that pulls in stations it
should not may leave small vertical bumps, and tightening the default of 10
nearest stations usually clears them. See
[Troubleshoot Carpeting](../scraps/troubleshoot-carpeting.md).

For the mechanism behind the solve, see
[Why CaveWhere](../getting-started/why-cavewhere.md#keeping-the-map-correct-loop-closure); for the
re-carpeting, see [Scraps and Carpeting](../scraps/carpeting.md).

## What cuts across the tree

Two things deliberately break the strict parent-child hierarchy:

- **[Keywords](glossary.md#keyword)** tag caves, trips, teams, notes, and scraps
  alike, then drive
  [layer visibility](../view-3d/the-3d-view.md#focus-on-part-of-the-cave-layers)
  in the 3D view. They let you focus on one part of a big cave regardless of where
  it sits in the tree.
- **Version history** belongs to the project as a whole. Every
  [save](../projects-and-files/save-a-project.md) records a version of the entire
  tree at once, which is why rolling back moves the whole project together.

## Next steps

- [Glossary](glossary.md): terse definitions of every term above.
- [Organize Caves and Trips](../survey-data/caves-and-trips.md): building the top
  of the tree in the app.
- [Directions and Coordinate Systems](coordinate-systems.md): how a cave gets
  placed on the real-world map.
