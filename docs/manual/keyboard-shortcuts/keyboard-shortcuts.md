---
title: Keyboard Shortcuts
summary: Every keyboard shortcut CaveWhere has — the File-menu accelerators, the keys that drive the survey table, and the handful of single keys the note editor, the 3D view, and the tools respond to.
problem: Enter and edit survey data without reaching for the mouse, and know which keys actually do something so you're not hunting for shortcuts that don't exist.
keywords: [keyboard, shortcut, shortcuts, accelerator, hotkey, key, tab, enter, escape, delete, space, arrow keys, ctrl, cmd, command, survey table, data entry, mouse-free]
related: [../survey-data/enter-survey-data.md, ../measurement/measure-distance-and-bearing.md, ../point-clouds/clip-a-point-cloud.md, ../scraps/digitize-a-scrap.md]
---

# Keyboard Shortcuts

## Why / when you need this

Entering survey data is a two-handed job: a book open in front of you, your eyes
on the page, and no time to keep reaching for the mouse between every reading. So
the **[survey table](../survey-data/enter-survey-data.md) is built to be driven
entirely from the keyboard** — that's where the shortcuts matter most, and where
they save the most time.

Everywhere else, CaveWhere leans on the mouse — you orbit and zoom the 3D model by
dragging, and most commands live on buttons and menus rather than behind hotkeys.
This page is the honest, complete list of the keys that *do* something, grouped by
where you are in the app, so you can commit the useful ones to muscle memory and
not waste time pressing keys that aren't bound to anything.

> **On macOS, use ⌘ (Command) wherever this page says Ctrl.** The File menu shows
> the ⌘ form automatically. The single-key shortcuts — Esc, Delete, Tab, the arrow
> keys, Space, Enter, and P — are the same on every platform.

## File and application

These live on the **File** menu, and the menu shows each one beside its item, so
you don't have to memorize them — but they're the fastest way to the things you do
most.

| Key | Does |
|-----|------|
| **Ctrl+N** | New project |
| **Ctrl+O** | Open a project |
| **Ctrl+Shift+O** | Open from Online — go to the remote-repository page |
| **Ctrl+S** | [Save](../projects-and-files/save-a-project.md) |
| **Ctrl+Q** | Quit CaveWhere |

**Ctrl+N** and **Ctrl+O** both check whether the current project has unsaved
changes first and offer to save, so neither can drop work on the floor. **Ctrl+S**
is greyed out only in the rare case where saving directly would lose data the
current project can't represent — the same guard that greys out the menu item (see
[Save a Project](../projects-and-files/save-a-project.md)).

## The survey table

This is the important set. Land on a cell, type, and move on — a whole trip can go
in without your hands leaving the keyboard. The full workflow is in
[Enter Survey Data](../survey-data/enter-survey-data.md#move-around-with-the-keyboard);
this is the reference.

| Key | Does |
|-----|------|
| **Arrow keys** | Move to the neighbouring cell in any direction |
| **Tab** / **Shift+Tab** | Move to the next / previous cell |
| **Enter** / **Return** | Start editing the cell; while editing, commit the value and move on |
| Any letter or digit | Start editing immediately, with that character as the first keystroke — you don't "open" a cell first |
| **Space** | Start a new [data block](../survey-data/enter-survey-data.md#start-a-new-data-block) (chunk) |
| **Esc** | While editing, abandon the change and put the old value back |
| **Delete** | While editing, clear the cell |

Two things make this fast in practice. Landing on an empty station at the end of a
run shows a greyed-out **guess** at the next name (`A1` → `A2`) with a *Press Tab*
hint — **Tab** accepts it. And cells that aren't shown — the backsight halves on a
foresight-only trip — are **skipped automatically** as you Tab across, so you only
ever land on cells you can actually type into.

## The 3D view

Navigating the 3D model is a **mouse job** — you drag to orbit, drag to pan, and
roll the wheel to zoom. There are no arrow-key or reset-view shortcuts; if you're
looking for one, it isn't there.

The one keyboard-assisted gesture is for point clouds:

| Key | Does |
|-----|------|
| **Hold P + scroll the wheel** | Resize the [point cloud's](../point-clouds/add-a-point-cloud.md) points instead of zooming the camera |

Hold **P** and the mouse wheel stops zooming and starts growing or shrinking the
rendered dots of a LiDAR point cloud; let go and the wheel zooms again. It works
wherever the pointer is over the 3D view — you don't have to click first. This is
the only control over point size; there's no slider.

## Notes and digitizing tools

When you're working on a [note](../notes/add-a-note.md) — tracing a scrap, placing
stations, marking leads — two keys come up repeatedly:

| Key | Does |
|-----|------|
| **Delete** / **Backspace** | Remove the selected item — a [scrap](../scraps/digitize-a-scrap.md) outline point, a station, or a lead |
| **Esc** | Cancel and close the active tool |

**Esc** backs out of whatever tool you started — drawing the scale line, placing
the north arrow, the coordinate picker, or [clipping a point
cloud](../point-clouds/clip-a-point-cloud.md) — without applying it, so a tool you
opened by mistake is one keystroke from gone.

## The measurement tool

| Key | Does |
|-----|------|
| **Esc** | Exit the [measurement tool](../measurement/measure-distance-and-bearing.md) |

While the measurement tool is active, **Esc** turns it off and clears the current
measurement. (Clicking starts and places points; only Esc leaves the tool.)

## Lists and buttons

A few keys work on focused controls throughout the app, the way you'd expect:

- **Enter**, **Return**, or **Space** activates a button that has keyboard focus.
- **Delete** removes the selected row from an editable list, such as a
  [trip's team](../survey-data/enter-survey-data.md).

## Where to go next

- **[Enter Survey Data](../survey-data/enter-survey-data.md)** — the shortcuts
  above in the context of actually filling in a trip.
- **[Measure Distance and Bearing](../measurement/measure-distance-and-bearing.md)**
  — the tool the measurement Esc belongs to.
- **[Add a Point Cloud](../point-clouds/add-a-point-cloud.md)** — where hold-P
  point sizing comes in.
