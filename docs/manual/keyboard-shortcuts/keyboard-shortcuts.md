---
title: Keyboard Shortcuts
summary: Every key CaveWhere binds: 5 File-menu accelerators, the survey table's set, the docs viewer's find keys, and the tools' single keys.
problem: Enter and edit survey data without reaching for the mouse, and know which keys actually do something so you're not hunting for shortcuts that don't exist.
keywords: [keyboard, shortcut, shortcuts, hotkey, tab, enter, escape, delete, backspace, space, arrow keys, ctrl, cmd, survey table, data entry, find in page]
related: [../survey-data/enter-survey-data.md, ../measurement/measure-distance-and-bearing.md, ../point-clouds/clip-a-point-cloud.md, ../scraps/digitize-a-scrap.md]
---

# Keyboard Shortcuts

## Why / when you need this

Entering survey data is a two-handed job: a book in front of you and no time to
reach for the mouse between readings. So the **[survey
table](../survey-data/enter-survey-data.md) runs entirely from the keyboard**,
where the shortcuts earn their keep.

Everywhere else CaveWhere leans on the mouse: most commands live on buttons and
menus. To build this list I read every `Shortcut` and `Keys` handler in the
source, so a key missing here is one CaveWhere does not bind itself. Two that
look real are not: `Ctrl+P` and `Ctrl+.` sit in `FileMenu.qml` commented out.

> **On macOS, press ⌘ (Command) wherever this page says Ctrl.** The File menu
> shows the ⌘ form itself. Esc, Delete, Backspace, Tab, the arrow keys, Space,
> Enter and P are identical everywhere. The docs viewer's find keys are the
> exception, and they get their own table below.

## File and application

These 5 live under **File**, a menu bar on macOS and a sidebar button
elsewhere. Each shows its key, and macOS moves Quit to the application menu.

| Key | Does |
|-----|------|
| **Ctrl+N** | New project |
| **Ctrl+O** | Open a project |
| **Ctrl+Shift+O** | *Open from Online…*, the remote-repository page |
| **Ctrl+S** | [Save](../projects-and-files/save-a-project.md) |
| **Ctrl+Q** | Quit CaveWhere |

**Ctrl+N** and **Ctrl+O** both check for unsaved changes and offer to save first,
so neither drops work on the floor. **Ctrl+S** grays out in the one case where a
direct save would lose data the current project cannot represent, the same guard
that grays the menu item.

## The survey table

Land on a cell, type, move on. The full workflow is in [Enter Survey
Data](../survey-data/enter-survey-data.md#move-around-with-the-keyboard).

| Key | Does |
|-----|------|
| **Arrow keys** | Move to the neighboring cell in any direction |
| **Tab** / **Shift+Tab** | Move to the next / previous cell |
| **Enter** / **Return** | Start editing; while editing, commit and move on |
| Any character the cell accepts | Start editing, with that character already typed |
| **Space** | Start a new [data block](../survey-data/enter-survey-data.md#start-a-new-data-block) (chunk), from any cell |
| **Esc** | While editing, abandon the change and restore the old value |
| **Delete** / **Backspace** | While editing, clear the value |

Three of those rows carry a catch. *Any character the cell accepts* means
exactly that: the validator runs on your keystroke, so a letter typed into a
distance cell does nothing. **Space** works mid-edit too, committing first, and
CaveWhere will not stack 2 empty blocks: with an unused one at the end, Space
moves your focus there. **Delete** and **Backspace** work only inside the
editor, where opening a cell selects the whole value so one press wipes it.
Outside it, neither does anything.

Landing on an empty station at the end of a run puts a bold gray **Press Tab**
over the guessed next name (`A1` → `A2`). Tab accepts it, and so does clicking
the hint. Cells the trip does not use, the backsight halves on a foresight-only
trip, get skipped as you Tab across.

## The 3D view

Navigating the 3D model is a **mouse job**: drag to orbit, drag to pan, roll the
wheel to zoom. Arrow keys do nothing here, and no reset-view key exists.

| Key | Does |
|-----|------|
| **Hold P + scroll the wheel** | Resize the [point cloud's](../point-clouds/add-a-point-cloud.md) points instead of zooming the camera |

Hovering the view hands it focus, so hold-**P** needs no click first. It takes
the wheel even with no cloud loaded, leaving nothing to resize. This is the only
control over point size, and no slider exists.

## Notes and digitizing tools

On a [note](../notes/add-a-note.md), tracing a scrap or placing stations, these
come up repeatedly:

| Key | Does |
|-----|------|
| **Delete** / **Backspace** | Remove the selected item: a [scrap](../scraps/digitize-a-scrap.md) outline point, a station, a LiDAR station, or a lead |
| **Esc** | Cancel and close the active tool |

**Esc** backs out of every tool that has a keyboard exit: the scale line, the
north arrow, the coordinate picker, the LiDAR add-station and two-point tools,
and [clipping a point cloud](../point-clouds/clip-a-point-cloud.md). Nothing gets
applied, so a mistaken tool is one keystroke from gone.

## The measurement tool

| Key | Does |
|-----|------|
| **Esc** | Exit the [measurement tool](../measurement/measure-distance-and-bearing.md) |

Esc is not the only way out. Clicking **Measure** again turns the tool off, and
so does starting **Pick** or **Clip**: the 3 take turns.

## The documentation viewer

The built-in help is the one place the key itself changes, not just the
modifier. Qt takes these from the desktop's standard find sequences, so yours
may differ.

| Key | Does |
|-----|------|
| **Ctrl+F** / **⌘F** | Find in page |
| **F3** / **⌘G** | Next match |
| **Shift+F3** / **⌘⇧G** | Previous match |
| **Enter** / **Shift+Enter** | Next / previous, inside the find field |
| **Esc** | Close the find bar |

Next and previous work only with the find bar open.

## Lists and buttons

- **Enter**, **Return**, or **Space** presses a focused button, **Pick**,
  **Clip** and **Measure** included.
- **Delete** removes a focused **Role** chip from a member of a
  [trip's team](../survey-data/enter-survey-data.md). No key removes the member
  row itself.

## Where to go next

- **[Enter Survey Data](../survey-data/enter-survey-data.md)**, these keys inside
  a real trip.
- **[Add a Point Cloud](../point-clouds/add-a-point-cloud.md)**, where hold-P
  sizing comes in.
