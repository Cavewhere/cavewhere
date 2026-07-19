---
title: Check Loop Closure and Find Blunders
summary: Read CaveWhere's Cavern Output page to see whether your loops close, and use the loop-closure report to track down a blunder — a transposed digit or a reversed reading — before it warps the map.
problem: A survey that comes back to a station it already visited should close on itself; when it doesn't, one reading is wrong, and you want to find which one before the error is baked into the drawn map.
keywords: [loop closure, misclosure, blunder, closure error, tie-in, tie-in error, cavern, survex, network adjustment, solve, error percent, traverse, loop, survey error, backsight]
related: [../survey-data/survey-errors.md, ../concepts/why-cavewhere.md, ../concepts/glossary.md, ../concepts/data-model.md, ../survey-data/enter-survey-data.md]
---

# Check Loop Closure and Find Blunders

## Why / when you need this

Every instrument reading carries a little error. Most of the time that error is
small and random, and it stays small. But when a survey route returns to a
station it already visited — a **loop** — the two paths that reach that station
almost never meet exactly. The gap is the
**[misclosure](../concepts/glossary.md#loop-closure)**, and it is the single best
cross-check you have on your data. A small misclosure is just the instruments
being instruments. A *large* one almost always means a **blunder**: a transposed
digit (`173` written `137`), a compass read backwards, a distance typed into the
wrong row.

The point of checking loop closure is to catch that blunder now — while you still
remember the trip and can re-check the book — rather than six months later when
the passage that should line up on the map doesn't. This page shows where
CaveWhere reports closure, how to read the report, and how to run down the reading
that's throwing it off.

## CaveWhere closes loops for you

You don't run loop closure as a separate step. CaveWhere embeds
[Survex](../concepts/glossary.md#survex) — the long-established open-source
cave-survey engine — and its `cavern` solver does a proper **least-squares network
adjustment** every time your survey data changes. Rather than dumping all the
misclosure onto the last shot, it spreads the error across the whole loop,
weighted by each leg, to find the most likely position for every station. That is
the adjusted [line plot](../concepts/glossary.md#loop-closure) you see in the
[3D view](../view-3d/the-3d-view.md).

Because the solve is automatic, fixing a blunder is self-correcting: you change
the bad reading, the network re-solves, the loop closes, and — because scraps are
tied to stations — any [carpeted](../concepts/glossary.md#carpeting) sketches
re-drape to follow. See [Why an edit ripples](../concepts/data-model.md#why-an-edit-ripples)
for that cascade. (The solve is what
[Automatic Update](../scraps/carpeting.md#carpeting-is-automatic) keeps
current; with it switched off, loop closure stops running until you turn it back
on.)

## Open the Cavern Output page

The solver's own report lives on the **Cavern Output** page. Open it from the
**Data** page: click the **⋯** menu at the top of the region and choose **Cavern
Output**. (It's also in the **File** menu as **Cavern Output…**.) If the last
solve failed, the menu item reads **Cavern Output (solve error)** — a standing
signal that something needs your attention.

![The Cavern Output page. A status line reads "Last solve completed successfully"; a Solve button sits at the top right; and a highlighted tab bar shows two tabs — "Cavern log", selected, and "Loop closure", greyed out. The log below reports "Survey contains 14 survey stations, joined by 13 shots. There are 0 loops," followed by the survey's total and adjusted lengths.](../images/loop-closure-summary.png)
*The Cavern Output page for the example cave. The status line confirms the solve
ran; the highlighted tab bar is where the two reports live. The **Loop closure**
tab is greyed out here because this cave is a single line of passage with no loops
to close (the log says "There are 0 loops"), so there is no closure report to
show.*

The page has:

- A **status line** — **"Last solve completed successfully"**, or a red
  **"Cavern reported an error during the last solve"** with the message below it.
- A **Solve** button that forces a fresh solve. You rarely need it, since the
  solve is automatic, but it's there to re-run on demand.
- A **Cavern log** tab — Survex's plain-text summary of the whole solve. The line
  that matters most for closure is **"There are *N* loops"**: it tells you at a
  glance whether this cave even has loops to check, alongside the station and shot
  counts and the survey's total length (both raw and after adjustment).
- A **Loop closure** tab — the per-loop error report, described next. It is
  **enabled only when the cave has at least one loop**; a loop-free survey leaves
  it greyed out, exactly as shown above.

## Read the loop-closure report

When a cave has loops, the **Loop closure** tab fills with one block per loop.
Each block reads something like this (an example — a loop with a blunder in it):

```
a1 - a2 - a3 - a4 - a5 - a1
Original length  78.40m (  5 legs), moved   1.83m ( 0.37m/leg). Error   2.33%
2.47
H: 2.90 V: 0.71
```

Reading it line by line:

- The **first line** lists the stations in the loop, so you know exactly which run
  of passage it covers.
- **Original length** is the loop's total measured length, over that many
  **legs**.
- **moved** is how far the adjustment had to shift things to pull the loop shut —
  the misclosure itself — and the **m/leg** figure spreads that across the legs.
- **Error** is the headline number: the misclosure as a **percentage of the loop
  length**. This is what you scan for. A few tenths of a percent is a healthy
  loop; a couple of percent or more says a leg in this loop is wrong.
- The last two lines are the error measured against how much error the
  *instruments alone* should produce (an overall figure, then split into
  **H**orizontal and **V**ertical parts). Values close to 1 mean the loop closed
  about as well as the instruments allow; values well above 1 mean it closed
  *worse* than the gear can explain — the signature of a blunder rather than
  ordinary noise. A high **H** with a low **V** points at a compass or distance
  mistake; a high **V** points at the clino.

## How good is good enough?

There's no single pass/fail line, but these rough bands are a good starting point
for reading the **Error %**. Treat them as guidelines, not rules — the right target
depends on how long the loop is, what instruments you used, and how careful the
survey was:

- **Good — under about 0.5%.** A tight loop. Careful work with modern digital
  instruments (a laser rangefinder / DistoX and the like) routinely closes this
  well or better. Nothing to chase.
- **Okay — about 0.5% to 2%.** Perfectly usable, and normal for longer loops or
  traditional compass, clino, and tape. The adjustment absorbs it without visibly
  distorting the map. Worth a glance if you want precision, but not a red flag.
- **Bad — above about 2%, and near-certainly a blunder past ~5%.** Investigate
  before you trust the loop. The larger the percentage, the more likely one
  reading — or one tie-in — is simply wrong, rather than the error being ordinary
  noise.

Two caveats keep the percentage honest:

- **It is length-dependent.** A *short* loop can show an alarming percentage from a
  perfectly ordinary absolute error, because a fixed reading error is a bigger
  slice of a short loop; a long loop averages that same error down to a small
  percentage. So don't panic over a high percentage on a very short loop — check
  the absolute **moved** distance too, and ask whether a few centimetres is really
  worth chasing.
- **The E and H:V lines don't have that problem.** Because they compare the actual
  misclosure to the error your instruments *should* produce, they aren't fooled by
  loop length the way the percentage can be. A high Error % but an **E near 1**
  means the loop closed about as well as the gear allows — it's just short, not
  blundered. A high **E** is the stronger blunder signal, so when the two disagree,
  trust E.

## Track down the blunder

The report tells you *which loop* is bad, not *which leg*. Narrowing it to the
single reading is detective work, and a few techniques make it quick:

- **Check the front/back sight warning first.** If the leg was shot both ways, a
  disagreement between the foresight and backsight is flagged right in the survey
  table as **"Frontsight and backsight differs by *x*°"** — see
  [Fix Survey Errors](../survey-data/survey-errors.md). That is your sharpest
  per-shot signal, and it often lands you on the bad leg immediately.
- **Know the usual suspects.** Walk the loop's legs in the
  [survey table](../survey-data/enter-survey-data.md) and look for a **transposed
  digit**, a **compass reading about 180° off** (shot the wrong way and not marked
  as a backsight), a **mistyped distance**, or an **up/down** that should be a
  number. A blunder is usually one gross mistake, not many small ones.
- **Suspect a tie-in error, and break the loop to find it.** Not every blunder is
  a bad reading. A **tie-in error** is a bad *connection*: two runs of passage
  joined at the wrong station, usually because a station name was mistyped or reused
  so the survey ties in where it shouldn't. The measurements are all fine; the
  topology is wrong. To pin it down, **open the loop on purpose**: add a character —
  an `x`, say — to one of the tie-in station's names so it no longer matches its
  counterpart. That breaks the junction, but the survey stays connected through the
  loop's *other* end, so it still plots — and the end you just freed floats out to
  where its own shots put it. Now look at that free end in the
  [3D view](../view-3d/the-3d-view.md): it lands nearest the station it *should*
  tie to, which tells you the correct name. When the loop's error was low to begin
  with, the free end drops almost exactly onto the right station and the tie-in is
  obvious. Rename it to the correct station and the loop closes. (If the free end
  lands right back on the station it was already named for, the tie-in was fine —
  the blunder is a reading, so go back to the first two techniques.)

It's worth being clear about what CaveWhere does *not* do: a large closure error
does **not** put a red or yellow badge on any survey cell. Those badges come from
data checks — a missing reading, a name that doesn't connect — and are a separate
system from loop closure (again, [Fix Survey Errors](../survey-data/survey-errors.md)).
The closure report is where the misclosure lives, and it points you at a loop; the
front/back sight warning and your own eye do the rest.

Once you fix the reading, there's nothing to re-run: the solve re-fires, the loop
closes, and the 3D view and any affected scraps update on their own.

## When there are no loops

**"There are 0 loops"** is not a problem — it's just the shape of the survey. A
passage surveyed straight in and back out, or any cave that never reconnects to
itself, is a tree with no loops, so there is nothing to close and the Loop closure
tab stays empty. Loops appear the moment two runs of passage meet: a connecting
lead pushed through, or a new trip tied back onto a station an earlier trip
already set. Until then, the front/back sight tolerance check is your in-survey
cross-check on each leg.

## Where to go next

- **[Fix Survey Errors](../survey-data/survey-errors.md)** — the survey-table
  errors and warnings, including the front/back sight tolerance check that finds a
  bad leg for you.
- **[Why CaveWhere](../concepts/why-cavewhere.md#keeping-the-map-correct-loop-closure)**
  — the reasoning behind an always-current, always-closed 3D map.
- **[How a Project Is Organized](../concepts/data-model.md#why-an-edit-ripples)**
  — why fixing one shot moves stations elsewhere and re-drapes old sketches.
