---
title: Measure Distance and Bearing
summary: Measure the straight-line distance, azimuth, and inclination between two points in the 3D view, with the azimuth reported against grid, true, or magnetic north.
problem: You have a spatial question about the cave. How far from this station to that one, which way the dead end points, how much the passage climbs. The measurement tool answers all 3 from 2 clicks, without exporting anything.
keywords: [measure, measurement, distance, azimuth, bearing, inclination, grid north, true north, magnetic north, declination, grid convergence, station, snap, clipboard, ruler, 3d view]
related: [../view-3d/the-3d-view.md, ../georeferencing/grid-convergence.md, ../concepts/coordinate-systems.md]
---

# Measure Distance and Bearing

## Why / when you need this

The cave sits in the [3D view](../view-3d/the-3d-view.md) and you want a number
off it. How far from this station to that one, which way the dead end points, how
much the passage climbs on the way. The **measurement tool** answers all 3 at
once. Pick 2 points and CaveWhere reports the straight-line distance, the bearing,
and the vertical angle between them.

A shot carries those same 3 numbers. They come from somewhere else here, though:
the finished model, not a tape. The 3D view draws the *solved* survey, so a
station-to-station measurement inside a loop hands you the adjusted geometry
rather than the distance you wrote in the book. Outside a loop the two agree,
because cavern has nothing to distribute there.

Treat it as a scratch pad, not survey data. The measurement lives as long as you
look at it. A new pair replaces it, `Esc` throws it away, nothing reaches the
project file, and the survey never changes.

## Open the measurement tool

Measuring happens in the **3D view**. The floating toolbar at the bottom-left of
the view holds 3 tools: **Pick**, **Clip**, and **Measure**. Click **Measure**,
the ruler icon, tooltipped *Measure distance and bearing*. The screenshot below
shows it selected.

![The 3D view in plan with the Measure (ruler) tool highlighted in the bottom-left toolbar, beside Pick and Clip. A finished measurement runs between 2 endpoint dots with a 58.13 m distance chip on the line. The help box at the top of the view reads "Click to start a new measurement, or press Esc to exit."](../images/measurement-tool.png)
*The Measure tool, highlighted, in the 3D view's toolbar. The finished measurement
shown above runs between 2 endpoint dots, its distance on a chip at the midpoint.
The help box at the top says what a click will do next.*

Those 3 tools take turns. Turning **Measure** on turns **Pick** or **Clip** off,
and clicking **Measure** again hands the view back to the plain turn-table.
Leaving the tool discards the measurement, so `Esc` doubles as the delete key.

The cursor becomes a crosshair while the tool runs. Orbit and zoom keep working,
because right-drag and scroll go through to the turn-table, so you can swing the
cave around mid-measurement to line a shot up. Panning doesn't. The turn-table
pans on left-drag, and the measurement tool has taken left-click for placing
points, so nothing is left to forward. Orbit and zoom to reframe, or press `Esc`
and pan with the tool off.

## Place two points

A measurement runs between **2 points**. Hover the model and a marker previews
where the next point will land. The help box at the top of the view says what a
click will do:

1. **Click to place the first point.** The help box reads *"Click to place the
   first point."*
2. **Move the cursor.** A rubber band stretches from the first point to the
   cursor, with the running distance on a chip at its midpoint. You can read a
   distance by hovering, before committing the second point. The help box now
   reads *"Click to place the second point."*
3. **Click to place the second point.** The measurement freezes, the rubber band
   thickens into a solid line, and the readout panel appears. The help box
   switches to *"Click to start a new measurement, or press **Esc** to exit."*

Clicking again starts a fresh measurement and `Esc` puts the tool away. Only 1
measurement exists at a time, and while one sits frozen on screen the hover marker
goes away, so the first click of the next pair lands without a preview.

The line carries a second signal. While you choose the second point it draws thin
and slightly transparent, in the accent color. Move the cursor somewhere a click
wouldn't place anything and it switches to the danger color at full strength,
still tracking the cursor. A red band means a click there would place nothing:
either the ray missed every piece of geometry, or **Station only** is on and you
are not over a station.

### Snap to a survey station

Where a point lands depends on what sits under the cursor. By default (**Free**
placement) a click lands wherever your line of sight meets the model: a morphed
scrap, a point cloud, anywhere. Pass within **1.5 mm** of a **survey station** on
the centerline and the marker swaps its filled dot for a hollow ring. The point
then **snaps exactly to that station**.

CaveWhere measures that tolerance in physical millimeters on the glass, not in
pixels. The snap therefore covers the same patch of screen on a 4K laptop panel as
on an old 1080p monitor. The coordinate picker reads the same 1.5 mm from the same
constant, so the 2 tools can't drift apart.

