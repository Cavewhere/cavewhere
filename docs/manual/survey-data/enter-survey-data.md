---
title: Enter Survey Data
summary: The survey table, its station rows and shot rows and LRUD, adding and removing rows, and the keyboard shortcuts that make entry fast.
problem: Get the numbers out of the survey book and into the project, quickly and without typos.
keywords: [survey, shot, station, data entry, table, distance, compass, clino, vertical angle, lrud, chunk, data block, keyboard, plumb]
related: [calibration.md, survey-errors.md, caves-and-trips.md, ../concepts/glossary.md]
---

# Enter Survey Data

## Why / when you need this

![A trip page. The survey editor down the left side is highlighted: the trip's name and date, a warning banner, Calibration, Team, the Notes thumbnails and the Data heading. On the right, a scan of the open survey book shows handwritten readings under Stations, Distance, Compass and Clino.](../images/survey-trip-page.png)
*The trip page, shown above. The **survey editor** fills the highlighted panel,
and the scanned survey book sits beside it.*

Paper surveyors spend most of their CaveWhere time on this one screen. A trip
brings back a book full of numbers, and none of them do anything until you type
them in. Everything else (the 3D model, the
[carpets](../concepts/glossary.md#carpeting), loop closure) builds on what you
enter here.

So the table chases 2 things: **speed, and catching mistakes**. You can enter a
whole trip without touching the mouse, and CaveWhere checks each reading as you
type rather than when you plot the cave. A transposed digit found now costs a
minute; found once you are home, it costs a return trip.

## Open the survey table

Click **Data** in the sidebar, click the cave, then click the trip. The survey
table fills the left side of the trip page, under the trip's name and date.

The chevron beside the **Trip** heading folds the table to a narrow strip and
hands the width to the note gallery, handy once the data is in and you are
working on the sketches. The chevron on the strip brings it back.

## Read the table

Before you type anything: **the table does not have one row per shot.** Station
and shot rows alternate. A station row carries the name and its **L**, **R**,
**U**, **D**. The shot row between 2 stations carries the **Distance**,
**Compass** and **Vertical Angle** that join them, drawn half a row high so its
cells straddle the boundary between that pair.

![Three data blocks in the survey table. A Station column on the left lists a1 to a5, L R U D columns sit on the right, and Distance, Compass and Vertical Angle readings sit on shot rows between consecutive station rows, tagged fs. The second block begins at station a4.](../images/survey-shot-table.png)
*The survey table, showing 3 data blocks. The readings are tagged `fs` for
foresight; this trip has no backsights, so no `bs` row sits under them. The
second block restarts at `a4`, a station the first already passed through: a
branch, which a block cannot contain.*

The layout mirrors your survey book, because it mirrors the cave: a
[shot](../concepts/glossary.md#shot) measures *between* 2 stations, while
[LRUD](../concepts/glossary.md#lrud) measures *at* one. A flat row per shot would
have to pick one station's LRUD and get the other wrong.

The 8 columns, left to right:

- **Station** holds the name, `A1` say. `cwStationValidator.cpp` pins it to the
  regex `(?:[a-zA-Z0-9]|-|_)+`, so the cell rejects a dot or a space as you type
  it.
- **Distance** takes any number at or above `0`, in the trip's units (see
  [Calibration](calibration.md#set-the-distance-units)).
- **Compass** takes the bearing in degrees, `0` to `360`.
- **Vertical Angle** takes the inclination, `-90` to `90` degrees. This column
  will not match your book, which almost certainly says *clino*. CaveWhere's own
  error messages call it the clino too, and so does the rest of this manual.
- **L**, **R**, **U**, **D** take left, right, up and down from the station to
  the walls, ceiling and floor.

On a trip recording both foresights and backsights, the Compass and Vertical
Angle cells each split in 2, foresight (`fs`) above backsight (`bs`).
[Calibration](calibration.md) decides which you see, per trip.

## Add shots

**Just start typing.** There is no "new row" button. Each block carries a spare
station-and-shot pair below its last filled row; put data in it and CaveWhere
makes it real, then hangs a fresh spare below. Rows keep appearing ahead of you.

Only the focused block shows that spare pair (`cwSurveyEditorModel` calls them
virtual rows). Move to another block and it goes away, taking every other empty
pair with it, so an accidental Tab off the end leaves no debris.

A brand-new trip instead starts with an **Add Survey Data** button, which shows
only while the trip holds no data and seeds the first block.

### Let CaveWhere name the next station

Land on an empty station at the end of a run and CaveWhere guesses the next name
(`A1` → `A2`) under a grayed **Press Tab** hint. Tab accepts it, and so does
clicking the hint. Type over a wrong guess.

The guess is a regex in `cwSurveyChunk.cpp`: find the first run of digits in the
previous name and add 1. `A9` gives `A10`, `B12` gives `B13`. It fails on 3
shapes of name:

- No digits at all, so nothing to increment: `entrance` gets no guess.
- A leading zero vanishes: `A01` guesses `A2`, not `A02`.
- Anything after the number vanishes with it: `A1B` guesses `A2`.

In a brand-new block the guess reaches back to the last station of the block
above, so walking off from `A5` offers you `A6`.

Use it. `A10` and `A1O` both read fine at a glance, and only one of them
connects to your survey.

### Station names ignore case

`a1` and `A1` name **the same station**. `cwStation::canonicalKey()` lowercases
every name before the pipeline compares it, so a block entered in lowercase ties
to stations written uppercase elsewhere.

That has 2 consequences. Case never explains 2 stations failing to join, so look
for a real difference in the characters. And you cannot use `a1` and `A1` as 2
distinct stations; they will silently become one.

This holds CaveWhere-wide, scraps and [LiDAR notes](../notes/lidar-notes.md)
included. Compass is the exception, since it *is* case-sensitive, so CaveWhere
uppercases names when exporting to it.

## Start a new data block

A block, which CaveWhere calls a **chunk**, holds a *continuous* run of stations
joined by shots. That is the whole rule, and it explains the button.

**A chunk cannot branch.** It forms a line, not a tree. Walk back to `A3`, head
off down a side passage, and those shots cannot continue the block you were in:
it would have to fork at `A3`. They start a new block instead, which is why the
second block in the screenshot above restarts at `a4`.

Press **Space** in any cell to add one, or click the **Press Space to add
another data block** bar under the table. CaveWhere will not add a second empty
block while an unused one sits at the end; it moves your focus there instead.

Space means *new data block* even mid-edit: `DataBox.qml` commits the cell
first, then adds the block, and an invalid value stops it there with no block
added. No cell in this table would take a space anyway.

Blocks also explain why an imported survey arrives split into sections. Every
importer runs the same test, `canAddShot`: does this shot's from-station match
the last station in the current block? On a mismatch it opens a new block.

## Move around with the keyboard

Data entry is a two-handed job with a book in front of you, so the table drives
entirely from the keyboard.

| Key | Does |
|-----|------|
| **Tab** / **Shift+Tab** | Next / previous cell |
| **Arrow keys** | Move a cell in any direction |
| **Enter** | Start editing; while editing, commit and move on |
| **Space** | Add a new data block |
| **Esc** | Abandon the edit and put the old value back |
| Any valid character | Starts editing that cell immediately |

That last row is what makes entry fast: you never have to "open" a cell. Land on
it, type the number, Tab. Cells the trip does not use, the backsight halves on a
foresight-only trip, get skipped.

Tab does not run straight across. It walks a block roughly in book order: both
station names of the first pair, then the readings between them, then the LRUDs,
then the next station name, the next shot, on down. Only that first pair breaks
the pattern. The arrow keys give you the plain grid, and they stop at the edges:
Left on Station and Right on **D** go nowhere.

**I recommend letting Tab do the walking.** It lands on every cell that matters
without you steering, and it skips the hidden backsight cells.

An invalid value pins you in place: Tab runs the cell's validator before it
moves, and a rejected value pops the validator's complaint under the cell.

## Enter a plumbed shot

For a vertical shot, type **up** or **down** into the Vertical Angle instead of
a number. `cwClinoValidator.cpp` matches either word case-insensitively, so
`up`, `Up` and `UP` all land.

A plumbed shot needs no compass, and CaveWhere knows it. The word form stops the
missing-compass and missing-clino errors, and skips the 2° foresight/backsight
tolerance too, since `up` carries no number to compare. A numeric `90` or `-90`
also counts as a plumb for the missing-reading check, with 0.001° of slop, but
still gets compared against its backsight.

You cannot mix the 2 conventions across a foresight and backsight of the same
shot: both must be words, or both numbers. (See
[Survey Errors](survey-errors.md#you-are-mixing-types).)

## Insert and remove rows

Right-click any cell for its menu. The station name and the 4 LRUDs share one
menu; Distance, Compass and Vertical Angle share the other.

On a **station** cell an **Insert** submenu offers **Station above** and
**Station below**. Because a block keeps its stations and shots paired, removing
a station always takes a shot with it. So the remove item opens a submenu named
for the station (**Remove A3**) asking *which*: **and the shot above** or **and
the shot below**. The shot menu mirrors it, with **Remove shot**.

Hover a remove item and CaveWhere strikes a line through exactly the rows that
would go. Read that preview; it beats reasoning about which shot pairs with
which station.

Both menus also offer **Remove Chunk**, which deletes the whole block. No dialog
asks you to confirm, so the strike-through is the only warning you get.

A block can never shrink below 2 stations and the shot between them, so
`canRemoveStation` refuses there and the remove items gray out. To clear the
last of it, remove the chunk.

## Exclude a distance from the total

Select a Distance cell and a caret button appears in its top-right corner, only
while that cell holds the focus. Its menu holds a single item: **Exclude
Distance** on a shot that counts toward the total, **Include Distance** on one
that does not. An excluded shot wears an **Excluded** badge.

Excluding does **not** remove the shot from the cave; the leg still positions
the stations exactly as before. `cwTripLengthTask` drops it from the sum and
from the shot count that the tape calibration multiplies, so it contributes
nothing to the length either way.

You want that when you re-survey passage someone already surveyed, tying 2
surveys together or redoing a section to better standards. The shot holds the
stations in place, but its passage is already in the total, and counting it
twice inflates the cave.

**Total Length** under the table reports the trip's own length, to 2 decimal
places, in the trip's units.

## Fill in LRUD

LRUD goes on the station rows. Plotting does not need it, since shots alone make
the survey line, but without it the passage has no width or height, so nothing
downstream can give it shape.

CaveWhere asks for all 4. Once a station has a name, each empty LRUD beside it
raises its own warning, so a skipped station carries 4. Fill them in as you go
down the block; at 4 warnings a station, they bury the one warning that matters.

A shot with an **explicitly entered zero** distance and no compass or clino is
special: an **LRUD-only** station, where you recorded passage dimensions without
measuring a leg. Compass sometimes writes these. Leaving the distance **empty**
means something else entirely, an unfinished shot, which CaveWhere marks fatal.

## Next steps

- Instruments do not read true. [Calibration](calibration.md) corrects them.
- A compass points at magnetic north. [Declination](declination.md) turns that
  into true north.
- CaveWhere marks up mistakes as you type. See
  [Survey Errors](survey-errors.md).
