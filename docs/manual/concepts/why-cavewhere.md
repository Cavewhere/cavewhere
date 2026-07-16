---
title: Why CaveWhere
summary: The problems CaveWhere solves for cave surveyors, and how its main features map to them.
problem: Understand what CaveWhere is for before learning how to use it.
keywords: [overview, morphing, loop closure, git sync, lidar, keywords, layers, project format, why]
related: [glossary.md]
---

# Why CaveWhere

## Why / when you need this

Read this first. CaveWhere has a lot of features, and each one exists to solve a
specific, long-standing problem in cave surveying. If you understand the
problems, the rest of the manual — and the app — makes far more sense.

Cavers measure passages by hand in the dark: a tape, a compass, a clinometer,
and a pencil sketch on waterproof paper — or, increasingly, a digital sketch
drawn in a mobile app like TopoDroid and exported as SVG. Turning thousands of
those raw measurements and sketches into an accurate, shareable 3D map is the
hard part. CaveWhere is built to do exactly that, and to keep the map correct
and in sync as a team keeps surveying.

## The guiding idea: a living map that helps you find cave

CaveWhere was built to answer one question fast: **where is there more cave to
find?** A map that takes weeks of drafting to finish is a historical record. A
map that updates the moment you enter your survey is a *tool* — you can see
where the [leads](glossary.md#lead) are and where the passages you've surveyed
are heading, all before the next trip.

Everything else in CaveWhere serves that goal. Quick 2D-to-3D
[morphing](#from-a-2d-sketch-to-a-3d-cave) means a working map exists as soon as
you've entered data, not months later. [Loop closure](#keeping-the-map-correct-loop-closure)
and automatic re-morphing keep it trustworthy as the survey grows.
[Sync](#working-as-a-team-sync) means the whole team is looking at the same
current map. The point of an always-up-to-date map is not tidiness — it's using
what you know to go find what you don't.

## The long-term goal: salon-quality maps in CaveWhere

The second overarching goal is to produce **award-winning, salon-quality 2D cave
maps entirely within CaveWhere** — finished cartography good enough for
publication and map salons, without leaving the app.

**This does not exist yet.** Today CaveWhere produces the *working* map — the
fast, always-current draft you use to survey and find cave. To make a
presentation-quality final map, the current workflow is to export a view of the
working map and redraw it in an external illustration tool such as Adobe
Illustrator. Closing that gap — bringing full finished-map cartography into
CaveWhere so the working map and the published map are one continuous
document — is a long-term direction for the project, not a feature you'll find
in the app today.

## From a 2D sketch to a 3D cave

**The problem:** A surveyor's sketch is a flat drawing of a passage that twists
and climbs in three dimensions. Traditionally the finished map stays 2D — the
sketches are never turned into a 3D-aware model — and even that 2D map has to be
redrawn by hand whenever the underlying survey data changes.

**What CaveWhere does:** CaveWhere's signature feature — called
*[carpeting](glossary.md#carpeting)* — morphs your 2D cave notes into 3D. You
digitize pieces of a sketch as [scraps](glossary.md#scrap), tie them to survey
[stations](glossary.md#station), and CaveWhere deforms each scrap onto the 3D
survey line so the sketch drapes over the real passage geometry like a carpet.
The result is a 3D model built from the drawings you already make in the cave.

## Keeping the map correct: loop closure

**The problem:** Every instrument reading has a little error. When a survey
route comes back to a station it already visited — a *loop* — those errors add
up and the two paths don't quite meet. A large mismatch usually means a
[blunder](glossary.md#loop-closure): a transposed digit, a reversed compass
reading, a mistyped distance.

**What CaveWhere does:** CaveWhere detects loops and reports the misclosure so
you can find and fix blunders instead of shipping a warped map. When the
underlying survey shifts — because you fixed a reading or closed a loop —
CaveWhere automatically re-carpets the affected scraps so the map stays
consistent with the data. You don't redraw anything.

**Powered by Survex:** CaveWhere doesn't reinvent the survey math — it embeds
[Survex](glossary.md#survex), the open-source cave-surveying engine cavers have
trusted for decades to process the world's largest and most complex cave
systems. Survex's `cavern` solver does a proper least-squares network
adjustment: rather than dumping all the misclosure onto the last shot, it
spreads the error across the whole loop, weighted by each leg, to find the most
statistically likely position for every station. Two benefits follow. First,
the results are the community standard — a cave closed in CaveWhere lines up the
way the rest of the caving world expects, and the same engine underlies Survex's
own tools. Second, because Survex is built in as a library rather than a
separate program, CaveWhere re-solves the whole network automatically on every
edit, so the loop closure and the 3D line plot are always current with no export
step.

## Working as a team: sync

**The problem:** Cave surveys are a team effort, but sharing the project has
always been painful. For a small cave you email the file around. Bigger projects
put it on a file share — Dropbox, Google Drive, a network drive — which suffers
the same problem: two people open the same project, and one person's save
quietly overwrites the other's work, or a change is lost to a stale copy. A few
projects reach for Git, which solves the merge problem technically — but Git was
built for programmers, and getting a survey team to learn branches, commits, and
merge conflicts is its own uphill battle.

**What CaveWhere does:** CaveWhere gives you **sync powered by Git, without
having to know Git.** You sign in to GitHub once and press a single Sync button;
there are no commands, branches, or merge conflicts to resolve by hand. Under
the hood it is real Git — so you still get full version history and can roll back
to any previous state — but the plumbing stays out of your way. When two cavers
change different parts of the cave, CaveWhere combines them with a smart
three-way merge that understands its own data model (caves, trips, shots, notes,
scraps), not just lines of text, so the edits merge cleanly. Sync also lays the
groundwork for in-cave digital survey on phones and tablets, where teams capture
on multiple devices and merge on the surface.

## Not losing the data: the project format

**The problem:** Cave survey data takes enormous time and money to collect —
often years of trips into remote caves. Losing it is not an option. Yet the normal
way software treats a document is to hold your work in memory until you remember to
save, which puts an evening of data entry one crash, flat battery, or power cut away
from being gone.

**What CaveWhere does:** CaveWhere's project formats are built around safety,
readability, and portability. **Your work is written to disk as you type it**, so
there is no unsaved window to lose — which frees [Save](../projects-and-files/save-a-project.md)
to mean something more useful: it marks a version you can return to. The `.cwproj`
directory format is human-readable, machine-parseable, and Git-backed, with atomic
saves and full version history so you can roll back to any previous state. A
bundled `.cw` file packs the same data (with history) into one file for easy
sharing. You can inspect or recover raw data without needing CaveWhere at all.

## Capturing passage shape faster: LiDAR notes

**The problem:** Hand-sketching passage shape is slow, and some detail is nearly
impossible to capture with a pencil.

**What CaveWhere does:** You can scan a passage in seconds with a LiDAR-capable
phone (or build a photogrammetry model), export it as glTF/GLB, and drop it into
your trip notes alongside traditional sketches. Scans tie to survey stations and
are [carpeted](glossary.md#carpeting) into the cave model just like sketched
scraps — the same morphing applies to LiDAR and photogrammetry, adding rich 3D
geometry.

## Focusing on one part of a big cave: keywords and layers

**The problem:** As a project grows to dozens of trips and hundreds of stations,
it becomes hard to focus on the area you're actually working on.

**What CaveWhere does:** You can tag caves, trips, teams, notes, and scraps with
[keywords](glossary.md#keyword), then filter and control layer visibility in the
3D view — showing or hiding parts of the cave by keyword so you can concentrate
on the section that matters.

## Where to go next

- New to the vocabulary? Start with the [glossary](glossary.md).
- Ready to survey? The Getting Started and Survey Data chapters walk through a
  first project. (Being written — see [the manual index](../index.md) for
  what's available.)
