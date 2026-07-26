---
title: Check Loop Closure and Find Blunders
summary: Read the Cavern Output page to see whether your loops close, and use Survex's loop-closure report to find the blunder behind a bad one before it warps the map.
problem: A survey that comes back to a station it already visited should close on itself. When it does not, one reading is wrong, and you want to find which one before the error reaches the drawn map.
keywords: [loop closure, misclosure, blunder, closure error, tie-in, tie-in error, cavern, cavern log, err file, survex, network adjustment, solve, error percent, traverse, articulation, loop, survey error, backsight]
related: [../survey-data/survey-errors.md, ../concepts/why-cavewhere.md, ../concepts/glossary.md, ../concepts/data-model.md, ../survey-data/enter-survey-data.md]
---

# Check Loop Closure and Find Blunders

## Why / when you need this

Every instrument reading carries a little error. Most of the time it stays small
and random. But when a survey route returns to a station it already visited (a
**loop**), the 2 paths that reach that station almost never meet exactly. That
gap, the **[misclosure](../concepts/glossary.md#loop-closure)**, gives you the
best cross-check your data has. A small misclosure means the instruments behaving
like instruments. A *large* one almost always means a **blunder**: a transposed
digit (`173` written `137`), a compass read backwards, a distance typed into the
wrong row.

Catch that blunder now, while you still remember the trip and can re-check the
book. Leave it, and you meet it again on the day the passage that should line up
on the map does not. This page shows where CaveWhere reports closure, how to read
the report, and how to run down the reading throwing it off.

## CaveWhere closes loops for you

You never run loop closure as a separate step. CaveWhere embeds
[Survex](../concepts/glossary.md#survex) 1.4.21, the long-established open-source
cave-survey engine, and calls its `cavern` solver in-process every time your
survey data changes. `cavern` performs a **least-squares network adjustment**.
Rather than dumping the whole misclosure onto the last shot, it spreads the error
across the loop, weighted by each leg, and settles on the most likely position
for every station. You orbit that adjusted
[line plot](../concepts/glossary.md#loop-closure) in
[The 3D View](../view-3d/the-3d-view.md).

Because the solve runs on its own, fixing a blunder repairs everything
downstream. You correct the bad reading, the network re-solves, the loop closes,
and any [carpeted](../concepts/glossary.md#carpeting) sketches re-carpet to
follow (scraps hang off stations, not off the page). See
[Why an edit ripples](../concepts/data-model.md#why-an-edit-ripples) for the full
cascade.

One checkbox governs all of that. Clear **Automatic Update** at the bottom of the
sidebar and the survey solve stops, so loop closure stops with it. The report on
screen then describes your last solve rather than your current data — and nothing
warns you. See
[Carpeting is automatic](../scraps/carpeting.md#carpeting-is-automatic).

## Open the Cavern Output page

The solver's own report lives on the **Cavern Output** page. Open it from the
**Data** page: click the **☰** menu button at the right of the region's name row
and choose **Cavern Output**. The **File** menu reaches the same page as **Cavern
Output…**. After a failed solve both items rename themselves: **Cavern Output
(solve error)** on the Data page, **Cavern Output… (solve error)** in the File
menu. A bad solve announces itself from the menu before you go looking. Below is
the page after a clean solve.

![The Cavern Output page. A status line reads "Last solve completed successfully."; a Solve button sits at the top right; and a highlighted tab bar shows two tabs, "Cavern log", selected, and "Loop closure", grayed out. The log below names the solver build Survex-v1.4.21-62-g5ffc6a8c, reports "Survey contains 14 survey stations, joined by 13 shots. There are 0 loops," then a total shot length of 152.54m with the same 152.54m adjusted, a plan length of 126.00m, a vertical length of 45.71m, and the Vertical, North-South and East-West ranges of the survey.](../images/loop-closure-summary.png)
*The Cavern Output page for Phake Cave 3000, shown above. The status line
confirms the solve ran; the highlighted tab bar holds the 2 reports. **Loop
closure** grays out here, because this cave runs as a single line of passage with
no loops to close, so Survex wrote no closure report to show.*

The page carries 4 things:

- A **status line**, reading either **"Last solve completed successfully."** or,
  in red, **"Cavern reported an error during the last solve."** with the message
  underneath it (cavern's own words, passed through untouched).
- A **Solve** button, which re-runs cavern on demand. You rarely need it.
- A **Cavern log** tab holding the `.log` file cavern writes.
- A **Loop closure** tab holding cavern's `.err` file, covered in the next
  section. The report gates its own tab: it enables only when that file has
  content, and Survex writes content only for a cave with loops.

Both panes hold read-only monospace text; the Cavern log pane is the one shown
above. You can select and copy out of them. That matters, because the `.log` and
`.err` files live in a temporary directory CaveWhere deletes at the end of each
solve. Nothing lands in your project folder, so this page holds the only copy.
Neither pane wraps, either, so a long line scrolls sideways instead of folding. With no cave loaded,
or before the first solve finishes, the log pane reads **"No cavern output to
display."**

### What the Cavern log tells you

The log opens with the exact solver build, `Survex-v1.4.21-62-g5ffc6a8c` as shown
above, and then reports the survey:

```
Survey contains 14 survey stations, joined by 13 shots.
There are 0 loops.
Total length of survey shots =  152.54m ( 152.54m adjusted)
Total plan length of survey shots =  126.00m
Total vertical length of survey shots =   45.71m
```

**"There are *N* loops"** tells you at a glance whether this cave has anything to
close. The figure in parentheses on the total-length line gives the length
*after* adjustment, so the 2 numbers side by side show how much the adjustment
stretched or shortened your measured lengths. Phake Cave 3000 has 0 loops and nothing to adjust, so both read
152.54 m. Below the 3 **Total** lines the log gives the survey's extents, its
bounding box. Phake Cave 3000 measures 36.41 m of Vertical range, 41.01 m
North-South and 48.39 m East-West.

Expect one thing in the log to look broken. Station names come out as
`cave_a684605f6b4d44048b2897db3ae62d97.a8`, not `a8`. CaveWhere renames every
cave to `cave_` plus its 32-character internal id before handing the survey to
cavern, so it can map cavern's stations back onto the right cave afterwards. Read
past the prefix (your station name follows the dot).

CaveWhere passes cavern a single `--quiet`. That drops the progress chatter but
keeps the informational messages, so implicit-fix notices and their like still
turn up here. A second `--quiet` would empty the log altogether, which is why
there is only one.

## Read the loop-closure report

The **Loop closure** tab shows the `.err` file exactly as Survex writes it.
CaveWhere neither parses nor reformats a character of it.

The file holds one block per **traverse**, not one per loop. A traverse is a run
of legs between 2 junctions (or between a junction and a fixed point), and Survex
reports only the traverses that sit inside a loop. A leg whose removal would
split the survey in two counts as an *articulation* leg (Survex's term) and gets
skipped. Cavern creates the `.err` file on every solve, then writes into it only
when a traverse survives that test. So a dead-end side passage never appears
here, and a cave with no loops leaves the file empty. A loop built from 3
junction-to-junction runs prints 3 blocks, not 1.

Here is a block, with a blunder in it:

```
a1 - a2 - a3 - a4 - a5 - a1
Original length  78.40m (  5 shots), moved   1.83m ( 0.37m/shot). Error   2.33%
2.470000
H: 2.900000 V: 0.710000
```

The station names carry the same `cave_` prefix here as in the log. They are cut
short above to keep the block readable.

Line by line:

- **The station line** names the stations the traverse runs through, joined by
  ` - `. Stations joined by an *equate* get ` = ` instead, and a station with no
  name of its own prints as the literal `<fixed point>`.
- **Original length** gives the traverse's measured length, over that many
  **shots**. Survex's own term is *leg*; the US English messages CaveWhere ships
  say shot.
- **moved** measures the misclosure vector itself: how far the adjustment had to
  shift things to pull the traverse shut. The **m/shot** figure divides that by
  the shot count, 1.83 over 5 shots above.
- **Error** does arithmetic on the 2 numbers before it, `100 × moved ÷ Original
  length`, and nothing more. Scan for this one. A traverse of zero measured
  length prints `Error    N/A` instead, since a percentage of zero says nothing.
- **The bare number on the third line** is E, and **H** and **V** split it into
  Horizontal and Vertical parts. Each one is the square root of the actual
  squared error divided by the squared error your instruments should produce.
  That square root puts the interesting line at 1.0. At E = 1.0 the traverse
  closed exactly as well as the gear allows. The E = 2.47 above says it closed
  nearly 2.5 times worse, and that gap between actual and expected is what a
  blunder looks like. A high **H** with a low **V** points at the compass or the
  tape; a high **V** points at the clino.

Survex prints those last 3 values to 6 decimal places, as the block shows. This
page writes E = 2.47 for readability.

## How good is good enough?

No pass/fail line exists, and Survex enforces nothing. The bands below come from
house guidance for reading the **Error %**, not from a rule the software applies,
so treat them as a starting point. The right target depends on the traverse
length, the instruments, and how careful the survey was.

- **Under about 0.5%.** A tight traverse. Careful work with a laser rangefinder
  (a DistoX, say) routinely closes this well or better. Nothing to chase.
- **0.5% to 2%.** Normal for a long traverse, or for compass, clino and tape. The
  adjustment absorbs it without visibly distorting the map.
- **Above about 2%, go looking.** Past roughly 5% you almost certainly have a
  blunder rather than accumulated noise.

Two things keep that percentage honest:

- **It depends on traverse length.** A fixed reading error takes a much bigger
  slice of a 20 m loop than of a 200 m one. A short traverse can post an alarming
  percentage from an ordinary error, so check the absolute **moved** distance
  too, and ask whether a few centimeters deserve chasing.
- **E, H and V do not have that problem.** They compare the actual misclosure
  against what your instruments should produce, so traverse length cannot fool
  them. A high **Error %** with E near 1.0 means a short traverse, not a
  blundered one.

I recommend reading E first and the percentage second, and trusting E when the 2
disagree. Check out the example block above: 2.33% on its own might pass for a
long, sloppy loop, but E at 2.47 says the gear should have done far better.

## Track down the blunder

The report names a traverse, not a leg. Narrowing that down to the single wrong
reading takes detective work, and 3 techniques cover most of it.

### Check the front/back sight warning first

If you shot the leg both ways, CaveWhere flags a foresight and backsight that
disagree by more than **2°** right in the survey table, as **"Frontsight and
backsight differs by *x*°"**. See
[Fix Survey Errors](../survey-data/survey-errors.md). That gives you the sharpest
per-shot signal on the page, and it often lands you on the bad leg immediately.

### Walk the traverse for the usual suspects

Open the traverse's legs in
[Entering Survey Shots](../survey-data/enter-survey-data.md) and read down the 4
data columns, **Station**, **Distance**, **Compass** and **Vertical Angle**,
looking for:

- a **transposed digit** in any of them, `173` typed as `137`.
- a **Compass** reading about 180° off, shot the wrong way and not marked as a
  backsight.
- a **Distance** typed into the wrong row.
- an **up** or a **down** in **Vertical Angle** where a number belongs.

A blunder is usually one gross mistake, not a scattering of small ones.

### Break the tie-in on purpose

Not every blunder is a bad reading. A **tie-in error** is a bad *connection*: two
runs of passage joined at the wrong station, usually because someone mistyped or
reused a station name. Every measurement is fine and the topology is wrong.
Survex has to honor the connection you declared, so it hauls the whole second run
across the cave to reach the station you named, and the traverse closes badly.

To find the station that run really meets, open the loop on purpose. Add a
character, an `x` say, to one of the tie-in station's names so it stops matching
its counterpart. See below.

![Two plan views of the same small cave. On the left, trip 2's last station reads A2, so the solver hauls that whole run up to meet A2 and the loop closes badly. On the right, the same station now reads A2x; the junction breaks, the run stays anchored at A1 and swings out to where its own shots put it, and its free end lands beside A4, the station it belongs on.](../images/illustrations/break-the-tie-in.svg)
*Above shows the same small cave twice: tied in wrong, then broken loose.
Renaming one end of a bad tie-in frees the dragged run, and it settles next to
the station it belongs on.*

Breaking that junction does not break the plot. The left panel shows the run
being dragged; the right shows what happens once you cut it loose. The survey
stays connected through the loop's other end, so it still draws, and the end you
freed floats out to where its own shots put it. Look at that free end in the 3D
view: it lands nearest the station it *should* tie to, as shown in the right-hand
panel above. Rename it to that station and the loop closes.

The technique works best on a loop that started with a small error, because then
the free end drops almost exactly onto the right station. On a badly closing loop
the free end may land between 2 stations and settle nothing. And if it lands back
on the station it already carried, the tie-in held, so go back to the first 2
techniques.

### What CaveWhere will not tell you

A large closure error puts no red or yellow badge on any survey cell. Those
badges come from data checks (a missing reading, a name that fails to connect),
and they run as a separate system from loop closure. Again, see
[Fix Survey Errors](../survey-data/survey-errors.md). A misclosure surfaces in
exactly one place, the Cavern Output report, and it points at a traverse. The
front and back sight warning, plus your own eye, do the rest.

Once you fix the reading, nothing needs re-running. The solve re-fires, the loop
closes, and the 3D view and any affected scraps update on their own.

## When there are no loops

**"There are 0 loops."** describes the shape of the survey and nothing else. A
passage surveyed straight in and back out, or any cave that never reconnects to
itself, forms a tree. Nothing exists to close, so cavern leaves the `.err` file
empty and the **Loop closure** tab stays grayed out, as shown above. Phake Cave 3000
sits in exactly that state: 14 stations, 13 shots, 0 loops, and a 152.54 m total
the adjustment left untouched.

Loops appear the moment 2 runs of passage meet, whether a connecting lead pushed
through or a new trip tied back onto a station an earlier trip already set. Until
then, the 2° front and back sight tolerance check gives you your only in-survey
cross-check, and only on the legs you shot both ways.

## Where to go next

- **[Fix Survey Errors](../survey-data/survey-errors.md)**: the survey-table
  errors and warnings, including the 2° front and back sight tolerance check that
  finds a bad leg for you.
- **[Why CaveWhere](../concepts/why-cavewhere.md#keeping-the-map-correct-loop-closure)**:
  the reasoning behind an always-current, always-closed 3D map.
- **[How a Project Is Organized](../concepts/data-model.md#why-an-edit-ripples)**:
  why fixing one shot moves stations elsewhere and re-carpets old sketches.