The distinction between the 2 placements matters more than it looks. Free
placement gives an approximate answer by construction, because the surface you
clicked came out of somebody's traced outline, so a wall-to-wall number is worth
no more than the sketch behind it. A station has one exact position, and snapping
takes it whole.

Snapping only reaches the centerline. Triangles pick exactly where the ray hits
them, and a station hidden behind a wall never snaps at all, because the wall
returns the nearer hit. Orbit until the station comes into view. Where several
stations crowd into that 1.5 mm the snap may take a neighbor instead of the one
you meant, so zoom in until they separate.

To measure *only* between stations, turn on the **Station only** switch at the
bottom of the readout panel, shown below. Clicks then place a point when they snap
to a station, and do nothing over open passage. Reach for it when the number has
to mean a true station-to-station distance.

That switch has an awkward corner worth knowing about. It lives inside the readout
panel, and the panel only exists once a measurement is complete, so you can't arm
Station only before your first measurement. Measure any 2 points, flip the switch,
then measure the pair you actually wanted. Flipping it mid-measurement gates only
what happens next, and a point you already placed free stays put.

The mode lasts the session and no longer. Quit CaveWhere and it opens next time
back on Free, unlike the unit and the north reference, which both persist to disk.
Within a session it holds: press `Esc`, click **Measure** again, and the tool
comes back still in Station only, its help box now reading *"Click a survey
station to start measuring."*

**I recommend leaving Station only off** for most work, and turning it on for the
measurement you plan to write down. Free placement answers the question you
usually have, which is whether a dig connects, and that answer lives in rock
rather than in stations. A number headed for a report should land on something you
can name.

## Read the measurement

With both points down, a **Measurement** panel appears in the top-right of the
view. It groups the numbers the way a surveyor thinks about a shot.

![The Measurement readout panel. The unit selector reads m. Distance shows Straight-line (3D) 58.11 m and Horizontal (2D) 48.38 m. Direction shows Azimuth Grid 90.9 degrees and Inclination -33.6 degrees. By Axis shows Easting (X) +48.38 m, Northing (Y) -0.75 m, Vertical (Z) -32.19 m. The Station only switch is off and a Copy button sits at the bottom right.](../images/measurement-readout.png)
*One measured pair, in full. Every value is selectable monospace text, lengths at
2 decimal places and angles at 1.*

The readout shown above repays reading as arithmetic. Every group carries the same
vector in different clothes:

- **Distance**
    - **Straight-line (3D)**, `58.11 m` here, measures the direct distance through
      the air.
    - **Horizontal (2D)**, `48.38 m`, drops the height. It gives the length of the
      shadow the line would cast on a map, and the gap between the two says how
      steep the line runs. Here `hypot(48.38, 32.19)` returns the 3D distance,
      58.11 m.
