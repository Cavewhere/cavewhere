---
title: How Collaboration Works
summary: The idea behind CaveWhere's sync — Git without having to know Git, one shared copy on GitHub, and a merge that understands caves and trips rather than lines of text. Read this before the how-to pages.
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

This page explains the idea. The rest of the chapter is the how-to — signing in,
sharing, syncing, and reading the history — and all of it makes more sense once
the model here is clear. Sync is entirely opt-in: if you work alone, you can
ignore this chapter and nothing in it affects you.

## Git, without having to know Git

Under the hood, CaveWhere syncs with **Git** — the same version-control system
programmers use — and hosts the shared copy on **GitHub**. That choice buys two
things worth having: a complete, trustworthy history of every version, and a
merge engine robust enough that two people really can edit the same cave at once.

The catch with Git has always been that it was built for programmers: branches,
commits, merge conflicts, a command line. CaveWhere puts a cave-surveyor's front
end on it. You sign in to GitHub once, then press a single **Sync** button. There
are no branches to manage and no commands to type — the Git plumbing stays out of
your way, while you still get the history and the safe merging it provides.

You do not need a GitHub account or a network connection to use CaveWhere. Sync
is something you turn on when you want to share a project or back it up online.

## The shared copy is called a "remote"

When you share a project, CaveWhere creates a copy of it on GitHub. That online
copy is the **remote** — the common version the whole team syncs against. Each
person also has their own **local** copy on their own disk, which is the one they
actually edit.

Working with a remote is a loop:

- You edit your local copy and [save](../projects-and-files/save-a-project.md) as
  you go, exactly as you would working alone.
- When you **sync**, CaveWhere sends your saved changes up to the remote and
  brings down anyone else's.
- Your teammates sync too, so everyone's local copy catches up with everyone
  else's work.

A project has at most one remote, and syncing only ever moves changes between
your local copy and that one shared copy — never directly between two people.
GitHub in the middle is what lets people who are never online at the same time
still collaborate.

## Save marks a version; Sync shares it

Two words that sound similar do different jobs, and the difference is the key to
the whole chapter:

- **[Save](../projects-and-files/save-a-project.md)** records a version in your
  local project's history — a point you can return to. It does **not** touch the
  remote. (Remember that CaveWhere writes your edits to disk as you type, so Save
  isn't about flushing data; it's about marking a version.)
- **Sync** takes the versions you've saved, pushes them to the remote, and pulls
  down the versions your teammates have saved.

So you save often, as a matter of course, and sync when you want your work to
reach the team — or theirs to reach you. You can save a dozen times between syncs;
each is a version in the history, and one sync carries them all up together.

## The merge understands caves, not just text

The reason two people can edit the same cave at once is that CaveWhere doesn't
merge the project as raw text. When it pulls your teammates' changes, it combines
them with a **three-way merge that understands its own data model** — caves,
trips, survey shots, notes, scraps, teams, LiDAR. If you added a trip to a cave
while a teammate fixed a shot in a different trip of the same cave, both edits
survive and land in the right places. You don't resolve anything by hand.

This is why the [directory (`.cwproj`) format](../projects-and-files/project-formats.md)
stores survey data as many small, human-readable files: it gives the merge clean
seams to work along. A bundled `.cw` can sync too — it carries the same history —
but a big collaborative project is happiest as a directory.

## What the rest of this chapter covers

- **[Sign In to GitHub](sign-in-to-github.md)** — connect CaveWhere to GitHub,
  once, so it can sync on your behalf.
- **[Share a Project](share-a-project.md)** — put a local project on GitHub and
  send your team a link to it.
- **[Sync Your Changes](sync-your-changes.md)** — the Sync button, what it does,
  and what its badge is telling you.
- **[Open a Shared Project](open-a-shared-project.md)** — download a cave a
  teammate shared with you.
- **[Review Project History](review-history.md)** — see every version, what each
  one changed, and roll back to an earlier one.

For the product-level version of why sync exists, see
[Working as a team](../concepts/why-cavewhere.md#working-as-a-team-sync) in Why
CaveWhere.
