---
title: Install CaveWhere
summary: Where to download CaveWhere for macOS, Windows and Linux, what each platform gives you, and why double-clicking a project file works on macOS and Windows but not on Linux.
problem: Get CaveWhere onto your machine, and open the project files on your disk with it.
keywords: [download, install, macos, windows, linux, dmg, appimage, installer, file association, cw, cwproj, build from source]
related: [set-up-your-identity.md, find-your-way-around.md]
---

# Install CaveWhere

## Why / when you need this

CaveWhere is a desktop application that runs on your own machine, not a website
you log into. Your caves live on your disk, and everything works with no network
connection — syncing with a team is something you opt into later, not a
precondition for using the app.

## Download CaveWhere

Downloads for every platform are at **<https://cavewhere.com/downloads/>**.

CaveWhere is free and open source, so there is nothing to buy and no licence key
to enter. The first thing it asks for is your name and email, and that is for
[signing your own work](set-up-your-identity.md) — not an account to register.

## What you get on each platform

| Platform | What you download | How you install it |
|----------|-------------------|--------------------|
| **macOS** | A `.dmg` disk image | Open it and drag CaveWhere to Applications. The app is signed and notarized by Apple, so it opens without a security warning. |
| **Windows** | An installer | Run it and follow the prompts. |
| **Linux** | An AppImage | Mark it executable and run it — there is nothing to install and no root needed. Nothing to uninstall either: delete the file. |

## Your project files become double-clickable — on macOS and Windows

**On macOS and Windows**, installing registers `.cw` and `.cwproj` as CaveWhere
files, so a project on your disk opens in CaveWhere when you double-click it.

That matters more than it sounds, because CaveWhere holds
[one project at a time](../projects-and-files/open-a-project.md) and a directory
project is a folder rather than a single file. Double-clicking the `.cwproj`
*inside* that folder is the quickest way in, and it saves you hunting through the
Open dialog for a project you can already see in your file browser.

Those two platforms also register the `cavewhere://` link scheme, so a shared
project link opens the app straight from your browser. Sharing belongs to
CaveWhere's collaboration features, covered in the
[Collaboration](../collaboration/how-sync-works.md) chapter.

**On Linux, neither of these applies.** The AppImage is a single file you run
rather than something that installs itself, so nothing tells your desktop that
`.cw` and `.cwproj` belong to CaveWhere — and there's no one way to register a
file type that would work across the desktop environments Linux users actually
run.

Nothing is out of reach because of it; you just start from inside the app rather
than from your file browser:

- **A cave on your disk** — **File → Open…**, or the
  [recent projects list](../projects-and-files/open-a-project.md#reopen-a-recent-project),
  which is one click from anywhere and remembers everything you've worked on.
- **A link someone shared with you** — **File → Open from Link…**, and paste it
  in. The link does the same thing it does everywhere else; on Linux you hand it
  to CaveWhere yourself instead of your browser doing it for you.

## Build from source

If you would rather build it yourself — to run on a platform without a package,
or to work on CaveWhere itself — the repository's `README.md` has the build
instructions, and the GitHub Actions workflow files are the most up-to-date
reference for exactly how the official builds are produced.

Building from source is worth knowing about for one user-visible reason: a
source build can be missing optional pieces the official packages always include.
The clearest case is PDF support for notes, which needs Qt's PDF module at build
time. **Settings → PDF / SVG** reports whether your build has it. If you
downloaded CaveWhere from the site, it does.

## Next steps

- [Set Up Your Identity](set-up-your-identity.md) — the one thing CaveWhere asks
  for before it opens.
- [Find Your Way Around](find-your-way-around.md) — a tour of the window.