- **Direction**
    - **Azimuth**, `90.9°`, gives the bearing from the first point to the second,
      in the range `0-360°`. You choose which north it counts from, and the
      [next section](#choose-which-north-the-azimuth-uses) covers that.
    - **Inclination**, `-33.6°`, gives the vertical angle, from `-90°` straight
      down to `+90°` straight up, with `0°` level.
- **By Axis** splits the same displacement into signed components: **Easting
  (X)**, **Northing (Y)**, and **Vertical (Z)**. A positive value carries an
  explicit sign, `+48.38 m`, and a value that rounds to zero prints no sign at
  all.

The sign carries the direction. As shown above, the second point sits 48.38 m
east, 0.75 m south, and 32.19 m below the first. Both angles fall out of those 3
numbers. A 0.75 m drift south against 48.38 m of east tips the bearing 0.9° past
due east. The 32.19 m of drop over 48.38 m of ground reads as 33.6° down.

CaveWhere works in a frame where **+X points east, +Y north, and +Z up**. Grid
north therefore means the model's own +Y axis, and a grid bearing needs no
coordinate transform at all. Two coincident points read 0 everywhere, and a purely
vertical shot reads `±90°` of inclination with the azimuth left at `0°`.

The panel doesn't appear during the live preview. Until the second click lands you
get the distance chip on the line and nothing else, so a bearing always costs you
both points. That limitation is worth knowing before you go hunting for a readout
that isn't there yet.

Something related happens underneath. Resolving true or magnetic north rebuilds a
PROJ transform and runs the geomagnetic model, so CaveWhere runs it only for a
frozen measurement whose panel sits expanded and on screen. The live preview and
the collapsed chip never pay that cost.

### Units, copying, and a narrow view

The **unit selector** in the panel header, reading `m` in the screenshot above,
switches every length at once between **m**, **km**, **ft**, and **mi**. It starts
on whatever the project's unit system implies: meters for a metric project, feet
for an imperial one. Change that system later and the readout follows. Pick a unit
yourself, though, and the choice sticks: it saves, it stops tracking the project,
and it waits for you in the next session.

Every value selects and copies on its own, and **Copy** drops the whole readout on
the clipboard as plain text, laid out like the panel:

```
Distance
  Straight-line (3D): 58.11 m
  Horizontal (2D): 48.38 m
Direction
  Azimuth (grid): 90.9°
  Inclination: -33.6°
By Axis
  Easting (X): +48.38 m
  Northing (Y): -0.75 m
  Vertical (Z): -32.19 m
```

The azimuth line carries its reference in parentheses, `grid`, `true`, or
`magnetic, today`, so nobody can misread a pasted bearing later.

Below 480 logical pixels of view width the panel collapses on its own to a
distance-only chip. The unit selector hides along with everything else, and a
press outside the 3D view dismisses the chip, which the expanded panel never does.
The **Expand** button in the header brings the full breakdown back, and pressing
it also takes the decision away from the width, so a panel you expanded by hand
stays expanded.

## Choose which north the azimuth uses

A bearing means nothing until you say north of what. The
[three norths](../concepts/coordinate-systems.md#three-norths) come apart the
moment a cave lands on a map. The selector sits in the **Azimuth** row, right of
the label, and the **?** button next to it spells the 3 choices out in the app.
The mnemonic: **Grid = the map, True = the globe, Magnetic = your compass**.

- **Grid** means north of the coordinate grid the model gets drawn in, the UTM
  grid lines, say. It comes as the default, it always works, and it needs no
  coordinate system at all, because it amounts to the scene's +Y axis. A grid
  bearing transfers straight onto a plan.
- **True** means geographic north, toward the pole. CaveWhere gets there by adding
  the [grid convergence](../georeferencing/grid-convergence.md) at this spot, the
  lean between the map's grid and the real meridians.
- **Magnetic (today)** means where a compass points now, which comes to true north
  less the magnetic
  [declination](../concepts/coordinate-systems.md#magnetic-to-true-declination).
  Hand this one to someone walking the surface with a compass.

As arithmetic, counting both corrections east-positive:

> **true = grid + convergence**
> **magnetic = grid + convergence − declination**

That reads the [grid-convergence page](../georeferencing/grid-convergence.md)'s
*grid bearing = magnetic reading + declination − grid convergence* backwards.

CaveWhere evaluates both corrections at the **first** point you placed, not at the
midpoint. Convergence varies with where you sit in the projection, so strictly the
correction at the first point isn't the correction at the second, and CaveWhere
doesn't interpolate between them. It takes the first point and moves on.

The declination comes from the IGRF-14 model that ships with Survex, evaluated for
**today's date in UTC**. That differs from the
[declination on your trips](../survey-data/declination.md), which carries the date
of the survey. A cave surveyed in 2004 and a magnetic bearing read off it today
answer questions about different years, so the 2 numbers may not agree.

**True and Magnetic need the cave
[georeferenced](../georeferencing/grid-convergence.md).** Turning a grid bearing
into either takes a real-world position and a coordinate system. On a local cave
both rows stay in the list but come up grayed out, leaving **Grid** as the only
one you can pick, and there grid north means straight up the model's own axes.
Georeference the cave and all 3 switch on.

**I read grid bearings for anything headed onto a plan** and switch to magnetic
only when somebody is about to walk the surface with a compass. Grid costs nothing
to compute and matches what the 3D view already draws, so it is the reference that
can't quietly disagree with the picture in front of you.

A few details are worth knowing before you trust a bearing:

- **CaveWhere remembers the choice** across sessions, alongside the length unit.
- **Losing the coordinate system snaps the selection back to Grid.** Clear a
  project's coordinate system with True selected and the readout won't keep
  reporting a stale correction.
- **A geographic coordinate system makes True equal Grid.** Latitude and longitude
  have no grid to lean against, so the convergence comes out 0 and the 2
  references agree exactly. The cave page reports that same case as
  `n/a (geographic CS)`, which reads like a failure but says the same thing: no
  grid, nothing to correct.
- **Hover the azimuth value** to see the corrections folded in. The tooltip rounds
  to 1 decimal, so the `0.74°` convergence the
  [cave page](../georeferencing/grid-convergence.md) reports appears here as
  `Convergence 0.7°`. A magnetic bearing appends `· Declination` and its own angle.
  Grid has nothing to fold in and shows no tooltip at all.
- **A set but unresolvable coordinate system reads `n/a`**, never a silently wrong
  grid number, and the tooltip carries the reason, such as *PROJ failed to create
  CRS for 'EPSG:999999'*.

## Next steps

- [The 3D View](../view-3d/the-3d-view.md) to navigate and aim the view you
  measure in.
- [Understand Grid Convergence](../georeferencing/grid-convergence.md) for why a
  true bearing and a grid bearing differ, and by how much.
- [Directions and Coordinate Systems](../concepts/coordinate-systems.md) for the
  3 norths in full, and what georeferencing gives a cave.
