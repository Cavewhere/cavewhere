---
title: Find Your Way Around
summary: A tour of the CaveWhere window: the sidebar that switches between the 3D view, your data and the map, the breadcrumb that says where you are, the sync button, the job list, and how the window rearranges itself as it gets narrower.
problem: Work out what you are looking at when CaveWhere opens, and how to get from the 3D cave to the trip you want to edit.
keywords: [tour, interface, window, sidebar, breadcrumb, link bar, navigation, view, data, map, back, forward, sync, jobs, address, responsive, narrow, hamburger]
related: [set-up-your-identity.md, ../view-3d/the-3d-view.md, ../survey-data/caves-and-trips.md]
---

# Find Your Way Around

## Why / when you need this

CaveWhere has no single main screen. A cave exists as a 3D model *and* as a tree
of caves, trips, shots and notes, and you spend the day moving between the two.
Fix a station's data, look at what it did to the passage, go back. So the window
hangs on two controls. The **sidebar** picks what you are doing, and the
**breadcrumb** says where you are. Learn those two and nothing else in CaveWhere
stays hidden for long.

## What you're looking at

CaveWhere opens on the **View** page, your cave in 3D. On a new install that cave
sits empty, because no survey has gone into it yet.

The window keeps the same frame wherever you go. The screenshot below shows a
trip's page, which fills the breadcrumb with something worth reading.

![The CaveWhere main window on a trip page. A sidebar runs down the left with File, View, Data and Map buttons and an Automatic Update toggle at the bottom; a bar across the top holds back and forward arrows, a breadcrumb reading "Source, Data, Cave=Phake Cave 3000, Trip=Release 0.08", and a sync button at the far right; the rest of the window is the trip's editor and its scanned notes.](../images/getting-started-tour.png)
*The three regions: the sidebar (left), the breadcrumb bar (top), and the page
itself, which fills everything else.*

Three regions make up the whole frame:

- **The sidebar**, down the left: which of CaveWhere's three parts you sit in,
  plus anything the app currently works on.
- **The breadcrumb bar**, across the top: where you are, how to get back, and the
  sync button.
- **The page**, filling the rest: whatever you actually work on.

## Switch between the 3D view, your data, and the map

The sidebar holds 3 buttons:

- **View**: the [3D cave model](../view-3d/the-3d-view.md). Orbit it, filter it
  with layers, measure it.
- **Data**: the [caves and trips](../survey-data/caves-and-trips.md) behind the
  model, holding survey tables, calibration, notes, and scraps.
- **Map**: composing a map image to
  [export as a PNG, PDF or SVG](../import-export/export-a-map.md).

**Each button remembers where you were.** Click **Data**, drill down to a trip's
notes, click **View** to see what changed, then click **Data** again and you land
back on that note instead of at the top of the tree. CaveWhere tracks the last
page you visited under each of the 3, which is what makes the edit-look-edit loop
bearable.

Below the buttons, the sidebar lists **jobs**: background work such as
recomputing scraps after you change a shot, each with a progress bar. CaveWhere
never blocks you while it works, so the job list tells you whether it has caught
up. At the bottom sits the **Automatic Update** checkbox, on by default, which
keeps the model in step with your edits.
[Carpeting is automatic](../scraps/carpeting.md#carpeting-is-automatic) covers
what switching it off suspends.

**The File menu sits somewhere different depending on your platform.** On macOS
it lives in the system menu bar at the top of the screen, where macOS apps keep
their menus. On Windows and Linux it becomes a **File** button at the top of the
sidebar. Same menu either way.

## Follow the breadcrumb

The breadcrumb across the top works as an address. Drilling into a trip's note
builds up something like:

```
Source / Data / Cave=Phake Cave 3000 / Trip=Release 0.08 / Note=001
```

It reads left to right, outside in: the cave contains the trip, which contains
the note. **Every crumb is a link.** Click **Cave=Phake Cave 3000** to jump
straight back up to that cave without retracing your steps.

To its left sit **back** and **forward** arrows, which walk your history the way
a browser's do. They earn their keep when the place you want sits outside the
current trail, where the breadcrumb cannot reach it.

The **...** button at the right of the trail swaps the trail for a text field
holding the raw address. Edit it, press Enter, and you jump anywhere directly.
Most people may never need it; it exists for the times you would sooner type
than click.

At the far right of the bar sits the **sync button**, which shares your project
with a team and shows whether you hold the current version. Its tooltip always
says what clicking it will do. Sync stays opt-in and has its own chapter,
[How Collaboration Works](../collaboration/how-sync-works.md). Working alone, you
can ignore it entirely.

## The window rearranges itself as it gets narrower

CaveWhere stays usable in a window far narrower than a full screen. As the window
shrinks, things move instead of vanishing. `Theme.qml` holds the exact
thresholds:

| Width | What changes |
|---|---|
| **Below 800 px** | The sidebar narrows from 80 px to a 50 px strip, keeping the same View / Data / Map buttons at smaller icon and label sizes. |
| **Below 500 px** | The sidebar disappears, and its View / Data / Map entries move into a **hamburger menu** (**☰**) at the left of the breadcrumb bar. |

The page itself adapts separately, and it measures its own width instead of the
window's, so the sidebar's width counts against it. That catches people out: a
1250 px window still falls short of the 1200 px gallery threshold once the
80 px sidebar takes its cut.

- **Below 600 px, tables become lists.** A cave's trip table, with Name, Date,
  Stations, Length and Decl in columns, collapses to one trip per row carrying
  the same facts wrapped underneath, and a sort dropdown plus an
  ascending/descending button take over from the sortable column headers. The
  cave's leads and its fixed stations do the same.
- **Below 600 px, the 3D view's side panel becomes a drawer.** The View and
  Layers tabs give up their permanent column beside the model; buttons float over
  the 3D view and slide the panel in over it, so the cave keeps the width.
- **At 1200 px and wider, a trip page moves its note thumbnails out of the
  editor.** The Notes list inside the survey editor gives way to a strip of
  thumbnails down the side of the note itself, so you can flip between notes
  without scrolling the editor to find them.

No size takes anything away; the same controls simply arrive by a different
route. If a button you expect has gone missing, widen the window and it will sit
where the manual says.

I recommend keeping the window at 1200 px or wider while you digitize scraps.
That is where the note gallery appears, and below 500 px every page switch costs
an extra click through the hamburger menu.

## Next steps

- [The 3D View](../view-3d/the-3d-view.md): the page CaveWhere opens on.
- [Organize Caves and Trips](../survey-data/caves-and-trips.md): what lives
  under **Data**.
- [Open a Project](../projects-and-files/open-a-project.md): getting a cave into
  that empty window.
