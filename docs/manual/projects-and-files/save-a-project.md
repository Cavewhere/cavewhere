---
title: Save a Project
summary: What Save really does in CaveWhere, the temporary folder a new project starts in, the first save, saving a copy, and the prompt on the way out.
problem: Give a project a permanent home and a real name, and understand what Save is for when your work is already on disk.
keywords: [save, save as, ctrl+s, temporary project, autosave, version, commit, discard, quit, read-only, project name]
related: [project-formats.md, open-a-project.md, ../concepts/glossary.md]
---

# Save a Project

## Why / when you need this

CaveWhere saves differently from most apps, and the difference is worth two
minutes of your attention — because it works in your favour, and because it
changes what Save is for.

In most apps, what you type lives in memory until you save it — and memory doesn't
survive accidents. The app crashes. The battery dies. The generator at camp cuts
out. Any of those takes everything since your last save, because until you saved,
it existed nowhere but RAM. The usual defence is a habit: press `Ctrl+S` often
enough and an accident only costs you a little. That's a bet, and it's one you're
re-making every few minutes of an evening spent typing up a trip.

**CaveWhere is built so you never have to make that bet.** A survey costs far too
much to collect — days underground, often across years — to sit in memory waiting
for you to remember it. So your work goes to disk as you type it, from the moment
the app opens: the gap between entering a reading and it being written is measured
in moments, not in however long it's been since you last reached for `Ctrl+S`. And
CaveWhere stops to ask before letting you walk away from anything unsaved, rather
than deciding for you.

Save, then, isn't there to rescue your data. It does something more useful, and
the rest of this page is about what.

## Your project already exists

Open CaveWhere and you already have a project. Not a blank waiting to be filled
in — a **real one**, already on disk, already recording every change you make.

What it doesn't have yet is a home. Until you save it, it lives in a **temporary
folder** under a placeholder name CaveWhere picks at random — *Misty Cavern*,
*Thunder Ridge* — so there's something to call it in the meantime.

Saving gives it a permanent place and a real name. It's worth doing early, not
because the project is fragile but because everything afterwards is easier once it
sits where you'd look for it and is called what you'd call it — and because that's
the point where it starts keeping the history every later save adds to.

