---
title: How Collaboration Works
summary: The idea behind CaveWhere's sync: Git without having to know Git, one shared copy on GitHub reached through a remote named origin, and a three-way merge that matches objects by id rather than settling them as text.
problem: Understand how a team can all work on the same cave without emailing files around or overwriting each other's work.
keywords: [collaboration, sync, git, github, remote, merge, team, share, version, how it works]
related: [../concepts/why-cavewhere.md, ../projects-and-files/save-a-project.md, ../projects-and-files/project-formats.md, sign-in-to-github.md, sync-your-changes.md]
---

# How Collaboration Works

## Why / when you need this

A cave survey is a team effort, but sharing the project has always been the
awkward part. Email the file around and someone edits a stale copy; put it on a
shared drive and two people's saves overwrite each other. CaveWhere's answer is
**sync**: everyone works on their own copy, and CaveWhere merges the changes
together so the whole team stays on one current map.

Sync stays entirely opt-in: CaveWhere needs neither a GitHub account nor a
network connection until you turn it on. Work alone and you can ignore this
chapter.

## Git, without having to know Git

CaveWhere syncs with **Git**, the same version-control system programmers use,
and hosts the shared copy on **GitHub**. Why Git? Two reasons. You get a
complete history of every version, and you get a merge good enough that two
people can edit the same cave at once.

Git aims at programmers, though: branches, commits, merge conflicts, a command
line. CaveWhere puts a cave-surveyor's front end on it. Sign in to GitHub once,
then press a single **Sync** button. No branches to manage, no commands to type.

CaveWhere links **libgit2 1.9.1** into the app, so nothing needs the `git`
program installed, and the `.git` folder it writes is ordinary. A project starts
on a branch named `main`, and sharing adds a remote named `origin`. If you do
know Git, `git log` in that folder reads the same history CaveWhere reads.

## The shared copy is called a "remote"

When you share a project, CaveWhere creates a copy of it on GitHub. That copy is
the **remote**, the common version the whole team syncs against. Each person
keeps a **local** copy on their own disk, and that is the one they edit.

Working with a remote is a loop:

- Edit your local copy and [save](../projects-and-files/save-a-project.md) as
  you go, exactly as you would working alone.
- **Sync** sends your saved changes up to the remote and brings down anyone
  else's.
- Your teammates sync too, so every local copy catches up with the rest.

A project has at most one remote, and syncing only moves changes between your
local copy and that shared copy, never directly between 2 people. GitHub in the
middle lets people never online at the same time still collaborate.

## Save marks a version; Sync shares it

Two words that sound similar do different jobs:

- **[Save](../projects-and-files/save-a-project.md)** records a version in your
  local history, a point you can return to. It does **not** touch the remote.
  (CaveWhere writes your edits to disk as you type, so Save is not about
  flushing data; it marks a version.)
- **Sync** takes the versions you saved, pushes them to the remote, and pulls
  down the versions your teammates saved.

So save often; sync when you want your work to reach the team, or theirs to
reach you. You can save a dozen times between syncs, and one sync carries all of
those versions up together.

## The merge understands caves, not just text

Two people can edit one cave at once because text is not where the merge stops.
Git rebases the files, then CaveWhere reads them back through a **three-way
merge over its own data model**. The 3 ways are the common ancestor you both
started from, your copy, and theirs. The
model covers 9 kinds of object, each with its own merge plan: the region, caves,
trips, trip calibrations, survey chunks, notes, scraps, LiDAR notes, and teams.

Objects match on **id**. Rename a cave on one machine while a teammate edits it
and both changes still land on the same cave. From there the merge goes field by
field. Where your value still equals the ancestor and theirs moved, theirs wins;
otherwise yours stands and theirs fails to land, with nothing said about it. Add
a trip while a teammate fixes a shot in another trip and every edit survives.
Sometimes one side created the field and no ancestor exists to compare against;
that field takes the incoming value outright.

I recommend the [directory (`.cwproj`)
format](../projects-and-files/project-formats.md) for anything a team touches.
It spreads survey data over many small files of indented JSON, 6 extensions'
worth, giving the merge clean seams to work along. Note scans, PDFs, and point
clouds get no such treatment: 24 file extensions ride in Git LFS and
[arrive on demand](sync-your-changes.md#files-that-have-not-downloaded-yet), so
a fresh clone may open without them. A bundled `.cw` syncs too and carries the
same history; its drawback is the zip, which every save repacks whole.

## What the rest of this chapter covers

- **[Sign In to GitHub](sign-in-to-github.md)**: connect CaveWhere to GitHub,
  once.
- **[Share a Project](share-a-project.md)**: put a local project on GitHub and
  send your team a link.
- **[Sync Your Changes](sync-your-changes.md)**: the Sync button, its badge, the
  5-minute remote recheck, and the 3-retry limit.
- **[Open a Shared Project](open-a-shared-project.md)**: download a cave a
  teammate shared.
- **[Review Project History](review-history.md)**: every version, what each one
  changed, and rolling back.

Why sync exists at all:
[Working as a team](../concepts/why-cavewhere.md#working-as-a-team-sync).
