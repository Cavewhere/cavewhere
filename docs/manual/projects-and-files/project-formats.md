---
title: Choose a Project Format
summary: The difference between a .cwproj directory and a bundled .cw file, which one to pick, how to convert, and what CaveWhere does with an old .cw.
problem: Pick the project format that suits how you work, and understand what happened to the .cw file you have had for years.
keywords: [format, cwproj, cw, bundle, directory, zip, sqlite, legacy, convert, save as, git, version history]
related: [save-a-project.md, open-a-project.md, ../concepts/why-cavewhere.md]
---

# Choose a Project Format

## Why / when you need this

The first time you save, CaveWhere asks you to choose between a **directory** and
a **bundle**, and the choice isn't cosmetic. It decides *when* your work reaches
the file you named, how long a save takes, and what you can do with the result
afterwards.

Both formats hold the same data and the same complete history, and you can convert
between them at any time. So choose by the **size and life** of the project, not by
what's in it — and change your mind later if it grows.

## The short answer

- **A small, one-off cave** — one cave, a trip or two, something you'll finish and
  file away: **Bundle (`.cw`)**. One file to name, back up, and find again.
- **A big project** — several caves, years of trips, a team syncing: **Directory
  (`.cwproj`)**.
- **Handing it to someone, whatever it is**: **Bundle (`.cw`)**, made with
  [Save As](save-a-project.md#save-a-copy-of-a-project) so your working copy stays
  as it is.

**The line falls there because a bundle's cost scales with the whole project.**
Every save recompresses all of it, not just the part you touched. On one cave and a
few notes that's instant and you'd never know; on a multi-cave project carrying
years of scans, you'd feel it on every save. A directory only writes what changed,
so it doesn't care how big the project gets — which is why the bigger and
longer-lived a project is, the more obviously the directory wins.

Changed your mind? **Save As** and pick the other one. Nothing is lost either way,
history included.

## Directory (`.cwproj`)

A folder, on your disk, that CaveWhere writes into continuously.

This is the default, and it's the one for a project with a future — the big
multi-cave system you'll still be adding to in five years, or anything a team
syncs. Its advantage is the one that matters most: **your edits are written to disk
as you make them.** You don't have to remember to save to keep your data; you save
to mark a version (see [Save a Project](save-a-project.md#what-save-actually-does)).

Saves stay fast however large the project grows, because only what changed gets
written. And because the data sits in ordinary files in an ordinary folder, you can
open a note image, back the folder up, or point another tool at it without
CaveWhere's help — which matters more, not less, as a project accumulates years of
other people's work.

## Bundle (`.cw`)

One file, holding a compressed copy of everything a directory project would
contain.

It's the right answer for a **small, one-off cave** — the pit you surveyed in a
weekend and won't be back to — and for handing any project to someone or archiving a
finished one. One file to attach, copy, or file away, with nothing to accidentally
leave behind and no folder of parts to keep together. For a cave that size the
format's costs, below, are too small to notice.

**There is one trade worth understanding.** When you open a bundle, CaveWhere
unpacks it into a temporary folder and works there. Your edits are auto-saved to
*that* folder rather than straight into the `.cw`, and the file itself is brought
up to date when you **Save**, which repacks the whole project. So with a bundle,
Save is the step that puts your work back in the file — which is why CaveWhere
asks for it before you close one.

Saves are also slower, and get slower as the project grows: every one recompresses
the whole thing, not just the part you touched. That's what makes a bundle the
wrong home for a project that keeps expanding — and a perfectly good one for a cave
that's finished.

## The comparison, as CaveWhere states it

The **?** button next to the format picker opens the app's own summary:

| | Directory (`.cwproj`) | Bundle (`.cw`) |
|---|---|---|
| **Best for** | Active, multi-trip, or team projects | Sharing or archiving |
| Continuous auto-save | ✓ | — |
| Single portable file | — | ✓ |
| Speed | Fast | Slower (compresses on save) |
| Direct file access | ✓ | Requires extracting first |
| Version history | ✓ | ✓ |
| Remote sync | ✓ | ✓ |

The last two rows are worth dwelling on, because they are the rows people expect
to differ and they don't. **A bundle is a zipped-up directory project**, and the
zip includes the history, so a bundle keeps every version and can still sync with
a remote. Emailing someone a `.cw` hands them the whole story of the cave, not a
flattened snapshot of today.

## What's inside a `.cwproj`

Nothing you need day to day — but it's worth knowing that you *can* look, because
that's the point of the format. Data this expensive shouldn't be locked inside an
app.

> **Writing a tool that reads or writes CaveWhere projects?** Don't work from this
> section — it's a sketch for orientation. The official description is the
> **[CaveWhere File Format Specification](../../cavewhere-format-spec.md)**
> (`docs/cavewhere-format-spec.md`), a technical document aimed at implementers
> rather than surveyors: the protobuf schema for every entity, the name and path
> rules, the version history, and how bundling, saving, and Git/LFS work.

```
Jaws of the Beast/              the folder you chose (also a Git repository)
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
└── .git/                       the version history
```

Every file CaveWhere writes here except the images and point clouds is
**JSON**, indented and readable in any text editor. The `.cwproj` file itself is
a short descriptor:

```json
{
 "fileVersion": { "version": 9, "cavewhereVersion": "2026.4.3-409-gda9d7fe1" },
 "name": "Jaws of the Beast",
 "metadata": { "dataRoot": "Jaws of the Beast", "syncEnabled": true }
}
```

`version` is the **format** version — 9 is current, and it's what the
[Read-only banner](save-a-project.md#read-only-projects) is reading when it tells
you a project needs a newer CaveWhere. `cavewhereVersion` just records the build
that last wrote the file.

Note images and point clouds are stored as the **original files**, untouched and
under their own names. If CaveWhere vanished tomorrow, your scans would still be
sitting in `notes/`.

Two details that matter if you go poking around:

- **The outer folder is not the `.cwproj`.** The folder has no extension; the
  `.cwproj` is a file *inside* it. To move a project, move the whole folder.
- **Renaming the project renames folders.** The data folder is named after the
  project, so editing the project name (see
  [Save a Project](save-a-project.md#name-the-project)) physically renames the
  data folder and the `.cwproj` file. The outer folder keeps the name you gave it
  at save time.

## Legacy `.cw` files (v6 and older)

Old CaveWhere projects are a single `.cw` file too, but a completely different
thing inside: a **SQLite database** rather than a zip. CaveWhere still reads
them, and you don't have to do anything special — **opening one converts it
automatically**, and you land in a normal, editable project.

The part to know about is what happens on the **first save**, because CaveWhere
doesn't announce it:

> **Your old `.cw` keeps its name and becomes a bundle.** Saving writes a modern
> bundled `.cw` over the file you opened. Your survey comes through whole — this
> is a change of container, not of contents — but the file is no longer a SQLite
> database, so an **older** CaveWhere won't open it afterwards. It's a one-way
> upgrade. If you still need the original readable by an old version, keep a copy
> of it before you save.

Once converted, it's an ordinary bundle, so everything above applies — including
being able to **Save As** to a directory if you'd rather work that way.

Two footnotes:

- Because the file becomes a bundle, the Save As dialog **pre-selects Bundle**
  for a converted project, keeping the single-file shape you already had. Choose
  Directory if you want the other one.
- If the old `.cw` is **read-only** — off a CD, out of an archive, or with its
  permissions locked down — CaveWhere can't write back over it, so it treats the
  converted project as [temporary](save-a-project.md#your-project-already-exists)
  and sends you to Save As instead. Nothing is written over the original.

## Convert between formats

There is no Export. **Save As is the converter**, and the format follows the
choice you make in the dialog:

| From | To | Do this |
|---|---|---|
| Directory | Bundle | Save As → **Bundle (`.cw`)** |
| Bundle | Directory | Save As → **Directory (`.cwproj`)** |
| Legacy `.cw` | Either | Open it, then Save As → whichever you want |

Conversions carry everything, history included. What you get is a **copy** in the
new format: the original is left alone and you carry on working in the new one.
Note that converting doesn't rename anything — see
[Save a copy of a project](save-a-project.md#save-a-copy-of-a-project).

## Next steps

- [Save a Project](save-a-project.md) — what Save actually does, given that your
  work is already on disk.
- [Open a Project](open-a-project.md) — opening either format, and the recent
  projects list.
- [CaveWhere File Format Specification](../../cavewhere-format-spec.md) — the
  official, implementer-level spec for both formats. Outside the manual, and
  deliberately so: you need it to write a tool, never to use CaveWhere.