CaveWhere won't let the temporary state catch you out. Quit, or open something
else, with work in a temporary project and it stops to ask rather than deciding
for you, saying plainly where things stand: *"This project lives in a temporary
folder. Save to move it somewhere permanent."* A temporary project is only
discarded if you choose to discard it. (CaveWhere won't ask if you never put
anything in it — no caves, never saved — since there's nothing there to keep.)

## What Save actually does

Once a project is saved somewhere permanent, **Save** (`Ctrl+S`) stops being about
getting your data onto disk. Your data is already on disk — CaveWhere wrote it
there as you typed.

Save **marks a version**. It takes everything that has changed since the last one
and records it as a point in the project's history that you can come back to,
compare against, and roll back to. It's the survey equivalent of finishing a page
in the book and dating it.

Those versions are real, and you can go and look at them. **Right-click the sync
button** in the top-right corner and choose **History…**: every save is listed,
newest first, with who made it, when, and exactly which files it changed.
CaveWhere titles them all *"Save from CaveWhere"*, so what tells one from another
is its date and its list of changes.

![The History page. A list of three saves, each titled "Save from CaveWhere" by Philip Schuchardt, each with a short commit hash and the date 2026-07-15, joined by a line down the left; the newest carries a "main" badge and is selected. A panel on the right shows the selected save's author, date, full hash, and "1 changed file(s)" naming the one trip file that changed.](../images/project-git-history.png)
*Three saves, three points this project can be taken back to. The panel on the
right unpacks whichever one you select.*

Working through that history — reading a save, comparing two, restoring an old
one — is a chapter of its own, still to be written. What matters here is that it
exists, and that every Save you press adds to it.

That reframes two things people expect from other apps:

- **Forgetting to save costs you the checkpoint, not the work.** In a
  [directory project](project-formats.md#directory-cwproj) the edits are already
  written; what an unexpected shutdown takes with it is the *marker*, not the
  survey.
- **Discard means more than "don't save".** It's a rollback to your last save, so
  it also undoes work CaveWhere had already written for you. That's the real
  argument for saving at the points you'd want to come back to — it's what gives
  Discard somewhere sensible to land.

The one exception is a [bundle](project-formats.md#bundle-cw). A `.cw` is unpacked
into a temporary folder while you work, so its auto-saves land *there* and Save is
what repacks them into the file you named. With a bundle, Save is the step that
puts your work back in the file — and CaveWhere asks for it before you close.

## Save it for the first time

**Save** (`Ctrl+S`) or **Save As** both open the same dialog, because there's
nothing to save over yet.

![The Save CaveWhere Project As dialog. A Project Name field reads "Phake Cave 3000"; below it two radio buttons, Directory (.cwproj) selected and Bundle (.cw) not, with a ? button beside them; below that a Location field reading /Users/cave/Desktop with a Browse button, and under it the full path /Users/cave/Desktop/Phake Cave 3000/Phake Cave 3000.cwproj. Cancel and Save buttons sit at the bottom.](../images/project-save-as.png)
*The three decisions of a first save, in one dialog. The path underneath is worth
reading before you commit to it: the name you typed became the folder, and the
Location you chose is only its parent.*

### Name the project

The **Project Name** field is pre-filled with the random stand-in name. Replace it
with the name of the cave, or the area, or whatever you'd search for in two years.

The name is load-bearing: it names the folder your project goes into and the data
folder inside it, so it's worth typing rather than accepting. Characters a
filesystem won't take are cleaned up for you.

**This field only appears on the first save.** After that the project's name is set
by double-clicking it at the top of the **Data** page, not through this dialog —
see [Save a copy](#save-a-copy-of-a-project) for why that matters.

### Choose the format and where it goes

Pick **Directory (`.cwproj`)** or **Bundle (`.cw`)** —
[Choose a Project Format](project-formats.md) covers the decision, and the **?**
button beside the picker gives CaveWhere's own summary. Directory is the default
and the right answer for a cave you're still surveying.

Then set **Location**, by typing it or with **Browse…**. What you're choosing
differs by format, which is easy to misread:

- **Directory** — you pick the **parent folder**. CaveWhere makes a folder named
  after the project inside it, and shows you the full path underneath so you can
  check. You don't name the folder here; the project name did that.
- **Bundle** — you pick the **whole path**, filename and all. The `.cw` extension
  is added and de-duplicated for you, so you can't end up with `cave.cwproj.cw`.

Two messages can appear under the path, and they don't mean the same thing:

- **"A folder … already exists in this location"** — red, and it **blocks Save**.
  CaveWhere won't merge a project into an existing folder. Pick another name or
  another place.
- **"An existing bundle will be overwritten"** — orange, and it **allows Save**.
  You're about to replace that `.cw`. Intentional when you're re-saving; worth a
  second look when you're not.

## Save again later

After the first save, `Ctrl+S` just saves — no dialog, no questions. Each one adds
a version to the history.

There's no rule about how often. A reasonable habit is to save at the point you'd
want to come back to: after entering a trip, after finishing a scrap, before you
try something you're unsure about.

## Save a copy of a project

**Save As** on an already-saved project makes a **copy** and leaves the original
alone, exactly where it was and exactly as it was at its last save. You carry on
working in the **new** copy.

### It's also how you change format

The dialog's format picker applies to the copy, so **Save As is the way to turn a
directory project into a bundle, or a bundle back into a directory** — there's no
separate Export, and nothing is lost either way. Two moments when you'll want it:

- **A cave is finished.** You surveyed it as a `.cwproj` directory, you're not
  going back, and now you want one file to send to the survey archive or the
  landowner. Save As → **Bundle (`.cw`)**.
- **A `.cw` someone sent you turns into real work.** It came as a bundle, but
  you've started adding trips and it's growing. Save As → **Directory
  (`.cwproj`)** and work in that instead, so saves stop recompressing the whole
  cave every time.

[Choose a Project Format](project-formats.md) covers which way to jump, and
remember the copy is a copy: after switching you're working in the new one, and the
old file stays behind exactly as it was.

### It doesn't rename anything

The part that surprises people:

> **Save As does not rename your project.** It copies it. The copy is still called
> what the original was called, and CaveWhere still shows that name.

That's why the name field is missing from the dialog once a project has been
saved, and why a directory Save As only lets you choose the parent folder — the
folder it creates is named after the project, not after anything you type here.
Look inside a copy you made under a new folder name and you'll find the data folder
still carrying the old project name. Nothing is wrong; the copy simply kept its
identity.

**To actually rename a project**, double-click its name at the top of the **Data**
page. That renames the project everywhere, folders included.

The copy carries the full history, so a Save As is a genuine snapshot of the whole
project and not just of today.

## When you quit

CaveWhere asks before closing if there's anything unsaved, and the buttons depend
on where the project lives.

**A saved project** gets **Discard**, **Cancel**, and **Save**. Remember that
Discard rolls back to your last save rather than merely closing quietly.

**A project that has been set up to sync** gets a fourth button, **Save & Sync**,
which saves and then pushes to the remote before letting the app close — the right
one to reach for if someone else is waiting on your data. If the sync fails, you're
told your changes are still saved locally, and offered **Close anyway** or **Stay
open**. Nothing you did is at risk either way; only the push didn't happen.

**A temporary project** — one that has never been saved — gets **Delete**,
**Cancel**, and **Save** instead. There's no Discard, because there's no earlier
save to roll back to: the choice is to give the project a home or let it go.
**Delete** removes the temporary folder and the project in it — the button for
when you were only trying something out. If you're not sure, **Cancel** and decide
later; nothing is decided until you pick.

You won't be asked at all if nothing has changed since your last save, or if the
project is empty and never saved.

## Read-only projects

If a project was made by a **newer version of CaveWhere** than the one you're
running, a **Read-only** banner appears in the corner telling you which version you
need, and **Save** and **Save As** are both greyed out.

This is deliberate protection, not a limitation. A newer CaveWhere may have stored
things this one doesn't know how to represent, and saving would quietly drop them —
so it refuses rather than silently damaging a survey someone else is relying on.
You can look around all you like. To edit, upgrade to the version named in the
banner.

## Next steps

- [Choose a Project Format](project-formats.md) — directory versus bundle, and
  what happens to a legacy `.cw`.
- [Open a Project](open-a-project.md) — the Open dialog and the recent projects
  list.
