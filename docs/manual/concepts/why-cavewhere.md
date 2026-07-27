---
title: Why CaveWhere
summary: The problems CaveWhere solves for cave surveyors, and how its main features map to them.
problem: Understand what CaveWhere is for before learning how to use it.
keywords: [overview, morphing, loop closure, git sync, lidar, keywords, layers, project format, why]
related: [glossary.md]
---

# Why CaveWhere

We use cave maps as a tool for exploration. Without them, leads get lost and
projects stagnate. Plenty of caves have twenty years of survey data and no
finished map. CaveWhere turns that pile into an accurate, shareable 3D map.

This page lists those problems. The rest of the manual makes more sense after
it.

## The guiding idea: a living map that helps you find cave

I built CaveWhere to answer one question fast: **where is there more cave to
find?** A map that takes weeks of drafting is a historical record. A map that
updates the moment you enter your survey is a *tool*. You can see the
[leads](glossary.md#lead) and which way the surveyed passages head, before the
next trip.

The rest follows from that. 2D-to-3D [morphing](#from-a-2d-sketch-to-a-3d-cave)
gives you a working map as soon as you enter the data, not months later.
[Loop closure](#keeping-the-map-correct-loop-closure) and automatic re-morphing
keep it trustworthy as the survey grows. [Sync](#working-as-a-team-sync) keeps
the whole team on the same current map.

## The long-term goal: salon-quality maps in CaveWhere

Cave survey is a team effort in CaveWhere. Everyone captures data,
[Sync](#working-as-a-team-sync) merges it without a merge conflict to resolve
by hand, and the working map stays current for the whole team. Then, at
cartography, the team effort ends: today one person exports a view and
redraws it alone, by hand, in Adobe Illustrator or Inkscape.

**Cartography should be a team effort too.** That's the long-term goal:
digital survey from a phone or tablet underground, synced the moment you're
back on the surface, and drawn by CaveWhere into an award-winning,
salon-quality 2D cave map. Finished by the team, not redrawn by one person.

## From a 2D sketch to a 3D cave

You draw the passage in TopoDroid or with a pencil on waterproof paper, and
either way you get a flat drawing of something that twists and climbs in three
dimensions. Traditionally it stays that way: the sketches never become a 3D
model, and even the 2D map gets redrawn by hand every time the survey data
changes.

CaveWhere's signature feature, *[carpeting](glossary.md#carpeting)*, morphs
those sketches into 3D. You digitize pieces of a sketch as
[scraps](glossary.md#scrap), tie them to survey [stations](glossary.md#station),
and CaveWhere deforms each scrap onto the 3D survey line so the drawing carpets
the passage. The model comes out of the drawings you already
make in the cave.

## Keeping the map correct: loop closure

**The problem:** Every instrument reading carries a little error. When a survey
route comes back to a station it already visited, a *loop*, those errors add up
and the two paths don't quite meet. A large mismatch usually means a
[blunder](glossary.md#loop-closure): a transposed digit, a compass read
backwards, a mistyped distance.

**What CaveWhere does:** CaveWhere finds the loops and reports the misclosure as
a percentage of loop length. Under about 0.5% counts as a tight loop. Between
0.5% and 2% is normal for longer loops or older instruments. Past about 5%, you
almost certainly have a blunder. Go find it instead of shipping a warped map.
When the survey shifts, CaveWhere re-carpets the affected scraps on its own. You
don't redraw anything.

**Powered by Survex:** CaveWhere doesn't reinvent the survey math. It embeds
[Survex](glossary.md#survex) 1.4.21, the open-source engine cavers use on the
world's largest and most complex cave systems. Its `cavern` solver runs a real
least-squares network adjustment: instead of dumping all the misclosure on the
last shot, it spreads the error across the loop, weighted by each leg, to land
on the most likely position for every station.

Two things follow. Your caves close the way the rest of the caving world expects
(same solver as Survex's own `aven` viewer). And CaveWhere re-solves the whole
network on every edit, so the loop closure and the 3D line plot stay current,
with no export step.

## Working as a team: sync

**The problem:** A cave survey is a team effort, but sharing the project has
always been the awkward part. For a small cave you email the file around. Bigger
projects go on a file share (Dropbox, Google Drive, a network drive), which has
the same flaw: two people open the same project and one person's save quietly
overwrites the other's work. A few projects reach for Git, which does solve the
merge problem, but Git was built for programmers. Teaching a survey team
branches, commits, and merge conflicts becomes a project of its own.

**What CaveWhere does:** CaveWhere gives you **sync powered by Git, without
having to know Git.** Sign in to GitHub once, then press **Sync**. No commands,
no branches, no merge conflicts to resolve by hand. It is real Git underneath,
so you keep the full version history and can roll back to any earlier state.

When two cavers change different parts of the cave, CaveWhere merges them with a
three-way merge that understands caves, trips, shots, notes, and scraps, not
lines of text. Sync also lays the groundwork for in-cave digital survey on
phones and tablets, where several devices capture separately and merge on the
surface.

## Not losing the data: the project format

**The problem:** Cave survey data takes enormous time and money to collect,
often years of trips into remote caves. Most software holds your work in memory
until you remember to save. That puts an evening of data entry one crash, one
flat battery, one power cut away from gone — and nobody should have to re-survey
a cave because a laptop died.

**What CaveWhere does:** **CaveWhere writes your work to disk as you type it.**
Nothing sits unsaved, which frees
[Save](../projects-and-files/save-a-project.md) to mean something more useful:
it marks a version you can return to.

The `.cwproj` directory format is human-readable, machine-parseable, and
Git-backed, with atomic saves and full history, and you can recover the raw data
without CaveWhere at all. A bundled `.cw` file packs the same data, history
included, into one file for sharing. The catch with a bundle: every save repacks
the whole project, which you feel on a multi-cave project carrying years of
scans. I recommend `.cwproj` for anything you plan to keep working on, and `.cw`
for handing a finished cave to someone.
[Choose a Project Format](../projects-and-files/project-formats.md) has the full
comparison.

## Capturing passage shape faster: LiDAR notes

Hand-sketching passage shape takes time, and some detail never makes it onto
paper.

Scan a passage in seconds with a LiDAR-capable phone (or build a photogrammetry
model from photographs), export it as `.glb`, and drop it into your trip notes
next to the sketches. Scans tie to stations and get
[carpeted](glossary.md#carpeting) into the model just like sketched scraps.

A scan doesn't replace sketching. It records surfaces, and only surfaces:
airflow never shows up in a scan, and neither does which of the three ways on
deserves pushing. PolyCam works best, since its scans arrive upright and
life-size. Photogrammetry models usually need their up direction and scale set
by hand. See [LiDAR Notes](../notes/lidar-notes.md).

## Focusing on one part of a big cave: keywords and layers

As a project grows to dozens of trips and hundreds of stations, it gets hard to
focus on the part you care about.

Tag caves, trips, teams, notes, and scraps with
[keywords](glossary.md#keyword), then filter on them to hide everything in the
3D view except the section that matters.

## Where to go next

- New to the vocabulary? Start with the [glossary](glossary.md).
- Ready to survey? The Getting Started and Survey Data chapters walk through a
  first project. (Still in progress; see [the manual index](../index.md) for
  what exists so far.)
