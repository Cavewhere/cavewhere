---
title: Sync Your Changes
summary: The Sync button — where it is, what its badge and tooltip mean, and what one click does (save, commit, pull with a data-model merge, push). Plus Save & Sync when you close, version warnings, and what happens on a conflict.
problem: Push the work you've saved up to the shared copy and bring your teammates' work down, without touching Git directly.
keywords: [sync, sync button, push, pull, merge, badge, ahead, behind, save and sync, remote settings, conflict, version compatibility, reconnect]
related: [how-sync-works.md, sign-in-to-github.md, ../projects-and-files/save-a-project.md, review-history.md, share-a-project.md]
---

# Sync Your Changes

## Why / when you need this

Saving records your work locally; **syncing** is what moves it between you and the
rest of the team. You sync when you want your saved changes to reach the shared
copy, when you want your teammates' latest changes, or — usually — both at once.
It's a single button, and it only appears once a project
[has a remote](share-a-project.md).

## Where the Sync button is

The **Sync button** sits at the far right of the breadcrumb bar, at the top of the
window, past the back/forward arrows and the page address. Its normal icon is a
pair of circular arrows. Hover over it any time and its **tooltip tells you exactly
what a click will do right now** — it's the quickest way to know where you stand
without reading anything else.

When a project has no remote yet, the button shows an upload-cloud icon instead and
its click starts [setting one up](share-a-project.md#step-1-give-the-project-a-remote).

![The CaveWhere window with the Sync button highlighted at the far right of the top bar, past the back/forward arrows and the page address, showing an upload-cloud icon.](../images/collaboration-set-up-remote.png)
*The Sync button lives at the far right of the top bar. Here the project has no
remote yet, so it shows the upload-cloud icon.*

## What one click does

Clicking Sync runs the whole exchange in order, so you never do the steps by hand:

1. **Saves and commits** your current work locally — so even if you had unsaved
   edits, they're recorded first.
2. **Pulls** your teammates' changes down from the remote and **merges** them into
   your project with the [data-model merge](how-sync-works.md#the-merge-understands-caves-not-just-text)
   that understands caves, trips, shots, and notes.
3. **Pushes** your commits up to the remote.

Because it always commits before it pulls or pushes, your work is safely recorded
even if the network step fails partway — nothing is left in limbo.

## Reading the badge

A small badge on the corner of the button summarizes your standing at a glance:

- **↑ N** — you have N versions saved locally that haven't been pushed yet.
- **↓ N** — there are N versions on the remote you haven't pulled yet.
- **•** (a dot) — you have local edits not yet committed.
- **!** in yellow — the status is **unavailable**: CaveWhere couldn't reach the
  remote or check where you stand (often just offline, or a sign-in that needs
  refreshing).
- **…** — a sync is in progress.

So `↑2 ↓1` means two of your versions to send up and one of theirs to bring down.
No badge at all means you're up to date.

## What the tooltip is telling you

The tooltip spells the same thing out in words, and names the action:

| You'll see | It means |
|------------|----------|
| **Up to date** | Nothing to send or receive. |
| **Updates available — click to sync** | Teammates have pushed changes you don't have yet. |
| **Commits ready to push — click to sync** | You have saved versions the remote doesn't have. |
| **Commits to push and pull — click to sync** | Both of the above. |
| **Unsaved changes — click to save and sync** | You have edits not yet saved; a click saves them and syncs. |
| **Local edits pending — click to sync** | Committed work waiting to go up. |
| **Sign in to GitHub — click to sync** | You need to [sign in](sign-in-to-github.md) first. |
| **GitHub access expired — click to reconnect** | Your sign-in lapsed; see below. |

## Save, or Save & Sync, when you close

Syncing is manual — CaveWhere never pushes behind your back — but it does offer to
sync at the natural moment you're stepping away. When you quit or switch projects
with unsaved work, the save prompt includes an extra button if the project has a
remote with something to push:

- **Save** records the version locally, as always.
- **Save & Sync** saves *and* pushes it up in one step, so you don't leave the
  team a version behind.

If the sync part fails there — say your access expired — CaveWhere tells you plainly
(*"GitHub access has expired. Your changes are saved locally."*) and lets you close
anyway or stay to fix it. Your work is saved either way; only the upload waited.

## When a pulled version needs a newer CaveWhere

CaveWhere's project format evolves, so occasionally a teammate on a newer release
saves something your version can't fully represent. Sync notices this and **warns
you rather than dropping data**. If a project was created by a newer CaveWhere than
yours, the Sync button is disabled and its tooltip reads *"Sync disabled — upgrade
CaveWhere to v…"*, naming the version you need. Updating CaveWhere clears it. This
is the same protection as the read-only banner described under
[Save a Project](../projects-and-files/save-a-project.md).

## How merge conflicts are handled

The reason two people can work on one cave at once is that syncing **combines**
everyone's edits for you. There are no conflict markers to untangle and no
"resolve conflicts" screen to sit through — because CaveWhere settles merges
itself, on two levels:

- **Edits to different things always merge.** If you added a trip while a teammate
  fixed a shot in another trip, edited a note while they moved a station, or worked
  in a different cave entirely, every change survives and lands where it belongs.
  This is the normal case — teams naturally divide the cave up — and it asks nothing
  of you.
- **Edits to the *same* value are settled in your favor.** If you and a teammate
  both changed the *exact same field* — one shot's distance, say — CaveWhere can't
  apply both, so it keeps the copy on the computer doing the sync: yours. Their
  version of that one value isn't applied to the current map.

Their change isn't gone, though — it's still recorded as their own save in the
[project history](review-history.md), where you can see exactly what they entered,
so if theirs was the right value you can put it back in yourself. And because this
only bites when two people edit the *same reading at the same time*, a quick word
about who's working where avoids it almost entirely.

You will rarely, if ever, see a raw conflict message — settling merges is CaveWhere's
job, not yours. In the unusual event a sync reports *"Merge Conflicts need to be
resolved,"* it appears as a yellow warning among CaveWhere's messages, your committed
work is untouched (sync commits it first), and the fix is simply to **Sync again**.
If you'd rather return to a clean starting point first,
[Discard](review-history.md#reverting-changes) takes you back to your last save.

## The Sync button's menu

Right-click the button for the actions around syncing:

- **Sync now** — the same as a left-click.
- **Set up remote…** — when the project has none yet.
- **Install CaveWhere on GitHub** — if your account still needs the
  [app installed](sign-in-to-github.md#install-the-cavewhere-app-on-github).
- **Remote settings…** — the **Remote Management** page, where you can see the
  remote and, if you need to, remove it. Removing a remote *"does not delete the
  remote repository — it only removes the link from this project."*
- **History…** — the [project history](review-history.md).
- **Log in** / **Log out** — for the GitHub account behind this remote.

## If your access expires

If a sync fails because your GitHub sign-in lapsed, CaveWhere raises a **GitHub
Access Expired** prompt — *"Your GitHub session has expired. Reconnect to continue
syncing."* Click **Reconnect** and repeat the short
[device-code step](sign-in-to-github.md#signing-in-the-device-code). Nothing is
lost; the sync just waited for you to reconnect.

## Next steps

- [Review Project History](review-history.md) — every version you and your team
  have synced, and how to roll back.
- [Open a Shared Project](open-a-shared-project.md) — download a cave a teammate
  shared.
- [Save a Project](../projects-and-files/save-a-project.md) — the difference
  between Save and Sync, in full.
