---
title: Fix Survey Errors
summary: What CaveWhere's errors and warnings mean, which ones stop the cave plotting, and how to fix each one.
problem: Find the bad reading that's stopping your cave from plotting — or the one that plots fine and is quietly wrong.
keywords: [error, warning, fatal, survey error, duplicate station, missing station, backsight, tolerance, unconnected, blunder]
related: [enter-survey-data.md, calibration.md, ../concepts/glossary.md]
---

# Fix Survey Errors

## Why / when you need this

Survey data is entered by tired people from a muddy book, and some of it will be
wrong. The useful question isn't whether you made a mistake — it's whether you
find it now or after the map is drawn.

So CaveWhere checks every reading as you type it, and marks the cell rather than
waiting for you to plot the cave. The cell gets a border and a **small square
badge** in its corner, carrying a stop sign if it's an error or a triangle if
it's a warning. Click the badge to read the message; clicking another one closes
the first, so you read them one at a time.

## Errors versus warnings

The distinction is exact, and it tells you how urgently to care:

| | Meaning |
|---|---|
| **Error** (fatal) | The cave **cannot be plotted** until you fix it. CaveWhere can't guess what you meant. |
| **Warning** | The cave plots fine. This looks wrong and is worth a look, but you're allowed to disagree. |

The trip's header shows a running total — **"There are *n* errors"** behind a
stop sign, **"There are *n* warnings"** behind a warning triangle — and it
disappears entirely when the trip is clean. That summary is the fastest way to
tell whether a trip is finished.

**The counts roll all the way up.** A bad cell marks its trip, and the trip
marks its cave, so the same stop sign and triangle appear beside the trip in the
cave's trip table and beside the cave in the cave list. You never have to open a
trip to find out whether it needs attention — and a project you haven't touched
in a year tells you where you left off.

Warnings can be **suppressed** when you've checked one and it really is fine:
the popup puts a checkbox beside each warning, and ticking it strikes the
message through. Errors have no such checkbox — there is nothing to plot until
the data says something.

## Errors

### Missing station name

A shot next to this station has data, but the station has no name. A shot has to
run *from* somewhere *to* somewhere; without a name there's nothing to connect.

Type the name. Note that a blank name on a row you haven't touched is **not** an
error — CaveWhere only asks once there's data beside it.

### Duplicate station name "*x*"

This station has the same name as **the station immediately above it**, so the
shot between them starts and ends in the same place.

Only the adjacent pair is checked, and that's deliberate — **a station name
repeating elsewhere is not a mistake, it's the point.** Coming back to `A3` from
a different direction is how a loop closes, and how CaveWhere knows two runs of
passage meet. What can't be meaningful is a shot from `A3` to `A3`.

Usually it's a slip: you meant to type the next name and repeated the last.
Remember that names ignore case, so `a3` under `A3` is a duplicate too.

### Missing "*x*" from shot "*A1*" ➔ "*A2*"

A reading the shot needs is empty. **Distance is always fatal** — a leg with no
length can't be plotted at all.

For compass and clino, it depends on what else you have: if the matching
backsight (or foresight) is filled in, this is only a *warning*, because
CaveWhere can plot from the one reading. If both are empty, the shot has no
direction and it's fatal.

### The "*x*" value ("*y*") isn't a number for station "*z*"

An LRUD cell contains something that isn't a number. Clear it or fix it.

### You are mixing types

> You are mixing types. Frontsight and backsight must both be Up or Down, or
> both numbers

You've written a word in one half of a shot's Vertical Angle and a number in the
other — `up` against `89`, say. Pick one convention for the shot: either both
readings are plumbed (`up`/`down`), or both are angles.

### Invalid readings

Typing something a cell can't accept gets you the cell's own message:

- **Distance** must be a number **≥ 0**.
- **Compass** must be a number **between 0 and 360**.
- **Vertical Angle** must be a number **between −90 and 90**, or the keywords
  **up** or **down**.
- **Station names** must be made of letters and numbers (hyphens and
  underscores are allowed too).

### Survey leg isn't connected to the cave

The plot couldn't run because a block of shots doesn't reach the rest of the
cave:

> Cannot solve: *n* survey leg(s) are not connected to the cave network
> (*cave*). Open the affected chunks and fix the disconnected station names.

Every block has to join the network *somewhere* — through a station name it
shares with another block. A block that shares none is floating: CaveWhere has
no way to know where in the cave it goes, so it refuses to plot rather than put
it somewhere arbitrary.

It is nearly always a name that doesn't match what you think it matches. Check
the first station of the orphaned block against the station it's supposed to tie
to, character by character — `O` against `0` and `l` against `1` are the classic
pair. Case is *not* the problem: CaveWhere ignores it.

## Warnings

### Frontsight and backsight differs by *x*°

**This is the one worth having.** The foresight and backsight of this shot
disagree by more than **2°** once the backsight is reversed and
[calibration](calibration.md) is applied.

Two readings of the same leg should give the same answer. When they don't, one
of them is wrong, and the shot is telling you so on the day rather than at loop
closure six months later. Common causes, in rough order:

- A **transposed digit** — `173` typed as `137`.
- **Iron** near one end of the shot: a bolt, a carbide dump, a helmet on the
  floor beside the instrument.
- The wrong **corrected/backwards** box ticked in
  [Calibration](calibration.md#tell-cavewhere-which-way-a-reading-was-shot) —
  though that misfires on every shot at once, not one.

If the two readings disagree by *about 180°*, it's a convention problem, not a
reading problem — see Calibration.

Plumbed shots (`up`/`down`) are never tolerance-checked; there's no angle to
compare.

### Missing "*x*" for station "*y*"

An LRUD is empty. Only a warning — the survey line doesn't need it — but the
passage has no width or height there, so anything that draws passage shape has
nothing to work with.

## A warning is not always wrong

Warnings are judgement calls, and CaveWhere lets you overrule them. A 3°
foresight/backsight split next to a large iron deposit may be the best reading
that cave will ever give up. Check it, satisfy yourself, and suppress it.

What you shouldn't do is leave warnings lying around unread. The value of the
check is that an unexpected one stands out — and it can't stand out if there are
already forty on the trip.
