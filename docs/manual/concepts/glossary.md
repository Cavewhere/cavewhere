---
title: Glossary
summary: Definitions of the cave-surveying and CaveWhere terms used throughout the manual.
problem: Understand the survey and CaveWhere vocabulary the rest of the manual assumes.
keywords: [glossary, terms, station, shot, lrud, scrap, declination, grid convergence, loop closure, backsight, trip, lead, morphing, warping]
related: [../getting-started/why-cavewhere.md]
---

# Glossary

Definitions of the terms the rest of the manual uses. Where a term names a
CaveWhere feature, the relevant chapter covers it in depth.

## The data model

CaveWhere organizes a project as a hierarchy 5 levels deep:
[Region](#region) → [Cave](#cave) → [Trip](#trip) →
[Survey chunk](#survey-chunk) → [Shot](#shot) / [Note](#note) →
[Scrap](#scrap).

### Region
The root of a CaveWhere project. A region holds all of the [caves](#cave) in the
project.

### Cave
A single cave system. A cave contains one or more [trips](#trip).

### Trip
One surveying expedition: a team going into the cave on a given day and recording
measurements and sketches. A trip owns the [shots](#shot) surveyed and the
[notes](#note) drawn on that trip, plus its [calibration](#calibration) and team.

### Survey chunk
A continuous run of connected [shots](#shot) within a trip, the rows you enter in
the survey table. A trip can hold several chunks.

## Survey measurements

### Station
A labeled point in the cave, a marked spot on the wall or floor, that
[shots](#shot) connect. Station names (`A1`, `A2`) tie the survey together and
anchor [scraps](#scrap) and [leads](#lead). CaveWhere matches them
case-insensitively, so `a1` and `A1` reach the same point.

### Shot
A measured leg between two stations, a *from* station and a *to* station, carrying
up to 5 readings: a **distance**, plus front and back **compass** bearings and
**clino** (inclination) angles. Shots form the skeleton of the cave.

### Backsight
A reading of the same [shot](#shot) taken in the reverse direction, sighting from
the *to* station back to the *from* station. Comparing the foresight and backsight
catches reading errors. See [calibration](#calibration) for how CaveWhere corrects
backsight instruments.

### LRUD
**Left, Right, Up, Down**: the 4 distances from a [station](#station) to the
passage walls, ceiling, and floor. LRUDs give the passage its width and height,
turning a line into a shaped tube.

### Splay
An extra shot from a station out to a wall feature, used to capture passage
shape beyond the LRUDs. CaveWhere ignores splay shots when importing some formats.

## Corrections and coordinates

### Calibration
The corrections applied to raw instrument readings before use: a distance offset,
front-sight and back-sight compass and clino corrections, and
[declination](#declination). Calibration accounts for the specific instruments a
team carried, so different teams' data lines up.

### Declination
The angle between **magnetic north** (where the compass points) and **true
north**. It varies by location and drifts over time, and skipping it rotates the
entire map off true north. CaveWhere looks the value up automatically from
IGRF-14 or takes one you type.

### Grid convergence
The angle between **true north** and **grid north**, the "up" direction of a map
projection's grid (a UTM zone, say). It varies with projection and location. Once
a project is [georeferenced](#georeferencing), the bearing correction applied to
each reading becomes *(declination − grid convergence)*, so corrected bearings
align to grid north and match the projected coordinate system.

### Georeferencing
Tying the cave to real-world coordinates by [fixing](#fixed-station) one or more
stations to known positions in a chosen coordinate system, so the survey lines
up with the surface world and other datasets.

### Fixed station
A [station](#station) pinned to a real-world coordinate. Fixing a station is how
a cave gets placed on the map.

## Notes and drawings

### Note
A drawing attached to a [trip](#trip): a scanned sketch image, an imported PDF, a
digital sketch imported as SVG (for example from TopoDroid), or a
[LiDAR](#lidar-note) scan. Notes hold the raw drawings a surveyor makes in the
cave, and [scraps](#scrap) get digitized from them.

### Scrap
A digitized outline traced from a [note](#note), one piece of the cave map. Scraps
tie to [stations](#station) and morph into 3D by [carpeting](#carpeting). One note
usually becomes several scraps. A scrap's **type**, 1 of 3 (plan, running profile,
projected profile), tells CaveWhere how to project the drawing.

### Carpeting
CaveWhere's name for its signature capability: morphing a flat [scrap](#scrap)
into 3D by deforming it so its drawn [stations](#station) land on the real 3D
[shot](#shot) positions, laying the sketch over the actual passage like a
carpet. The same process handles [LiDAR notes](#lidar-note) and photogrammetry
models, which carpet into the cave model just like sketched scraps. Also called
*warping* or *morphing*. See
[Why CaveWhere](../getting-started/why-cavewhere.md#from-a-2d-sketch-to-a-3d-cave).

### Lead
A noted unexplored passage, a "go", recorded so a future trip knows where more
cave waits. Leads can carry size estimates and export to CSV.

## Analysis and organization

### Loop closure
When [shots](#shot) form a closed loop, accumulated measurement error keeps the
loop from meeting itself exactly. That gap is the **misclosure**, reported as a
percentage of loop length: under about 0.5% counts as tight, and past about 5%
you almost certainly have a **blunder** to find (a transposed digit, a reversed
reading, a mistyped value). Closing loops spreads the small remaining error and
keeps the map accurate.

### Survex
The open-source cave-survey processing engine CaveWhere embeds to compute the
line plot and [loop closure](#loop-closure). Its `cavern` solver runs a
least-squares network adjustment that spreads misclosure error across the survey
network. Cavers use the same engine on the world's longest cave systems through
its own `aven` viewer. CaveWhere links Survex 1.4.21 in as a library instead of
shelling out to it, so the survey re-solves on every edit. See
[survex.com](https://survex.com).

### Keyword
A tag applied to caves, trips, teams, notes, or scraps. Keywords drive filtering
and **layer visibility**, so you can show or hide parts of a large cave in the 3D
view.

### LiDAR note
A 3D scan imported as a [note](#note), in glTF's binary `.glb` format. LiDAR
notes tie to stations and [carpet](#carpeting) into the cave model like sketched
scraps, capturing passage detail that would take hours to draw.
