---
title: Calibrate the Instruments
summary: Correct a trip's tape, compass and clino readings, choose foresights or backsights, and set the trip's units.
problem: Make readings from your team's specific instruments come out as true measurements, so different teams' data lines up.
keywords: [calibration, tape, compass, clino, front sight, back sight, backsight, foresight, backwards, corrected, units, meters, feet]
related: [enter-survey-data.md, declination.md, survey-errors.md, ../concepts/glossary.md]
---

# Calibrate the Instruments

## Why / when you need this

No instrument reads true. A tape that has been re-tied after a break is short by
the piece that's missing. A compass reads a degree or two off. These are not
mistakes — they're fixed, knowable properties of the kit your team carried, and
they are the same on every shot of the trip.

That makes them correctable, and **calibration is where you record the
correction once instead of doing arithmetic on every reading in your book.** You
type what you actually read off the instrument; CaveWhere applies the offset.

It matters most when data from several teams meets in one cave. Two teams with
two compasses produce two slightly different versions of the same passage, and
loops between them won't close. Calibrated, both teams' raw readings resolve to
the same true measurements and the surveys line up.

**Calibration is per trip**, which is the right grain: one team, one day, one set
of instruments. It lives in the **Calibration** section of the trip page, under
the trip's name and date.

![The Calibration section of a trip. A Declination box and a Distance box (holding Calibration and Units, set to m) sit side by side. Below them a ticked Front Sights box holds Compass calibration, Clino calibration, and the Backwards Compass and Backwards Clino checkboxes; the Back Sights box beside it is unticked and empty.](../images/survey-calibration.png)
*The Calibration section. This trip recorded foresights only, so **Back Sights**
is unticked and its box collapses — the survey table hides the backsight cells
to match.*

## The rule: calibration is added

Every calibration in CaveWhere works the same way, and the app states it
plainly:

> **UncorrectedValue + Calibration = TrueValue**

The value you type in the table is the *uncorrected* one — the number you read
in the cave. CaveWhere **adds** the calibration to it to get the true value.

The sign follows from that, and it's the part people get backwards. A tape
that's **1 m short** reads 10 m for a passage that's really 9 m, so the
calibration is **−1**: `10 + (−1) = 9`. The correction is what you *add to the
reading*, not the instrument's error.

## Choose foresights, backsights, or both

Two checkboxes — **Front Sights** and **Back Sights** — say which readings your
trip recorded. They control which cells the survey table shows, and each has its
own calibration box below.

You need at least one. With both unticked, CaveWhere tells you:

> Hmm, you need to **check** either *front* or *back sights* box, or both,
> depending on your data.

**Tick both if you shot both.** It costs nothing and buys the single most useful
error check CaveWhere has: with a foresight and a backsight for the same shot,
the two readings must agree, and CaveWhere warns you when they disagree by more
than **2°**. A misread compass has nowhere to hide. See
[Survey Errors](survey-errors.md#frontsight-and-backsight-differs-by-x).

## Set the tape calibration

The **Distance** box holds one number: what to add to every tape reading in the
trip, in the trip's units.

Leave it at `0` unless you know your tape is off. If you do know, measure it
against something trustworthy and enter the difference — negative for a short
tape, positive for a long one.

### Set the tape units

The **Units** dropdown sets whether the trip's distances are **meters (m)** or
**feet (ft)**. Those are the only two choices, and it applies to the whole
trip — both shot distances and LRUDs.

Set it *before* you type, and check it when a trip is imported. Nothing about a
bare number says what unit it is, so entering feet into a metric trip produces a
cave a bit over three times too big, with no error to warn you. The unit is also
the one thing here that isn't a property of an instrument — it's a property of
how your team writes numbers down.

> **Changing the unit does not convert your numbers.** A `10` stays a `10`; it
> just means ten feet now instead of ten meters. That's the correct behaviour —
> it's how you fix a trip entered under the wrong setting — but it means
> switching the unit on a finished trip silently resizes the cave. If you meant
> to convert, you have to retype.

## Set the compass and clino calibration

The **Front Sights** and **Back Sights** boxes each hold a **Compass
calibration** and a **Clino calibration**, both in degrees, both following the
same add rule.

Front and back get separate numbers because they are usually separate
instruments, and even on one instrument the backsight optics can be off on their
own. Calibrate each against what it actually reads.

## Tell CaveWhere which way a reading was shot

Below the calibrations sit two checkboxes per box. They are the subtlest
controls on this page, and they encode your team's **reading convention** rather
than any instrument error.

| Box | Checkbox | Means |
|-----|----------|-------|
| Front Sights | **Backwards *Compass*** / **Backwards *Clino*** | Your foresights were read as if they were backsights |
| Back Sights | **Corrected *Compass*** / **Corrected *Clino*** | Your backsights were read as if they were foresights |

Both say the same thing: *the reading in this column points the opposite way to
what the column normally means.* CaveWhere reverses it for you — subtracting
180° from a compass reading, or multiplying a clino reading by −1.

This exists because reading conventions genuinely differ between teams and
instruments. Some teams shoot every reading from the same end of the tape. Some
instruments display a backsight already reversed, so it reads like a foresight
straight off the screen — tick **Corrected Compass** and enter exactly what the
display said.

Get one of these wrong and the symptom is dramatic and easy to recognise:
passages run 180° from where they belong, or the cave hangs upside down. If a
trip comes out mirrored, check these four boxes before you suspect your data.

> **Which box do I tick?** Ask what the number in front of you *is*, not what
> you want it to be. If the number in your backsight column was read pointing
> forwards, that's a *corrected* backsight. If it was read pointing back — the
> normal way — leave it alone.

## Next steps

- [Declination](declination.md) is also set here, but it corrects for the
  **world** rather than the instrument, and has its own page.
- [Survey Errors](survey-errors.md) explains the foresight/backsight warning
  these settings enable.
