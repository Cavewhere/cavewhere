---
title: Organize Caves and Trips
summary: Create caves and trips, name and date them, record the team, read the cave's trip list, and remove what you no longer want.
problem: Structure a project so every shot is attached to the trip that measured it, and the calibration that corrects it.
keywords: [cave, trip, region, project, team, date, name, organize, add cave, add trip, roles, remove cave, remove trip]
related: [enter-survey-data.md, calibration.md, ../concepts/data-model.md, ../concepts/glossary.md]
---

# Organize Caves and Trips

## Why / when you need this

A cave never gets surveyed in one go. Different people, different days,
different instruments, sometimes across decades, and the map has to come out of
all of it.

The structure follows that:

**[Region](../concepts/glossary.md#region) → [Cave](../concepts/glossary.md#cave)
→ [Trip](../concepts/glossary.md#trip) → shots and notes**

The **trip** carries the weight. A trip means *one team, one day, one set of
instruments*, which makes it the unit that shares a [calibration](calibration.md)
and a [declination](declination.md). Everything measured on a given day needs the
same correction, and another day needs another one.
[How a Project Is Organized](../concepts/data-model.md) argues that shape at
length. This page builds it.

## Add a cave

Click **Data** in the sidebar for the cave list, then **Add Cave**. The
screenshot below shows the button highlighted. CaveWhere names the new cave from
the count already in the project (`Cave 1`, `Cave 2`, and on up), then opens its
page so you can rename it.

![The Data page. A bold heading gives the region's name, a Geospatial box holds a Coordinate system dropdown set to Local and a Layers count, and the Add Cave button below it is highlighted. Under it one cave is listed, with a warning triangle beside it, reading "Phake Cave 3000 is 152.54 m long and 36.41 m deep".](../images/survey-add-cave.png)
*The cave list, shown above. The bold name on top names the **region**, the
project's root, which here matches its only cave. Each row reads as a sentence:
**Phake Cave 3000 is 152.54 m long and 36.41 m deep**. The triangle flags
something inside that wants attention.*

The list doubles as the project's dashboard. Every solve of the survey writes
length and depth back into that row, and **Automatic Update** (foot of the
sidebar in the shot above, on by default) keeps those solves coming. Those 2
numbers then track your typing with nothing to refresh.

### Remove a cave

Right-click a cave row, or long-press it on a touchscreen, and the menu offers
**Remove** followed by the name. A red box then asks **Remove Phake Cave 3000?**
beside a **Remove** and a **Cancel** button.

Take that box seriously. Removing a cave takes every trip inside it, and every
note and scrap hanging off those trips. CaveWhere pushes the removal onto an undo
stack, but nothing in the UI reaches that stack: no Edit menu, no Ctrl+Z. Short
of closing without saving, or
[discarding back to your last save](../collaboration/review-history.md#discard-back-to-your-last-save),
that confirmation is what stands between you and the delete.

## Add a trip

Open a cave and click **Add Trip**. Trips get counted names too (`Trip 1`,
`Trip 2`), and the new one lands dated **today**. Today suits you if you type up
tonight, and misleads you if you work through a backlog.

**I recommend fixing the date before anything else.** Auto declination computes
for whatever date the trip carries. Date a trip wrong by a decade and it picks up
a declination wrong by however far the field drifted in that decade.

![A cave page. The cave's name heads a panel reading Length 152.54 m, Depth 36.41 m, Leads 5, Fix stations 0, Grid convergence n/a (no fix station). The highlighted Add Trip button sits beside Import Survex and Export, above a table with columns Name, Date, Stations, Length and Decl holding one trip: "Release 0.08", 2020-02-26, A 1-14, 152.54 m, 0° manual.](../images/survey-add-trip.png)
*A cave page, shown above. **Add Trip** and **Import Survex** sit over the trip
table. The cave measures 152.54 m and so does the trip, because this cave holds a
single trip.*

**Import Survex** beside it brings a trip in from an existing `.svx` file instead
of typing. See
[Import a Survex file as a new trip](../import-export/import-surveys.md#import-a-survex-file-as-a-new-trip).

The trip table runs 5 columns:

| Column | Shows |
|--------|-------|
| **Name** | The name, error icons in front, elided to fit 200 px |
| **Date** | `yyyy-MM-dd` |
| **Stations** | The **largest** run of consecutive stations, abbreviated: `A 1-14` |
| **Length** | How long the trip surveyed, in the trip's own units |
| **Decl** | Its [declination](declination.md), tagged **auto** or **manual** |

**Stations** reports one range. A trip covering `A1` through `A14` and then `B1`
through `B3` still shows `A 1-14`, and the B run goes unmentioned. That
limitation makes the column a label for the trip, not an inventory of it.

Length and Decl both round to 2 decimals and then drop trailing zeros, which is
why the row shown above reads `0°` and not `0.00°`.

Click a header to sort by that column, click it again to reverse, and a small
arrow marks whichever column drives the sort. Narrow the window past the panel
breakpoint and the table becomes a list with a **Name / Date / Stations /
Length** dropdown.

That **Decl** column makes for a fast audit. A cave whose trips run mostly *auto*
with one stray *manual* deserves a second look, and so does the reverse. Fix a
batch in place: shift-click or ctrl-click (cmd-click on macOS) a run of rows,
then right-click and pick **Declination → Auto**. Every selected trip flips at
once. A mixed selection leaves both **Auto** and **Manual** unchecked, so the
menu tells you where the trips stand before you touch it. Removing a trip sits in
that same menu, behind the same red confirmation.

## Name and date a trip

The name and date sit at the top of the trip page, above the survey table.
**Double-click** either one to edit it.

![The top of a trip's survey editor, under a Trip heading. The highlighted row holds the trip's name, "Release 0.08", on the left and its date, "2020-02-26", on the right. Below the row a banner reads "There are 4 warnings".](../images/survey-trip-name-date.png)
*The name on the left, the date on the right, as shown above. Neither answers a
single click.*

A **name** has to stay unique within its cave, and CaveWhere compares names
case-insensitively, so `Sump Push` and `sump push` collide. Uniqueness stops at
the cave, though: two caves in one project may each hold a `Trip 1`. The app also
turns down an empty name, a name padded with spaces, a leading or trailing dot,
and anything holding `\ / : * ? " < > |`.

Those rules come from the disk layout, not from taste. A `.cwproj` writes each
cave as a directory and each trip as a directory under it, so under the project's
data folder the trip above saves to
`Phake Cave 3000/trips/Release 0.08/Release 0.08.cwtrip`. What you type becomes a
folder name, which is where the rules come from.
[Project Formats](../projects-and-files/project-formats.md) has the full tree.

A rejected rename says nothing at all. The field snaps back to the old text and
no message appears. So when a rename looks like it failed, one of those rules
caught it.

Imports go the other way. A cave or trip arriving under a name already in use
gets ` 2` appended and goes in anyway. **Add Trip** can land in the same place,
since it names from the count. Delete the first of 3 trips and the next one you
add proposes a name still on the list, which comes out as `Trip 3 2`.

A **date** takes `yyyy-MM-dd` and keeps the day only, discarding any time on the
way in. It does real work in 3 places. Auto declination computes for it, since
declination drifts year on year. The trip also publishes a `Date` keyword
(`2020-02-26` for the trip above) and a `Year` keyword (`2020`). Both filter
[layers](../view-3d/the-3d-view.md#focus-on-part-of-the-cave-layers) in the 3D
view, as does the `Trip` keyword carrying the name.

Type something the format cannot parse and the field does not push back. The
parse fails quietly and the trip loses its date. On a trip set to **auto**,
declination falls back to the stored manual value and a warning appears:

> Trip has no date; auto declination unavailable. Using stored manual value.

Naming repays a moment's thought. These names are what you navigate by for years,
and they are your folder names besides. Whatever your team recognizes (a date, a
survey number, the passage, the people) beats `Trip 1`.

## Record the team

The **Team** section lists who came. The **+** beside the heading adds a blank
row; double-click the name cell to fill it in.

![The Team section of a trip: a Team heading with a + button, columns headed Name and Role, and one row reading "Philip Schuchardt" with a Developer role chip and a green "+ Role" button.](../images/survey-team.png)
*The Team section shown above. That row sits selected, which is the only reason
the **−** button and the green **+ Role** button appear. Click a row before you
reach for either.*

**The names do more than document.** Every non-empty name becomes a **Caver**
[keyword](../concepts/glossary.md#keyword) on the trip, and keywords drive
[layer visibility](../view-3d/the-3d-view.md#focus-on-part-of-the-cave-layers) in
the 3D view. So the team list is what lets you show only the passage a given
person surveyed. Check one caver's work, show someone what they found, or track
down whose trips a suspect pattern runs through.

Spell names consistently, then. `Phil` and `Philip` make 2 different cavers as
far as the keyword goes, and the filter can only match the list it gets.

The green **+ Role** button drops in a placeholder chip named `Role 1`, `Role 2`
and on up; double-click it to type what the job actually was. Clearing a chip's
text deletes the chip, and so does selecting it and pressing Delete. Roles stay
free text. Whatever your team calls the jobs (book, instruments, sketch, tape,
lead) is what to type. Unlike names, roles never become keywords.

When a shot looks wrong years later, the fastest way to settle it is to ask
whoever took the reading, and the team list holds the only record of who that
was.

## Next steps

- [Enter Survey Data](enter-survey-data.md) fills in the shot table itself.
- [Calibrate the Instruments](calibration.md) sets the per-trip corrections.
- [How a Project Is Organized](../concepts/data-model.md) covers why the tree has
  these levels.
