---
title: Install CaveWhere
summary: Where to download CaveWhere for macOS, Windows and Linux, what each platform gives you, and why double-clicking a project file works on macOS and Windows but not on Linux.
problem: Get CaveWhere onto your machine, and open the project files on your disk with it.
keywords: [download, install, macos, windows, linux, dmg, appimage, installer, file association, cw, cwproj, build from source]
related: [set-up-your-identity.md, find-your-way-around.md]
---

# Install CaveWhere

## Why / when you need this

CaveWhere runs on your own machine as a desktop application, not as a website you
log into. Your caves live on your disk, and everything works with no network
connection. Syncing with a team stays opt-in, never a precondition for using the
app.

## Download CaveWhere

Downloads for every platform live at **<https://cavewhere.com/downloads/>**.

CaveWhere costs nothing and ships as open source, so you buy nothing and enter no
license key. It asks for your name and email first, purely for
[signing your own work](set-up-your-identity.md), not to register an account.

Free doesn't mean the work is free to make. If CaveWhere is useful to you, you
can support its development at **<https://www.patreon.com/cavewhere>**.

## What you get on each platform

| Platform | What you download | How you install it |
|----------|-------------------|--------------------|
| **macOS** | A `.dmg` disk image | Open it and drag CaveWhere to Applications. |
| **Windows** | An installer | Run it and follow the prompts. |
| **Linux** | An AppImage, built for x86_64 and arm64 | Mark it executable and run it. Nothing installs, nothing needs root, and uninstalling means deleting the file. |

## Your project files become double-clickable on macOS and Windows

**On macOS and Windows**, installing registers `.cw` and `.cwproj` as CaveWhere
files, so double-clicking a project on your disk opens it in CaveWhere.

That matters more than it sounds. CaveWhere holds
[one project at a time](../projects-and-files/open-a-project.md), and a directory
project takes the form of a folder instead of a single file. Double-clicking the
`.cwproj` *inside* that folder gets you in fastest, and it saves you hunting
through the Open dialog for a project you can already see in your file browser.

Those two platforms also register the `cavewhere://` link scheme, so a shared
project link opens the app straight from your browser. Sharing belongs to
CaveWhere's collaboration features, covered in the
[Collaboration](../collaboration/how-sync-works.md) chapter.

**On Linux, neither applies.** You run the AppImage as a single file rather than
installing it, so nothing tells your desktop that `.cw` and `.cwproj` belong to
CaveWhere. Unfortunately no single way to register a file type works across the
desktop environments Linux users actually run.

Nothing goes out of reach because of it. You just start from inside the app
instead of from your file browser:

- **A cave on your disk.** **File → Open…**, or the
  [recent projects list](../projects-and-files/open-a-project.md#reopen-a-recent-project),
  one click from anywhere, remembering everything you have worked on.
- **A link someone shared with you.** **File → Open from Link…**, then paste it
  in. The link behaves the same everywhere; on Linux you hand it to CaveWhere
  yourself instead of letting your browser do it.

## Build from source

To build CaveWhere yourself, whether to run it on a platform without a package or
to work on the app itself, start from the repository's `README.md`. The build
needs CMake 3.23 or newer and Qt 6.11; the official Linux and Windows builds pin
6.11.1. For exactly how those get produced, read
`.github/workflows/build-linux.yml` and `build-windows.yml`. macOS packaging runs
outside CI, in `installer/mac/installMac.sh`.

One user-visible reason to know about source builds: a source build may lack
optional pieces the official packages always include. PDF support for notes makes
the clearest case, because it needs Qt's PDF module at build time.
**Settings → PDF / SVG** reports whether your build has it. A download from the
site always does.

I recommend the packaged download unless you specifically need to change
CaveWhere. A source build pulls the full submodule tree and a Conan dependency
set, and it takes considerably longer than dragging an app to Applications.

## Next steps

- [Set Up Your Identity](set-up-your-identity.md): the one thing CaveWhere asks
  for before it opens.
- [Find Your Way Around](find-your-way-around.md): a tour of the window.
