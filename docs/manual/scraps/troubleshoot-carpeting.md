---
title: Troubleshoot the Carpet
summary: Why a carpet comes out distorted or missing, and how to find the cause: scale, rotation, station names, and scrap type.
problem: A morphed scrap looks stretched, creased, or misplaced and you need to find the cause.
keywords: [carpet, troubleshoot, distortion, warping, morphing, scale, rotation, station, seesaw, crease]
related: [carpeting.md, scrap-types.md, digitize-a-scrap.md, warping-settings.md]
---

# Troubleshoot the Carpet

## Why / when you need this

Carpeting bends a flat drawing onto measured 3D stations, so whatever went wrong
in the drawing arrives as geometry. A wrong scale makes the whole passage come
out uniformly too thin or too fat. An orientation off by some angle turns the
carpet by exactly that angle, because the note transform hands the value straight
to a rotation in the plane of the drawing. A station carrying a name the survey
uses somewhere else drags that corner of the sketch across the cave.

This page maps symptoms onto those causes.
[Choose the scrap type](scrap-types.md) covers the controls themselves, and
[What carpeting actually does](carpeting.md#what-carpeting-actually-does) covers
the morph stage by stage. The order below runs the cheapest check first. I have
no measurements on which cause turns up most often, so read it as a search plan
and not as a ranking.

## Nothing appears at all

Rule these out before you suspect the morph.

- **The passage has not solved.** The morph reads its station positions from the
  cave's position lookup, which the line plot fills in, so a passage that has not
  solved gives the morph nothing to bend onto. Open the
  [3D view](../view-3d/the-3d-view.md). No line plot there means the problem sits
  in the survey data.
- **The scrap is missing a half.** CaveWhere triangulates a scrap only once it
  holds at least 1 station and 1 outline point. A shape on its own never reaches
  the 3D view.
- **Automatic Update is off.** That one checkbox feeds the line plot manager and
  the scrap manager together, so clearing it stops the survey solve and the
  re-morph at the same time. See
  [Carpeting is automatic](carpeting.md#carpeting-is-automatic).
- **A running profile is down to 1 station.** It warps each point between the 2
  stations the point falls between along the page. With fewer than 2 the weighted
  position comes out as zero and every point collapses onto the survey's origin,
  so the carpet disappears rather than landing somewhere wrong.

## The carpet is the wrong size

Select the scrap and read the **Scale** row in Scrap Info, shown below.

![The Scrap Info panel with the Scale row highlighted: a measurement-tool button, the label Scale, an On Paper box reading 1 in, an equals sign, an In Cave box reading 516.99 in, and the resulting ratio 1:517.](../images/scraps-scale.png)
*The Scale row. CaveWhere reduces the equation to a ratio on the right, 1:517 on
this scrap.*

That ratio is the quickest number to read off. It will not usually match the
seed a new scrap starts on, 1:250 on a metric project and 1:240 on an imperial
one. The scrap shown above sits at 1:517 and carpets perfectly well. Look twice
at a ratio out by an order of magnitude.

Two messages can replace the ratio, both covered in
[Set the scale](scrap-types.md#set-the-scale). **"A scale of 1:1 or smaller is
bad!"** appears whenever the ratio reaches 1.0 or above, usually because Auto
Calculate had no shot to derive from. **"Invalid scale (check DPI)"** appears
whenever the ratio is not a finite positive number. That message names DPI
because a bad note resolution is the usual cause, but the check reads the ratio,
so a scale with nothing to derive from lands there too.

The quiet failure is the one to watch for. A scale off by the 39.37
inches-per-meter factor puts no message in the panel at all. It can ask for a
grid of hundreds of millions of cells, which CaveWhere clamps at 2048 points per
axis. What you get is a coarse carpet, and a `clamping pathological point grid`
line on stderr that nothing in the UI surfaces.

**A scale that comes out non-finite, or 0 or below, takes the rotation down with
it.** The page-to-cave matrix bails out and returns the identity matrix before it
ever applies the north, so the carpet stops scaling *and* stops rotating. What
you see is a carpet shrunk to the physical size of the sheet, sitting unrotated
on its stations. Fix the scale before you touch the north.

## The carpet is turned the wrong way

The orientation row sits inside the Auto Calculate box and changes name with the
type: **North** on a plan scrap, **Up** on either profile, described in
[North or up](scrap-types.md#north-or-up-orient-the-drawing) and shown below.

![The Scrap Info panel with the North row highlighted: a compass-arrow button, the label North, and the value 2.32 degrees. The Type above reads Plan and Auto Calculate is unchecked.](../images/scraps-north-up.png)
*The orientation row on a plan scrap, so it reads **North**. On either profile
type the same row reads **Up**.*

Whatever angle sits in that row becomes a rotation of the drawing in its own
plane, so the carpet turns by exactly your error. On a plan that is a compass
turn about the vertical; on either profile it tilts the sketch inside its
projection plane. Nothing in the code holds a tolerance or a warning threshold
for the angle, so I cannot give you a "close enough" figure. Compare the carpet
against the line plot in the 3D view and judge it by eye.

Two branches worth knowing:

- **A plan carpet also turns with the trip's declination. Only a plan.** The
  morph gets the scrap's north with the trip declination subtracted and, when
  Auto Declination is on, the cave's grid convergence added back on. A profile
  scrap is gravity-up, so its stored angle passes through untouched. Change a
  trip's declination and every plan carpet in that trip rotates while its
  profiles stay put. See [Declination](../survey-data/declination.md).
- **On a running profile, Up decides more than the tilt.** Before picking the
  stations to warp a point between, CaveWhere rotates the page by the note
  transform and sorts the stations by their x-coordinate in that rotated frame. A
  wrong Up re-sorts them, so points warp between the wrong pair rather than
  merely leaning.

## Part of the carpet is somewhere else

CaveWhere matches a drawn station to a surveyed one by name, across the whole
cave and not just the current trip, lowercasing both before comparing. It drops a
name the cave cannot place without a word, covered under
[Place the stations](digitize-a-scrap.md#place-the-stations).

**A typo that lands on a real station elsewhere is worse than one that lands on
nothing.** The morph weights every control point by the inverse square of its
distance to the point it warps, and it measures that distance **on the page**
rather than in the cave. A dot you drew inside your own outline therefore counts
as close, whatever coordinates its name resolves to. One mislabeled station near
the middle of a scrap can haul the surrounding area to the far side of the cave.

To find it, select suspect stations and remove them with Delete or Backspace, one
at a time, watching what settles. Two warnings before you start:

- **Nothing undoes it.** No Edit menu, no Ctrl+Z, and the undo stack never
  covered anything inside a scrap. On a `.cwproj` project,
  [Restore to here](../collaboration/review-history.md#restore-back-to-an-earlier-version)
  reaches back past the deletion; a bundled `.cw` file has no such fallback.
- **With Auto Calculate on, removing a station re-derives the scale and the north
  as well.** Two things then move at once and the carpet cannot tell you which.
  Uncheck Auto Calculate first and the transform holds still while you bisect.

## Choose the right scrap type

A drawing morphed as the wrong [type](scrap-types.md) distorts by construction,
because CaveWhere undoes a projection you never made. Each type then fails in its
own way.

**Plan and projected-profile scraps get localized bumps.** Each point takes a
weighted average over the nearest control points, 10 of them by default, so a
control point offset from its neighbors drags the carpet near it. On a plan that
offset reads as elevation. On a projected profile the scrap's view axis has been
pitched flat, so the same fault reads as a patch pushed toward you or away from
you rather than as a hump.
[Why does a carpet come out bumpy?](carpeting.md#why-does-a-carpet-come-out-bumpy)
has the mechanism. Three fixes, in the order I would try them:

1. Lower **Max closest stations** from its default of 10, so each part of the
   drawing follows a shorter run of line around it.
2. Raise the **smoothing radius** from its default of 2.0 m.
3. Split the sketch.

The first 2 live in [the warping settings](warping-settings.md), and they carry a
catch. Those settings sit in your CaveWhere preferences rather than in the
project, so they apply to every project you open on this computer, and changing
one re-morphs all of your scraps. Splitting a sketch only touches the sketch.
That is why I recommend reaching for the split before the knobs when a single
scrap misbehaves.

**Running profiles crease.** The sort that decides which pair of stations a point
warps between compares the page x-coordinate alone. Stations drawn one above
another in a shaft sit nearly tied on x, so which pair a point belongs to turns
on a fraction of a page unit. Neighboring points land in different pairs and tilt
opposite ways. That alternation is the crease. Two overlapping vertical scraps
warp independently, each seeing only its own stations, so they can disagree the
same way and seesaw against each other. Give a vertical drop a projected profile
instead.

Raising the smoothing radius will not help there. **The Gaussian pass hands a
running profile straight back untouched**, so the smoothing radius does nothing
at all to one.

## When in doubt, split the scrap

Several small scraps morph cleaner than one big one, for reasons you can check:

1. **Type, scale and orientation are per scrap.** A note holding a plan and a
   profile side by side has to become 2 scraps, or one of them goes into the
   morph wrong.
2. **A scrap only ever sees its own stations.** CaveWhere hands the morph the
   station list belonging to that scrap and skips any survey neighbor you did not
   draw on it, so a station in the next chamber cannot pull on it whatever Max
   closest stations says.
3. **Smoothing compares every point in a scrap against every other one.**
   Splitting a scrap in half roughly halves that work.

The price is bookkeeping. Every scrap needs its own type, its own outline, and
its own stations. Count on 1 station to carpet at all, and 2 with a shot between
them before Auto Calculate can derive the scale and the orientation for you.

## Where to go next

- Adjust morph density and smoothing: [Tune the warping
  settings](warping-settings.md).
- Review how each type projects: [Choose the scrap type](scrap-types.md).
- Follow the morph stage by stage: [What carpeting actually
  does](carpeting.md#what-carpeting-actually-does).
