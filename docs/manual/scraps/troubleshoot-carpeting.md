---
title: Troubleshoot the Carpet
summary: Why a carpet comes out distorted and how to fix it — scale, rotation, station labels, and scrap type.
problem: A morphed scrap looks stretched, creased, or misplaced and you need to find the cause.
keywords: [carpet, troubleshoot, distortion, warping, morphing, scale, rotation, station, seesaw, crease]
related: [scrap-types.md, digitize-a-scrap.md, warping-settings.md]
---

# Troubleshoot the Carpet

## Why / when you need this

Carpeting bends a flat drawing onto measured 3D survey stations, so anything
wrong with the drawing's scale, its orientation, or the stations it's tied to
shows up as a stretched, creased, or misplaced carpet. The good news is that
distortion almost always traces back to one of a handful of causes. Work through
them in order — the first two fix the large majority of problems.

## First, confirm the survey itself

Carpeting has nothing to morph onto until the survey computes. If a scrap won't
carpet at all, check the [3D view](../view-3d/the-3d-view.md) first: if the
line plot for that passage isn't there, fix the survey data before touching the
scrap.

## Scale and rotation (the usual culprits)

Scale and rotation are the two most important values for an accurate carpet, and
they cause most distortion:

- **Wrong scale** makes the passage come out unnaturally **thin or fat**. If a
  carpet looks pinched or bloated, re-check the scrap's scale in the
  [Scrap Info panel](scrap-types.md#north-or-up-orient-the-drawing) — either let
  **Auto Calculate** derive it from the stations or enter it by hand.
- **Wrong rotation** twists the sketch off the survey. Errors beyond about
  **ten degrees** are badly visible. Re-check the north (plan) or up (profile)
  direction on the page.

Validate both with the panel's own tools before looking anywhere else.

## Check the station names and positions

CaveWhere matches drawn stations to survey stations **by name across the whole
cave — not just the current trip.** A mislabeled or mistyped station therefore
snaps the carpet toward a station somewhere else entirely, causing severe
warping. When a carpet lands in the wrong place or shears:

- Confirm every drawn station's **name** matches the survey exactly.
- Confirm each station is placed at the right **spot** in the drawing.
- If you can't find the offender, **delete the suspect stations** one at a time
  to isolate which one is dragging the morph, then re-add it correctly.

## Choose the right scrap type

A drawing morphed with the wrong [scrap type](scrap-types.md) distorts by
construction:

- Use a **Running Profile** for **horizontal passage** drawn in profile — it
  wraps around corners and keeps each shot's length.
- Use a **Projected Profile** for **vertical shafts and deep pits**, and make
  sure its **azimuth** is right so the plane faces the way you drew it.

Two type-specific artifacts to recognize:

- **Plan scraps — localized vertical bumps.** When a plan morph pulls in
  stations it shouldn't, unrelated nearby stations create small vertical
  bulges. Tightening which stations the scrap uses (see
  [max closest stations](warping-settings.md#max-closest-stations)) or splitting
  the scrap removes them.
- **Running profiles — creases and seesawing.** A profile drawn over
  **stacked** stations (as in a vertical shaft) creases, and two vertical scraps
  that **overlap** can "seesaw" as the morph alternates which station each point
  follows. Running profiles want a continuous run of shots without branches;
  give a vertical drop a projected profile instead.

## When in doubt, split the scrap

A single large, complicated drawing is harder to morph cleanly than several
smaller ones. **Subdividing a sketch into multiple independent scraps** — one per
stretch of passage, each with the right type and its own stations — is the most
reliable way to tame a stubborn carpet. Each scrap then morphs against only the
stations that belong to it.

## Where to go next

- Adjust morph density and smoothing: [Tune the warping
  settings](warping-settings.md).
- Review how each type projects: [Choose the scrap type](scrap-types.md).
