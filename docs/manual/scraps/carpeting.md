---
title: Scraps and Carpeting
summary: How carpeting morphs a flat sketch onto the 3D survey, and how the chapter fits together.
problem: Turn flat passage sketches into a 3D model that stays correct as the survey changes.
keywords: [scrap, carpet, carpeting, morphing, warping, sketch, note, digitize, triangulation, compute scraps]
related: [../concepts/why-cavewhere.md, ../concepts/glossary.md, ../view-3d/the-3d-view.md]
---

# Scraps and Carpeting

## Why / when you need this

![A 3D view of a cave: hand-drawn passage sketches morphed and carpeted over the red survey line, their texture visible across two levels.](../images/scraps-carpet-orbit-poster.png)
*The payoff. Flat sketches morphed onto the 3D survey and carpeted over the line.
Everything in this chapter builds toward it.*

A surveyor's sketch is a flat drawing of a passage that twists, climbs, and
drops in three dimensions. Traditionally it stays 2D forever, and somebody
redraws the map by hand every time the survey changes. **Carpeting** is
CaveWhere's answer.

You trace pieces of your sketch as [scraps](../concepts/glossary.md#scrap) and
tie them to survey [stations](../concepts/glossary.md#station). CaveWhere bends
each scrap onto the 3D survey line, so the drawing lies over the real passage
like a carpet, as shown above, and rebuilds it whenever the survey moves. See
also [Why CaveWhere](../concepts/why-cavewhere.md#from-a-2d-sketch-to-a-3d-cave).

The toolbar button that opens this workflow is labeled **Carpet**; this manual
calls it *Carpet mode*.

## What carpeting actually does

Carpeting crops, scales, grids, and warps a flat scrap until it matches the 3D
survey line plot. Four stages:

1. **Crop.** CaveWhere crops the note image to the outline's bounding box and
   mipmaps it. That crop becomes the carpet's texture.
2. **Scale and rotate.** Every scrap carries a scale and a north (plan) or up
   (profile) direction. A wrong scale makes the passage come out thin or fat. A
   wrong rotation twists the sketch off the survey. **Auto Calculate** derives
   both from the stations you place, plus the plane's azimuth on a projected
   profile. [Choose the scrap
   type](scrap-types.md#auto-calculate-and-what-it-hides) covers switching it
   off.
3. **Grid.** CaveWhere lays a square grid over that bounding box, 0.5 m of cave
   by default, and throws away every point outside your outline. Squares
   straddling the outline get clipped against it, which is why a carpet ends on
   your traced line, not on a staircase of grid squares.
4. **Morph.** CaveWhere anchors the sketch to the stations you placed *and* to
   control points interpolated every 0.5 m along the shots between them, so the
   surveyed line inside the scrap steers the deformation. Each grid point
   follows the 10 nearest control points, measured on the page, weighted by the
   inverse square of their distance. Last, a Gaussian pass smooths the heights
   over a 2.0 m radius.

Those 4 numbers are the [warping settings](warping-settings.md): grid
resolution, shot interpolation spacing, max closest stations, and smoothing
radius. The defaults suit most caves.

The same engine morphs [LiDAR notes](../concepts/glossary.md#lidar-note) and
photogrammetry models into the cave, but a LiDAR mesh carries no warping
settings. It always morphs at the built-in 0.5 m spacing, uses *every*
interpolated control point rather than the 10 nearest, and never gets smoothed.
Changing the warping settings will not change how it lands.

## Why does a carpet come out bumpy?

Inverse-distance weighting is a blunt instrument, and plan scraps are where you
see it. Each grid point's height is a weighted average of the control points
around it, so a station sitting a couple of meters above its neighbors lifts the
carpet near it. The morph knows only where the stations landed and how the
straight line between them runs.

The fix is to constrain the morph rather than trust it. Lower **Max closest
stations** so each point is fitted to a shorter run of line, raise the smoothing
radius, or split the sketch into smaller scraps.
[Troubleshoot the carpet](troubleshoot-carpeting.md#choose-the-right-scrap-type)
works through that.

Three more rough edges:

- **A running profile warps between exactly 2 stations**, the pair your point
  falls between along the page. Give it fewer than 2 and every point collapses
  onto the survey's origin, so the carpet vanishes rather than landing wrong.
- **A degenerate scale can ask for a grid of hundreds of millions of cells.**
  The classic cause is a note scale off by the 39.37 inches-per-meter factor.
  CaveWhere clamps the grid at 2048 points per axis and logs `clamping
  pathological point grid`. Without the clamp, one bad scrap pins every core!
- **A carpet whose texture won't load comes back empty.** If the crop fails,
  CaveWhere logs `Problem cropping image, does it not exist` and returns an
  empty result. The scrap never appears in the 3D view, with no warning in the
  UI.

## Carpeting is automatic

You do not normally trigger carpeting. CaveWhere marks a scrap dirty and
re-morphs it in the background when you edit the outline, add or move or rename
a station, change its scale, north/up direction, or type, or move a lead. A
scrap open for editing sits the round out; the recompute fires once you finish.

The survey re-solves on every edit as well, so **closing a loop or fixing a
blunder re-carpets everything affected on its own**. Correct the survey once,
and the drawings follow.

One checkbox governs the whole arrangement: **Automatic Update**, at the bottom
of the sidebar, on by default. I recommend leaving it checked, and clearing it
only on a very large project where a recompute is genuinely in your way. **Turn
it off and CaveWhere stops recomputing** — and that covers a great deal more
than carpeting. One switch feeds the line plot manager and the scrap manager, so
the **survey solve stops too** and **loop closure stops running** with it. Turn it
back on and CaveWhere catches up at once. The setting is per-machine and
survives a restart, so a box you cleared last week is still clear today.

## The workflow at a glance

Building a carpet is 4 tasks, each on its own page:

1. **[Digitize a scrap](digitize-a-scrap.md)**: trace the outline and place the
   stations that fall inside it.
2. **[Choose the scrap type](scrap-types.md)**: plan, running profile, or
   projected profile.
3. **[Troubleshoot the carpet](troubleshoot-carpeting.md)**: scale, rotation,
   station labels, type.
4. **[Tune the warping settings](warping-settings.md)**: grid density and
   smoothing.

The carpets appear in the [3D view](../view-3d/the-3d-view.md) over the survey
line plot, and keyword layers hide and show them.

## Advanced: recompute and visibility toggles

Two developer controls live under **File → Debug**:

- **Compute Scraps** marks every scrap dirty and recomputes the lot. It exists
  for testing, but it will refresh a carpet that looks stale.
- **Scraps Visible** toggles the carpets in the 3D scene. The name undersells
  it: carpets and morphed LiDAR meshes share one render list, so this hides your
  LiDAR notes too. For normal work, use
  [keyword layers](../view-3d/the-3d-view.md#focus-on-part-of-the-cave-layers)
  instead.
