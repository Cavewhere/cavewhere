---
title: Glossary
summary: Definitions of the cave-surveying and CaveWhere terms used throughout the manual.
problem: Understand the survey and CaveWhere vocabulary the rest of the manual assumes.
keywords: [glossary, terms, station, shot, lrud, scrap, declination, grid convergence, loop closure, backsight, trip, lead, morphing, warping]
related: [why-cavewhere.md]
---

# Glossary

Definitions of the terms the rest of the manual uses. Where a term names a
CaveWhere feature, the relevant chapter covers it in depth.

## The data model

CaveWhere organizes a project as a nested hierarchy:
[Region](#region) → [Cave](#cave) → [Trip](#trip) →
[Survey chunk](#survey-chunk) → [Shot](#shot) / [Note](#note) →
[Scrap](#scrap).

### Region
The root of a CaveWhere project. A region holds all of the [caves](#cave) in the
project.

### Cave
A single cave system. A cave contains one or more [trips](#trip).

### Trip
One surveying expedition — a team going into the cave on a given day and
recording measurements and sketches. A trip owns the [shots](#shot) surveyed and
the [notes](#note) drawn on that trip, plus its [calibration](#calibration) and
team.

### Survey chunk
A continuous run of connected [shots](#shot) within a trip — the rows you enter
in the survey table. A trip can have several chunks.

## Survey measurements

### Station
A labeled point in the cave — a marked spot on the wall or floor — that
[shots](#shot) connect. Station names (e.g. `A1`, `A2`) tie the survey together
and anchor [scraps](#scrap) and [leads](#lead).

### Shot
A measured leg between two stations: a *from* station and a *to* station, with a
**distance** (tape), a **compass** bearing, and a **clino** (inclination) angle.
Shots are the skeleton of the cave.

### Backsight
A reading of the same [shot](#shot) taken in the reverse direction — sighting
from the *to* station back to the *from* station. Comparing the foresight and
backsight catches reading errors. See [calibration](#calibration) for how
CaveWhere corrects backsight instruments.

### LRUD
**Left, Right, Up, Down** — the distances from a [station](#station) to the
passage walls, ceiling, and floor. LRUDs give the passage its width and height,
turning a line into a shaped tube.

### Splay
An extra shot from a station out to a wall feature, used to capture passage
shape beyond the LRUDs. CaveWhere ignores splay shots on import of some formats.

## Corrections and coordinates

### Calibration
The set of corrections applied to raw instrument readings before they're used:
tape (distance) offset, front-sight and back-sight compass/clino corrections,
and [declination](#declination). Calibration accounts for the specific
instruments a team used so different teams' data lines up.

### Declination
The angle between **magnetic north** (where the compass points) and **true
north**. It varies by location and drifts over time. If you don't correct for
declination, the entire map is rotated off true north. CaveWhere can look up
declination automatically from the IGRF model or take a manual value.

### Grid convergence
The angle between **true north** and **grid north** — the "up" direction of a
map projection's grid (e.g. a UTM zone). It varies with projection and location.
When a project is [georeferenced](#georeferencing), the bearing correction
applied to each reading is *(declination − grid convergence)*, so corrected
bearings align to grid north, matching the projected coordinate system.

### Georeferencing
Tying the cave to real-world coordinates by [fixing](#fixed-station) one or more
stations to known positions in a chosen coordinate system, so the survey lines
up with the surface world and other datasets.

### Fixed station
A [station](#station) pinned to a real-world coordinate. Fixing a station is how
a cave gets placed on the map.

## Notes and drawings

### Note
A drawing attached to a [trip](#trip) — a scanned sketch image, an imported PDF,
a digital sketch imported as SVG (for example from TopoDroid), or a
[LiDAR](#lidar-note) scan. Notes are the raw drawings a surveyor makes in the
cave, from which [scraps](#scrap) are digitized.

### Scrap
A digitized outline traced from a [note](#note) — one piece of the cave map.
Scraps are tied to [stations](#station) and morphed into 3D by
[carpeting](#carpeting). A note is usually digitized as several scraps.
Scrap **type** (plan, running profile, projected profile) tells CaveWhere how to
project the drawing.

### Carpeting
CaveWhere's name for its signature capability: morphing a flat [scrap](#scrap)
into 3D by deforming it so its drawn [stations](#station) land on the real 3D
[shot](#shot) positions, draping the sketch over the actual passage like a
carpet. The same process applies to [LiDAR notes](#lidar-note) and
photogrammetry models — they carpet into the cave model just like sketched
scraps. Also referred to as *warping* or *morphing*. See
[Why CaveWhere](why-cavewhere.md#from-a-2d-sketch-to-a-3d-cave).

### Lead
A noted unexplored passage — a "go" — recorded so a future trip knows where
there's more cave to survey. Leads can carry size estimates and be exported to
CSV.

## Analysis and organization

### Loop closure
When [shots](#shot) form a closed loop, the accumulated measurement error means
the loop doesn't perfectly meet itself — the **misclosure**. A large misclosure
signals a **blunder** (a transposed digit, reversed reading, or mistyped value)
to find and fix. Closing loops distributes the small remaining error and keeps
the map accurate.

### Survex
The open-source cave-survey processing engine CaveWhere embeds to compute the
line plot and [loop closure](#loop-closure). Its `cavern` solver performs a
least-squares network adjustment that distributes misclosure error across the
survey network. Survex is a long-established, widely trusted standard in the
caving community; because CaveWhere runs it as a built-in library, the survey
re-solves automatically on every edit. See
[survex.com](https://survex.com).

### Keyword
A tag applied to caves, trips, teams, notes, or scraps. Keywords drive filtering
and **layer visibility** so you can show or hide parts of a large cave in the 3D
view.

### LiDAR note
A 3D scan (glTF/GLB) imported as a [note](#note). LiDAR notes tie to stations
and [carpet](#carpeting) into the cave model like sketched scraps, capturing
passage detail that would take hours to draw.
