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
piece that went missing. A compass sits a degree or two off. Those are fixed,
knowable properties of the kit your team carried, and they repeat on every shot
of the trip.

That makes them correctable. Calibration records the correction once, so you
never do arithmetic on a reading in your book. You type what you read off the
instrument, and CaveWhere applies the offset.

**Calibration belongs to one trip**: one team, one day, one set of instruments.
It lives in the **Calibration** section of the trip page, under the trip's name
and date, shown below.

![The Calibration section of a trip. A Declination box and a Distance box (holding Calibration and Units, set to m) sit side by side. Below them a ticked Front Sights box holds Compass calibration, Clino calibration, and the Backwards Compass and Backwards Clino checkboxes; the Back Sights box beside it sits unticked and empty.](../images/survey-calibration.png)
*The Calibration section shown above, 4 boxes in 2 rows. This trip recorded
foresights only, so **Back Sights** sits unticked and its box collapses, and
the survey table hides the backsight cells to match. Each pair stays side by
side while a column measures at least 200 px; narrower than that and all 4
boxes stack.*

## The rule: calibration is added

Every box here obeys one rule, and the app states it in each help bubble:

> **UncorrectedValue + Calibration = TrueValue**

The number you type in the survey table is the *uncorrected* one, the number you
read in the cave. CaveWhere adds the calibration to reach the true value. All 5
calibration fields start at `0` on a new trip, so an untouched trip changes
nothing.

The sign follows from that rule, and it trips people up. Take CaveWhere's own
tape example. You read **10 m** with a tape that's **1 m short**, so the true
length comes to 9 m and the calibration reads **−1**: `10 + (−1) = 9`. The
compass example runs the same way. You read **180°** off an instrument sitting
2° low, you enter **2**, and the bearing comes out 182°. Enter what you add to
the reading, never the instrument's error.

The fields display 2 decimal places. Type `1.234` and the box redraws as
`1.23`, though the value behind it stays `1.234`. So a third decimal may be
sitting in a field that looks rounded.

## Choose foresights, backsights, or both

**Front Sights** and **Back Sights**, 2 checkboxes, record which readings your
trip took. They decide which cells the survey table shows, and each one opens
its own calibration box below. A new trip starts with both ticked.

You need at least one of them. Untick both and CaveWhere says:

> Hmm, you need to **check** either *front* or *back sights* box, or both,
> depending on your data.

