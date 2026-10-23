---
title: Attach a PocketTopo Survey
summary: How to attach a PocketTopo .top file so its trips solve in CaveWhere while you keep surveying in PocketTopo.
problem: Use DistoX/PocketTopo survey data in CaveWhere without retyping it and without losing the PocketTopo file as the master copy.
keywords: [pockettopo, top, distox, attach, external centerline, triples, splay, series, declination, fix]
related: [import-surveys.md, ../survey-data/declination.md, ../survey-data/caves-and-trips.md, ../survey-data/survey-errors.md, ../georeferencing/georeference-a-cave.md]
---

# Attach a PocketTopo Survey

## Why / when you need this

A DistoX team does its surveying in PocketTopo, on the phone, and that file keeps
growing trip after trip. Retyping it into CaveWhere means two copies that drift
apart by the next trip. Attaching keeps PocketTopo as the master: CaveWhere keeps
a copy of the `.top` in the project and reads it again on every solve, so its
shots never become CaveWhere shots you could edit by accident.

CaveWhere reads the binary `.top` file only (version 3, the one current
PocketTopo writes). The `.txt` export is not supported, and PocketTopo has no
entry in the **Import** menu.

## Attach the file

1. Choose **Attach survey file…** from a node's add menu in the survey tree.
2. At **Pick the entry file:**, browse to the `.top` file.
3. Choose **Attach**, the recommended option. **Import a copy** stays grayed out,
   because "Import supports Survex files only."

You get a node of kind **PocketTopo file**, holding one trip for each PocketTopo
trip. A file with a single trip still gets the node, so the trips you survey
next weekend have somewhere to land.

## How CaveWhere names and orders the trips

CaveWhere identifies each trip by its position in the file's recording order:
`#1`, `#2`, and so on. The display name comes from PocketTopo: the trip date plus
the first line of its comment (`2026-02-05 Robin survey`), or the date alone when
the comment is empty. CaveWhere sets the name once, when it creates the trip,
so renaming it afterward is safe.

Every comment line after the first is read as a team member name, one per line,
so put only surveyor names there.

**Never delete a trip from the middle of the PocketTopo file.** Every trip after
it shifts down one position, and on the next update `#4`'s notes, scraps and
equates land on what used to be `#5`. Add trips at the end and leave old ones
alone.

## Station names

PocketTopo writes `1.5` for station 5 of series 1, and CaveWhere shows it as
`1.5`. Survex can't hold a plain station `1` next to a series `1.x`, so when a
file has both, CaveWhere renames the plain one to `1_`. A scan warning lists each
rename:

> survey.top: Survex can't hold a station N next to a series N.x, so CaveWhere renames 0 is 0_.

Use the renamed form when you tie in other trips or place stations on notes.

A zero-length leg between two named stations is how PocketTopo joins series, so
CaveWhere turns it into an equate rather than a 0 m shot.

## Repeated shots (DistoX triples) and splays

CaveWhere keeps every reading. The solve averages the three readings of a triple
into one leg with the weight of one reading, and counts its length once. A reading
taken backward (to → from) gets flipped, 180° on the azimuth and the clino
negated, so either direction works.

CaveWhere gives no warning when the three readings disagree, and does no
reweighting. A 2 m blunder in one reading of a triple just pulls the average.
I recommend checking your triples in PocketTopo before you copy the file off
the phone, where you can see all three side by side.

Splay shots come through as splays.

## Declination

When the PocketTopo trip has a declination value, the file owns it. The trip
shows the value read-only, and you change it in PocketTopo.

When PocketTopo has declination set to Auto, the trip shows "file auto" and
CaveWhere computes the value, as it does for a native trip in
[Auto](../survey-data/declination.md), once the node or a node above it has a
fix station with a coordinate system. Until then the trip applies 0. The value stays read-only
either way, since the file asked for it.

## Fix stations to coordinates

PocketTopo references become fix stations, converted from millimeters to meters.
The file stores no coordinate system, so the **Fix Stations** page shows each of
them as `none → origin` and places the cave at the project origin. Pick the
system under **Read fixes with no coordinate system as**, see
[Georeference a Cave](../georeferencing/georeference-a-cave.md). Pick the wrong
one and the cave lands in the wrong place on the map, possibly by hundreds of
kilometers, with nothing else to tell you.

## What CaveWhere leaves out

CaveWhere drops shot comments, the flipped flag, roll, plan and side sketches,
and shots that belong to no trip. One warning per file gives the counts:

> survey.top: CaveWhere leaves out 4 shot comments, 2 flipped shots, 312 plan drawing elements.

Sketch import may come later. PocketTopo bakes its own declination into the
sketch coordinates, which makes them harder to place than the shots.

## Update after more surveying

Copy the newer `.top` over the file you attached. The banner then says "1
external source has changed since it was copied into this project." Choose
**Update all**, or **Show** and **Update** for this file alone. **Reload** on
the node does the same. If you saved it somewhere new, choose **Replace…** and
pick it. New trips appear as `#3` and onward, and the existing
trips keep their names, notes and equates.

## Troubleshoot

A file CaveWhere can't read stops the attach with `cannot read PocketTopo file`
and one of these:

- **"The file is not a PocketTopo .top file."** The file lacks PocketTopo's
  header. Check that you picked the `.top` and not the `.txt`.
- **"The PocketTopo file is version 2; only version 3 can be read."** An old or
  future PocketTopo wrote it; the message names the version it found. Open and save it in a current PocketTopo.
- **"The PocketTopo file ends at byte 1234, inside a shot."** The file got cut
  short, usually by an interrupted copy off the phone. The message gives the
  file's length and the part it was reading, such as a trip or a shot. Copy it again.

A Survex file that `*include`s a `.top` gets "cavern cannot include a PocketTopo
file; attach it on its own instead". Attach the `.top` as its own node.

Scan warnings, the renames and left-out counts above included, also appear on
the **Attached external centerlines** section of the cavern output page. See
[Survey Errors](../survey-data/survey-errors.md) for what the solve reports.
