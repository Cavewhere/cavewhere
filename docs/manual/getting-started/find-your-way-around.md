---
title: Find Your Way Around
summary: A tour of the CaveWhere window — the sidebar that switches between the 3D view, your data and the map, the breadcrumb that says where you are, the sync button, the job list, and how the window rearranges itself as it gets narrower.
problem: Work out what you are looking at when CaveWhere opens, and how to get from the 3D cave to the trip you want to edit.
keywords: [tour, interface, window, sidebar, breadcrumb, link bar, navigation, view, data, map, back, forward, sync, jobs, address, responsive, narrow, hamburger]
related: [set-up-your-identity.md, ../view-3d/the-3d-view.md, ../survey-data/caves-and-trips.md]
---

# Find Your Way Around

## Why / when you need this

CaveWhere doesn't have one main screen. A cave is a 3D model *and* a tree of
caves, trips, shots and notes, and you spend the day moving between them — fix a
station's data, look at what it did to the passage, go back. So the window is
built around two controls: the **sidebar** picks what you're doing, and the
**breadcrumb** says where you are. Learn those two and nothing else in CaveWhere
is hard to find.

## What you're looking at

CaveWhere opens on the **View** page — your cave in 3D. On a new install that
cave is empty, because there's no survey in it yet.

The window looks the same wherever you are in it. Here it is on a trip's page,
which fills the breadcrumb in with something worth reading:

![The CaveWhere main window on a trip page. A sidebar runs down the left with File, View, Data and Map buttons and an Automatic Update toggle at the bottom; a bar across the top holds back and forward arrows, a breadcrumb reading "Source, Data, Cave=Phake Cave 3000, Trip=Release 0.08", and a sync button at the far right; the rest of the window is the trip's editor and its scanned notes.](../images/getting-started-tour.png)
*The three regions: the sidebar (left), the breadcrumb bar (top), and the page
itself, which fills everything else.*

Three regions, and that's the whole frame:

- **The sidebar**, down the left — which of the three parts of CaveWhere you're
  in, plus anything the app is busy doing.
- **The breadcrumb bar**, across the top — where you are, how to get back, and
  the sync button.
- **The page**, filling the rest — whatever you're actually working on.

## Switch between the 3D view, your data, and the map

The sidebar has three buttons:

- **View** — the [3D cave model](../view-3d/the-3d-view.md). Orbit it, filter it
  with layers, measure it.
- **Data** — the [caves and trips](../survey-data/caves-and-trips.md) the model is
  built from: survey tables, calibration, notes, scraps.
- **Map** — composing a map image to
  [export as a PNG, PDF or SVG](../import-export/export-a-map.md).

**Each button remembers where you were.** Click **Data**, drill down to a trip's
notes, click **View** to see what changed, then click **Data** again and you land
back on that note — not at the top of the tree. That's deliberate, and it's what
makes the edit-look-edit loop bearable: CaveWhere tracks the last page you
visited under each of the three and returns you to it.

Below the buttons, the sidebar lists **jobs** — background work like recomputing
scraps after you change a shot, each with a progress bar. CaveWhere doesn't block
you while it works, so the job list is where you look to see whether it's caught
up. At the bottom is the **Automatic Update** checkbox, on by default, which is
what keeps the model in step with your edits;
[Carpeting is automatic](../scraps/carpeting.md#carpeting-is-automatic) covers
what turning it off suspends.

**The File menu is somewhere different depending on your platform.** On macOS
it's in the system menu bar at the top of the screen, where macOS apps keep their
menus. On Windows and Linux it's a **File** button at the top of the sidebar.
Same menu either way.

## Follow the breadcrumb

The breadcrumb across the top is an address. Drilling into a trip's note builds
up something like:

```
Source / Data / Cave=Phake Cave 3000 / Trip=Release 0.08 / Note=001
```

It reads left to right, outside in — the cave contains the trip, which contains
the note. **Every crumb is a link:** click **Cave=Phake Cave 3000** to jump
straight back up to that cave, without retracing your steps.

To the left of it are **back** and **forward** arrows, which walk your history
the way a browser's do — useful when the place you want isn't an ancestor of
where you are, so the breadcrumb can't take you there.

The **...** button at the right of the trail swaps it for a text field holding
the raw address, which you can edit and press Enter on to jump anywhere directly.
Most people never need it; it's there for when you want to type rather than
click.

At the far right of the bar is the **sync button**, which shares your project
with a team and shows whether you're up to date. Its tooltip always says what
clicking it will do. Sync is opt-in and has its own chapter, still to be written
— if you're working alone you can ignore it entirely.

## The window rearranges itself as it gets narrower

CaveWhere is usable in a window far narrower than a full screen. As the window
shrinks, things move rather than vanish:

- **Below about 800 pixels wide**, the sidebar shrinks to a narrower strip —
  same View / Data / Map buttons, smaller icons and smaller labels.
- **Below about 500**, the sidebar goes away entirely and its View / Data / Map
  entries move into a **hamburger menu** (**☰**) at the left of the breadcrumb
  bar.

The page you're on adapts on its own, and it measures its own width rather than
the window's — so the sidebar's width counts against it:

- **Tables become lists.** A cave's trip table — Name, Date, Stations, Length,
  Decl in columns — collapses to one trip per row with the same facts wrapped
  underneath, and the sortable column headers are replaced by a sort dropdown and
  an ascending/descending button. The cave's leads and its fixed stations do the
  same.
- **The 3D view's side panel becomes a drawer.** Instead of the View and Layers
  tabs taking a permanent column beside the model, buttons float over the 3D view
  and slide the panel in over it — so the cave keeps the width.
- **On a wide page the note thumbnails move out of the editor.** Give a trip page
  about 1200 pixels and the Notes list inside the survey editor is replaced by a
  strip of thumbnails down the side of the note itself, so you can flip between
  notes without scrolling the editor to find them.

Nothing is taken away at any size; the same controls are reached differently. If
a button you expect is missing, widen the window and it will be where the manual
says.

## Next steps

- [The 3D View](../view-3d/the-3d-view.md) — the page CaveWhere opens on.
- [Organize Caves and Trips](../survey-data/caves-and-trips.md) — what lives
  under **Data**.
- [Open a Project](../projects-and-files/open-a-project.md) — getting a cave into
  that empty window.
