---
title: Calibrate the Instruments
summary: Correct a trip's distance, compass and clino readings, choose foresights or backsights, and set the trip's units.
problem: Make readings from your team's specific instruments come out as true measurements, so different teams' data lines up.
keywords: [calibration, distance, tape, compass, clino, front sight, back sight, backsight, foresight, backwards, corrected, units, meters, feet]
related: [enter-survey-data.md, declination.md, survey-errors.md, ../concepts/glossary.md]
---

# Calibrate the Instruments

## Why / when you need this

No instrument reads true. A tape re-tied after a break comes up short by the
missing piece; a compass sits a degree or two off. These are fixed properties of
the kit your team carried, repeating on every shot. Calibration records the
correction once, so you never do arithmetic in your book: type what the
instrument said and CaveWhere applies the offset.

**Calibration belongs to one trip**, one team, one day, one set of instruments.
It lives in the **Calibration** section of the trip page, shown below.

![The Calibration section of a trip. A Declination box and a Distance box (Calibration and Units, set to m) sit side by side. Below them a ticked Front Sights box holds Compass calibration, Clino calibration, and the Backwards Compass and Backwards Clino checkboxes; the Back Sights box beside it is unticked and empty.](../images/survey-calibration.png)
*The Calibration section, shown above. This trip recorded foresights only, so
**Back Sights** sits unticked and its box collapses.*

## The rule: calibration is added

Every box obeys one rule, stated in each help bubble:

> **UncorrectedValue + Calibration = TrueValue**

The number you type in the survey table is the *uncorrected* one, what you read
in the cave; CaveWhere adds the calibration to get the true value. All 5 fields
start at `0`, so an untouched trip changes nothing.

The sign trips people up. In CaveWhere's own tape example you read **10 m** with
a tape that's **1 m short**, the true length comes to 9 m, and the calibration
reads **−1**: `10 + (−1) = 9`. Enter what you add to the reading, never the
instrument's error. Fields display 2 decimals but store more: a typed `1.234`
redraws as `1.23`.

## Choose foresights, backsights, or both

**Front Sights** and **Back Sights**, 2 checkboxes, record which readings your
trip took. They decide which cells the survey table shows, and each opens its
own calibration box below; untick one and its box collapses. A new trip starts
with both ticked, and unticking both gets you:

> Hmm, you need to **check** either *front* or *back sights* box, or both,
> depending on your data.

**I recommend ticking both whenever your team shot both.** It costs nothing and
buys the best error check in the app. A foresight and a backsight on the same
shot must agree, and CaveWhere warns you when they differ by more than **2°**. A
misread compass has nowhere to hide. See
[Survey Errors](survey-errors.md#frontsight-and-backsight-differs-by-x).

That check compares the *corrected* numbers, backsight flipped, so a wrong
calibration raises the same warning as a wrong reading. Worth remembering when
every shot in a trip complains at once.

## Set the distance calibration

The **Distance** box holds one number: what to add to every distance reading in
the trip, in the trip's units. Leave it at `0` unless you know your tape reads
off; if you do, measure it against something trustworthy and enter the
difference, negative when it reads long, positive when it reads short.

Nothing checks this number. The 2° warning covers compass and clino only, so a
wrong tape figure quietly gives you a cave the wrong size. It accumulates too:
reported trip length multiplies it by the shot count, so `-0.05` across 200
shots takes 10 m off the trip.

### Set the distance units

The **Units** dropdown offers exactly 2 choices, **meters (m)** and **feet
(ft)**, covering the whole trip, shot distances and LRUDs alike. Set it before
you type, and check it on any imported trip. No bare number says which unit it
carries, so feet entered into a metric trip give a cave **3.28** times too big
(0.3048 m per foot), with no error to warn you.

> **Changing the unit does not convert your numbers.** A `10` stays a `10` and
> now means ten feet instead of ten meters. That lets you fix a trip entered
> under the wrong setting, and it means switching the unit on a finished trip
> silently resizes the cave. Converting means retyping.

## Set the compass and clino calibration

**Front Sights** and **Back Sights** each hold a **Compass calibration** and a
**Clino calibration**, both in degrees, both following the same add rule. Front
and back get separate numbers because they usually come from separate
instruments, and even on one instrument the backsight optics drift.

## Tell CaveWhere which way a reading was shot

Below the calibrations sit 2 checkboxes per box, encoding your team's **reading
convention**, not any instrument error.

| Box | Checkbox | Means |
|-----|----------|-------|
| Front Sights | **Backwards *Compass*** / **Backwards *Clino*** | You read your foresights as if they were backsights |
| Back Sights | **Corrected *Compass*** / **Corrected *Clino*** | You read your backsights as if they were foresights |

Both say one thing: *the reading in this column points the opposite way to what
the column normally means.* CaveWhere reverses it, subtracting **180°** from a
compass reading or multiplying a clino by **−1**.

One shot, `A1` to `A2`. Stand at `A1`, sight `A2`, and the compass reads
**60°**. Walk to `A2`, sight back, and the same shot reads **240°**. The 2
numbers always differ by 180°, and neither says which end you stood at. The
column you type into says that, and the checkbox says the column has it
backwards. Type `240` into **Front Sights** with **Backwards *Compass*** ticked
and CaveWhere uses 60°, exactly as if you had typed `60` with the box clear.

![A plan view of one shot between stations A1 and A2, north up. A blue arrow from A1 to A2 reads "60° foresight, stand at A1, sight A2"; an orange arrow back from A2 to A1 reads "240° backsight, stand at A2, sight A1". A table below lists the 4 combinations of column and typed reading, the box each needs ticked, and the direction CaveWhere uses.](../images/illustrations/calibration-reading-direction.svg)
*The 4 cases, shown above. Whichever number you typed, the box makes it mean the
direction you actually shot.*

Conventions genuinely differ. Some teams shoot every reading from the same end
of the tape; some instruments display a backsight already reversed, so tick
**Corrected Compass** and enter what it said.

Get one wrong and you see it immediately: passages run 180° from where they
belong, or the cave hangs upside down. Check these 4 boxes whenever a trip comes
out mirrored.

## What import and export do with these

CaveWhere plots a cave by writing a Survex file and running `cavern`, so your
calibrations leave as `*calibrate` lines: `tape`, `compass`, `backcompass`,
`clino` and `backclino`, each to 2 decimals. Two things surprise people there.
The sign comes out flipped, because Survex counts calibration the other way
around, and a calibration of `0` is omitted, so a missing `*calibrate tape` line
means zero, not forgotten.

Import runs that backwards, and on a Survex file it guesses, because a `.svx`
never says which convention produced a backsight. A `*calibrate backcompass`
from **180°** up to **225°** reads as a corrected backsight: CaveWhere ticks
**Corrected Compass** and keeps the remainder as the calibration. The window is
one-sided, so `180` counts and `179` does not. A `backclino` scale of `-1` ticks
**Corrected Clino**. Walls and Compass state their conventions outright, but
check these 2 boxes on anything you import.

## Next steps

- [Declination](declination.md) sits in the same section, but corrects the
  **world** instead of the instrument, and has its own page.
- [Survey Errors](survey-errors.md) explains the 2° warning these turn on.
