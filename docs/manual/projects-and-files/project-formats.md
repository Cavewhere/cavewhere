---
title: Choose a Project Format
summary: The difference between a .cwproj directory and a bundled .cw file, which one to pick, how to convert, and what CaveWhere does with an old .cw.
problem: Pick the project format that suits how you work, and understand what happened to the .cw file you have had for years.
keywords: [format, cwproj, cw, bundle, directory, zip, sqlite, legacy, convert, save as, git, version history]
related: [save-a-project.md, open-a-project.md, ../getting-started/why-cavewhere.md]
---

# Choose a Project Format

## Why / when you need this

The first time you save, CaveWhere asks for a **directory** or a **bundle**, and
the choice is not cosmetic. It decides *when* your work reaches the file you
named, how long each save takes, and what you can do with the result afterward.

Both formats hold the same data and the same complete history, and Save As
converts either way. So choose by the **size and life** of the
project, not by what sits inside it.

## The short answer

- **A small, one-off cave**, one cave and a trip or two, something you will
  finish and file away: **Bundle (`.cw`)**. One file to name, back up, and find
  again.
- **A big project**, several caves, years of trips, a team syncing: **Directory
  (`.cwproj`)**.
- **Handing it to someone, whatever it is**: **Bundle (`.cw`)**, made with
  [Save As](save-a-project.md#save-a-copy-of-a-project) so your working copy stays
  put.

**The line falls there because a bundle's cost scales with the whole project.**
Every save deflates all of it, including the note scans, which already arrive
compressed and shrink by almost nothing for the trouble. On one cave and a few
notes that runs instantly and you may never notice. On a multi-cave project
carrying years of scans you feel it on every `Ctrl+S`. A directory save writes
only the files that changed, so it does not care how big the project grows.

Changed your mind? **Save As** and pick the other one. Nothing gets lost either
way, history included.

## Directory (`.cwproj`)

A folder, on your disk, that CaveWhere writes into continuously.

CaveWhere defaults to this one, and it suits a project with a future: the
multi-cave system you will still add to in 5 years, or anything a team syncs.
**CaveWhere writes your edits to disk as you make them.** Saving marks a version
rather than rescuing data (see
[Save a Project](save-a-project.md#what-save-actually-does)).

Saves stay fast however large the project grows, because only changed files get
written. And the data sits in ordinary files in an ordinary folder, so you can
open a note scan, back the folder up, or point another tool at it without
CaveWhere's help. That matters more, not less, as a project accumulates years of
other people's work.

## Bundle (`.cw`)

One file, holding a compressed copy of everything a directory project contains.

Right for a **small, one-off cave**, the pit you surveyed in a weekend and will
not return to. Right too for handing any project to someone, or archiving a
finished one. One file to attach, copy, or file away, with no folder of parts to
keep together. At that size the format's costs, below, stay too small to notice.

**One trade comes with it.** Opening a bundle unpacks it into a temporary folder
and CaveWhere works there. Auto-save lands in *that* folder rather than in the
`.cw`, and **Save** repacks the project back over the file you named. So with a
bundle, Save is the step that puts your work into the file, which is why
CaveWhere asks for it before you close one.

Save times vary with the whole project's size, not with what you edited, so they
grow as the cave does. That drawback makes the bundle a poor home for a project
that keeps expanding and a fine one for a finished cave. **I recommend Directory
unless the cave is done.**

## The comparison, as CaveWhere states it

The **?** button next to the format picker opens the app's own 7-row summary,
shown below:

| | Directory (`.cwproj`) | Bundle (`.cw`) |
|---|---|---|
| **Best for** | Active, multi-trip, or team projects | Sharing or archiving |
| Continuous auto-save | ✓ | — |
| Single portable file | — | ✓ |
| Speed | Fast | Slower (compresses on save) |
| Direct file access | ✓ | Requires extracting first |
| Version history | ✓ | ✓ |
| Remote sync | ✓ | ✓ |

The last 2 rows repay a second look, because people expect them to differ and
they do not. **A bundle is a zipped-up directory project**, and the zip carries
`.git` across with everything else, so a bundle keeps every version and still
syncs with a remote. Emailing someone a `.cw` hands them the whole story of the
cave, not a flattened snapshot of today.

## What's inside a `.cwproj`

Nothing you need day to day. But you *can* look, and that is rather the point of
the format. Data this expensive should not sit locked inside an app.

> **Writing a tool that reads or writes CaveWhere projects?** Don't work from this
> section, it's a sketch for orientation. The
> **[CaveWhere File Format Specification](../../cavewhere-format-spec.md)**
> (`docs/cavewhere-format-spec.md`) aims at implementers rather than surveyors:
> the protobuf schema for every entity, the name and path rules, the version
> history, and how bundling, saving, and Git/LFS work.

Check out the layout below:

```
Jaws of the Beast/              the outer folder (also a Git repository)
├── Jaws of the Beast.cwproj    the project file
├── Jaws of the Beast/          your data, named after the project
│   └── Cave Name/
│       ├── Cave Name.cwcave
│       └── trips/
│           └── Trip Name/
│               ├── Trip Name.cwtrip
│               └── notes/
│                   ├── scan.jpg          the original note image
│                   └── scan.jpg.cwnote
├── GIS Layers/                 point clouds, each beside its own sidecar
├── .gitattributes              which extensions Git LFS takes over
└── .git/                       the version history
```

Every `.cw*` file CaveWhere writes here holds **JSON**, indented and readable in
any text editor. That covers 6 extensions: `.cwproj`, `.cwcave`, `.cwtrip`,
`.cwnote`, `.cwnote3d` (LiDAR), and `.cwlaz` (point cloud). The
`.cwproj` file itself is a short descriptor:

```json
{
 "fileVersion": { "version": 9, "cavewhereVersion": "2026.4.3-409-gda9d7fe1" },
 "name": "Jaws of the Beast", "id": "9f2c…",
 "metadata": { "dataRoot": "Jaws of the Beast", "syncEnabled": true }
}
```

`version` names the **format** version, the 10th since 0.07. This build reads and
writes 9, shipped as 2026.4. That field is what the
[Read-only banner](save-a-project.md#read-only-projects) checks; above 9 it names
no version at all. `cavewhereVersion` only records the build that
last wrote the file.

Note images and point clouds keep their **original files**, untouched and under
their own names, with `.gitattributes` handing them to Git LFS. If CaveWhere
vanished tomorrow, your scans would still sit in `notes/`.

Two details matter if you go poking around:

- **The outer folder is not the `.cwproj`.** The folder has no extension; the
  `.cwproj` is a file *inside* it. To move a project, move the whole folder.
- **Renaming the project renames folders.** The data folder takes its name from
  the project, so editing the project name (see
  [Save a Project](save-a-project.md#name-the-project)) physically renames the
  data folder and the `.cwproj` file. The outer folder keeps the name you gave it
  at save time.

## Legacy `.cw` files (v6 and older)

Old CaveWhere projects come as a single `.cw` file too, but a completely
different thing sits inside: a **SQLite database** where a modern bundle holds a
zip. File version 6 shipped as CaveWhere 2025.3, so anything from that release or
earlier lands here. CaveWhere still reads them and asks for nothing special:
**opening one converts it automatically**.

The part to know about is what the **first save** does, because CaveWhere does
not announce it:

> **Your old `.cw` keeps its name and becomes a bundle.** Saving writes a modern
> bundled `.cw` over the file you opened. Your survey comes through whole, a
> change of container rather than of contents, but the file stops being a SQLite
> database, so an **older** CaveWhere fails to open it afterward. It is a one-way
> upgrade. Copy the original somewhere else first if you still need an old
> version to read it.

Once converted it behaves as an ordinary bundle, so everything above applies,
**Save As** to a directory included.

Two footnotes:

- Because the file becomes a bundle, the Save As dialog **pre-selects Bundle**
  for a converted project, keeping the single-file shape you already had. Choose
  Directory for the other one.
- An old `.cw` sometimes turns up **read-only**: off a CD, out of an archive, or
  with its permissions locked down. CaveWhere cannot write back over it, so it
  treats the converted project as
  [temporary](save-a-project.md#your-project-already-exists) and sends you to
  Save As. Nothing touches the original.

## Convert between formats

No Export exists. **Save As is the converter**, and the format follows the
dialog's picker:

| From | To | Do this |
|---|---|---|
| Directory | Bundle | Save As → **Bundle (`.cw`)** |
| Bundle | Directory | Save As → **Directory (`.cwproj`)** |
| Legacy `.cw` | Either | Open it, then Save As → whichever you want |

Conversions carry everything, history included. What you get is a **copy** in the
new format: CaveWhere leaves the original alone and you carry on in the new one.
Converting renames nothing; see
[Save a copy of a project](save-a-project.md#save-a-copy-of-a-project).

## Next steps

- [Save a Project](save-a-project.md): what Save actually does, given that your
  work already sits on disk.
- [Open a Project](open-a-project.md): opening either format, and the recent
  projects list.
- [CaveWhere File Format Specification](../../cavewhere-format-spec.md): the
  official, implementer-level spec for both formats. Outside the manual,
  deliberately so: you need it to write a tool, never to use CaveWhere.