**I recommend ticking both whenever your team shot both.** It costs nothing and
buys the best error check in the app. Given a foresight and a backsight on the
same shot, the 2 readings must agree. CaveWhere warns you when they differ by
more than **2°** (the default lives in `cwSurveyChunk.h`). A misread compass
then has nowhere to hide. See
[Survey Errors](survey-errors.md#frontsight-and-backsight-differs-by-x).

That check runs on the *corrected* numbers. CaveWhere adds your calibrations and
flips the backsight before it compares, so a wrong calibration raises the same
warning as a wrong reading. Worth remembering on the day every shot in a trip
complains at once.

Ticking both also splits the Compass and Clino cells in the survey table into 2
half-height rows, foresight above backsight. Untick one and those cells go back
to full height. A collapsed **Back Sights** box then looks like the screenshot
shown above.

## Set the distance calibration

The **Distance** box holds one number: what to add to every distance reading in
the trip, in the trip's units. Leave it at `0` unless you know your instrument
reads off. Tapes vary, and a re-tied one may read either way, so measure it
against something trustworthy and enter the difference (negative when it reads
long, positive when it reads short).

Nothing checks the distance calibration. That 2° warning covers compass and
clino only, so a wrong tape number gives you a cave that comes out quietly the
wrong size. It also accumulates. The trip length CaveWhere reports multiplies
your calibration by the shot count, so `-0.05` across 200 shots takes 10 m off
the trip.

### Set the distance units

The **Units** dropdown offers exactly 2 choices, **meters (m)** and **feet
(ft)**, and the choice covers the whole trip, shot distances and LRUDs alike.
Set it before you type, and check it on any imported trip.

Nothing about a bare number says which unit it carries. Enter feet into a
metric trip and the cave comes out **3.28** times too big (CaveWhere converts
at 0.3048 m per foot), with no error to warn you. The unit also describes your
team instead of an instrument: it records how you wrote the numbers down.

> **Changing the unit does not convert your numbers.** The dropdown isn't a
> converter. A `10` stays a `10`, and now means ten feet instead of ten meters.
> That behavior lets you fix a trip entered under the wrong setting, and it also
> means switching the unit on a finished trip silently resizes the cave.
> Converting means retyping.

## Set the compass and clino calibration

**Front Sights** and **Back Sights** each hold a **Compass calibration** and a
**Clino calibration**, both in degrees, both following the same add rule. The
app's clino example: you read **+4°** off an instrument running 1° high, you
enter **−1**, and the angle comes out +3°.

Front and back get separate numbers because they usually come from separate
instruments, and even on one instrument the backsight optics drift on their own.
Calibrate each against what it actually reads.

## Tell CaveWhere which way a reading was shot

Below the calibrations sit 2 checkboxes per box. They are the subtlest controls
on this page, and they encode your team's **reading convention**, not any
instrument error.

| Box | Checkbox | Means |
|-----|----------|-------|
| Front Sights | **Backwards *Compass*** / **Backwards *Clino*** | You read your foresights as if they were backsights |
| Back Sights | **Corrected *Compass*** / **Corrected *Clino*** | You read your backsights as if they were foresights |

Both say one thing: *the reading in this column points the opposite way to what
the column normally means.* CaveWhere reverses it, subtracting **180°** from a
compass reading or multiplying a clino reading by **−1**, matching what
the help bubble beside each checkbox promises.

A worked example, one shot from `A1` to `A2`. Stand at `A1`, sight `A2`, and the
compass reads **60°**. Walk to `A2`, sight back at `A1`, and the same shot reads
**240°**. The 2 numbers always differ by 180°, and neither one, on its own, says
which end you stood at.

The column you type it into says that, and the checkbox tells CaveWhere the
column has it backwards. Type `240` into the **Front Sights** column with
**Backwards *Compass*** ticked and CaveWhere uses 60°, the same direction you
get by typing `60` with the box clear. Check out the diagram
below for all 4 combinations.

![A plan view of one shot between stations A1 and A2, north up. A blue arrow running from A1 to A2 reads "60° foresight, stand at A1, sight A2"; an orange arrow running back from A2 to A1 reads "240° backsight, stand at A2, sight A1". A table below lists the four combinations of column and typed reading, which box each needs ticked, and the direction CaveWhere ends up using.](../images/illustrations/calibration-reading-direction.svg)
*The 4 cases, shown above. Whichever of the 2 numbers you typed, the box makes
it mean the direction you actually shot.*

Reading conventions genuinely differ between teams and instruments, hence these
boxes. Some teams shoot every reading from the same end of the tape.
Instruments vary here too. Some display a backsight already reversed, so it
reads like a foresight straight off the screen. Tick **Corrected Compass** and
enter exactly what the display said.

Get one of these wrong and you see it immediately. Passages run 180° from where
they belong, or the whole cave hangs upside down. When a trip comes out
mirrored, check these 4 boxes before you suspect your data.

> **Which box do I tick?** Ask what the number in front of you *is*, not what you
> want it to be. A backsight-column number read pointing forwards counts as a
> *corrected* backsight. Read pointing back, the normal way, leave the box
> alone.

## What import and export do with these

CaveWhere plots a cave by writing a Survex file and running `cavern` on it. So
your calibrations leave the app as `*calibrate` lines: `tape`, `compass`,
`backcompass`, `clino` and `backclino`, each written to 2 decimals. Two things in
that file surprise people. The sign comes out flipped, because Survex counts
calibration the other way around. Any calibration sitting at `0` gets left out
entirely, so a missing `*calibrate tape` line means zero, not forgotten.

Import runs that mapping backwards. Survex takes a guess, because a `.svx` file
never says which convention produced a backsight:

- A `*calibrate backcompass` value from **180°** up to **225°** reads as a
  corrected backsight. CaveWhere ticks **Corrected Compass** and keeps the
  remainder as the calibration. The window is one-sided, so `180` counts and
  `179` does not, and the guess fails outright on a convention it has not seen.
  Check these 2 boxes on anything you import.
- `*calibrate backclino` with a scale of `-1` ticks **Corrected Clino**.

Walls and Compass need no guess, because both formats say it outright:

- In a Walls file, `INCD` becomes the distance calibration, `INCA` and `INCV`
  the front compass and clino, `INCAB` and `INCVB` the back pair. The `C` flag on `TYPEAB` or
  `TYPEVB` ticks **Corrected Compass** or **Corrected Clino**.
- A Compass `.dat` file declares its columns in a format string, and CaveWhere
  reads **Back Sights** out of it (the 14th character, the one marking the AZM2
  and INC2 columns). A file carrying no format string at all falls back to feet
  with **Back Sights** off.

## Next steps

- [Declination](declination.md) sits in this same section, but it corrects the
  **world** instead of the instrument, so it gets its own page.
- [Survey Errors](survey-errors.md) explains the 2° foresight/backsight warning
  these settings turn on.
