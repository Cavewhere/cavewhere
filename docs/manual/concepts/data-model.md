---
title: How a Project Is Organized
summary: The Region → Cave → Trip → Survey chunk → Shot / Note → Scrap hierarchy — what each level holds, what decision it encodes, and why an edit low in the tree ripples upward.
problem: Know where each kind of data lives and why editing one shot can re-close a loop and re-drape a sketch.
keywords: [data model, region, cave, trip, survey chunk, shot, note, scrap, station, hierarchy, structure, keywords]
related: [glossary.md, why-cavewhere.md, ../survey-data/caves-and-trips.md]
---

# How a Project Is Organized

## Why / when you need this

CaveWhere's window is a set of tables, notes, and a 3D view, but underneath them
is a single tree — and knowing its shape tells you three things the UI doesn't
spell out: **where** to put a piece of data, **why** a setting like calibration
belongs to a whole trip rather than a single shot, and **why** editing one shot
can shift a loop closure and re-drape a sketch you drew months ago.

You don't have to build this tree by hand — you fill it in as you survey, and the
levels appear as you go. But the moment something surprises you ("why did that
scrap move?", "where do I set units?"), the answer is almost always a fact about
the tree.

## The shape of a project

A project is one nested hierarchy, from the whole project at the top down to a
single digitized outline at the bottom:

```
Region                  the whole project — holds every cave
└─ Cave                 one cave system
   └─ Trip              one team, one day, one set of instruments
      ├─ Survey chunk   a connected run of shots
      │  └─ Shot        a measured leg: from/to station, distance, compass, clino
      └─ Note           a drawing made in the cave (scan, PDF, SVG, or LiDAR)
         └─ Scrap       a digitized outline, carpeted onto the survey
```

The structure isn't arbitrary — it mirrors how a cave is actually surveyed. Caves
are mapped by different teams, on different days, with different instruments, over
years, and the map has to be assembled from all of it. Each level of the tree is
the answer to "what is shared here?"

## What each level is for

### Region — the project's root

The **[region](glossary.md#region)** is the top of the tree and the project as a
whole. Its main job is to hold **[caves](glossary.md#cave)** — one project can
carry a whole system's worth. When a project has a single cave, the region often
just takes that cave's name, which is why the bold name at the top of the
[Data page](../survey-data/caves-and-trips.md#add-a-cave) can look like it names
the cave; it names the region.

### Cave — one system

A **[cave](glossary.md#cave)** is a single cave system, and it's the level that
reports the numbers you watch — length and depth — kept current as you enter data.
It's also the level a **coordinate system** is chosen on: georeferencing places a
cave on the real-world map. See
[Directions and Coordinate Systems](coordinate-systems.md).

### Trip — the load-bearing level

A **[trip](glossary.md#trip)** is *one team, one day, one set of instruments*, and
it is the most important level to understand, because it's where the **corrections
live**. A trip owns its [calibration](../survey-data/calibration.md) and its
[declination](../survey-data/declination.md) — and that's not a filing convenience,
it's a measurement fact: everything surveyed on a given day, with the same tape and
compass, needs the same correction, and a different day with different instruments
needs a different one. Put those settings any higher and two teams couldn't be
corrected apart; put them any lower and you'd retype them for every shot.

A trip also owns its **team**, and the team is more than a record: each member's
name becomes a **Caver** [keyword](glossary.md#keyword) you can filter the whole
cave by. See
[Organize Caves and Trips](../survey-data/caves-and-trips.md#record-the-team).

### Survey chunk — a connected run

Within a trip, shots are grouped into
**[survey chunks](glossary.md#survey-chunk)** — the blocks of rows you type in the
[survey table](../survey-data/enter-survey-data.md). A chunk is a *continuous* run
that can't branch, so a side passage leaving the main line starts its own chunk.
That's why entering a branch means pressing Space for a new block rather than
typing on: the split is the model telling the truth about the cave's shape.

### Shot — the skeleton

A **[shot](glossary.md#shot)** is the atom of measurement: a leg between two
**[stations](glossary.md#station)**, carrying a distance, a compass bearing, and a
clino angle. Shots are the skeleton the whole 3D model hangs on — every station's
position is the result of following shots from a fixed point, adjusted so loops
close.

### Note and Scrap — the drawing side

A **[note](glossary.md#note)** is a drawing made in the cave — a scanned sketch, a
PDF, an SVG from an app like TopoDroid, or a LiDAR scan — attached to the trip that
drew it. See [Add Notes to a Trip](../notes/add-a-note.md).

A **[scrap](glossary.md#scrap)** is one piece of the map
[digitized](../scraps/digitize-a-scrap.md) from a note: an outline you trace, tied
to the stations drawn inside it. A note is usually split into several scraps, and
the scrap is where the drawing meets the survey — CaveWhere
[carpets](glossary.md#carpeting) it onto the 3D shots so the sketch drapes over the
real passage. The [scrap's type](../scraps/scrap-types.md) (plan, running profile,
projected profile) lives here because it's a fact about *that drawing*, not the
whole trip.

## Stations: the thread through both sides

The tree has two branches under a trip — **shots** on the measurement side, **notes
and scraps** on the drawing side — and
**[stations](glossary.md#station)** are what stitch them together. A station is a
named point (`A1`, `A2`); a shot connects two of them, and a scrap is anchored by
placing those same named stations on the sketch. Station names are how a scrap
knows where it belongs on the skeleton, and how a shot in one trip ties to a shot
in another. Because CaveWhere matches stations by name across the whole cave,
consistent naming is what lets separate trips join into one connected survey — and
a mistyped name is the usual reason a leg won't connect or a scrap lands in the
wrong place.

## Why an edit ripples

The payoff of knowing the tree is understanding why a small change isn't small.
The survey is *solved*, not just stored: CaveWhere runs [Survex](glossary.md#survex)
over the whole shot network to find each station's most likely position, spreading
loop-closure error across every leg. So when you fix one shot's distance, the
solve re-runs and stations *elsewhere* can move — that's loop closure doing its job,
not a glitch.

And because scraps are anchored to stations, when a station moves, every scrap tied
to it re-carpets to follow. You fix a reading in a table, and a sketch you drew
months ago re-drapes to match — automatically, with nothing to redraw. That
cascade, from shot up through the solve and back down through the scraps, is the
whole reason the model stays honest as the survey grows. See
[Why CaveWhere](why-cavewhere.md#keeping-the-map-correct-loop-closure) for the
mechanism, and [Scraps and Carpeting](../scraps/carpeting.md) for the re-draping.

## What cuts across the tree

Two things deliberately don't fit the strict parent-child hierarchy:

- **[Keywords](glossary.md#keyword)** tag caves, trips, teams, notes, and scraps
  alike, then drive
  [layer visibility](../view-3d/the-3d-view.md#focus-on-part-of-the-cave-layers)
  in the 3D view. They're how you focus on one part of a big cave without regard to
  where it sits in the tree.
- **Version history** belongs to the project as a whole, not any one level: every
  [save](../projects-and-files/save-a-project.md) records a version of the entire
  tree at once, which is why rolling back moves the whole project together.

## Next steps

- [Glossary](glossary.md) — terse definitions of every term above.
- [Organize Caves and Trips](../survey-data/caves-and-trips.md) — building the top
  of the tree in the app.
- [Directions and Coordinate Systems](coordinate-systems.md) — how a cave gets
  placed on the real-world map.
