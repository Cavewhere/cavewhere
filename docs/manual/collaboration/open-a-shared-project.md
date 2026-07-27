---
title: Open a Shared Project
summary: Download a cave someone shared. Paste their link with Open from Link, or browse your own GitHub repositories with Open from Online, and clone it to your disk.
problem: A teammate shared a cave with you, or you want your own online project on another machine, and you need to download it.
keywords: [open from link, open from online, clone, download, shared project, remote, repository, destination, ctrl+shift+o, private repo]
related: [../getting-started/open-the-example-cave.md, ../projects-and-files/open-a-project.md, sign-in-to-github.md, share-a-project.md]
---

# Open a Shared Project

## Why / when you need this

The other side of [sharing](share-a-project.md) is receiving: a partner sends a
link to the cave, or a project you host on GitHub needs to appear on a second
computer. Either way you **clone** it, downloading a full copy with its history
onto your own disk. After that it behaves like any local project you can edit and
[sync](sync-your-changes.md).

## Open from a link someone sent you

Use **File → Open from Link…** and paste under *"Paste a CaveWhere share link or
repository URL:"*. CaveWhere accepts a share link,
`https://cavewhere.com/open?repo=…`; the `cavewhere://open?repo=…` form a browser
hands over on macOS and Windows; or a clone URL such as
`https://github.com/user/cave.git`, `git@github.com:user/cave.git`, or bare
`github.com/user/cave`.

Both share-link forms enforce a 3-host allowlist: the `repo=` address must be
`https` on `github.com`, `gitlab.com`, or `bitbucket.org`. A raw clone URL skips
that check, so a club server's repository still clones when you paste its address
directly.

On macOS and Windows you don't usually open this dialog yourself, since clicking
the link in a browser hands it straight to CaveWhere. Nothing on Linux claims the
`cavewhere://` scheme, so Open from Link is how you feed the link in by hand. The
[example cave](../getting-started/open-the-example-cave.md) is one of these links.

## Open from your own repositories

For a repository of *your own*, **File → Open from Online…** (`Ctrl+Shift+O`)
opens the Remote page shown below, headed **Connect to a Remote Caving Area**.
[Sign in](sign-in-to-github.md) and it lists your repositories alphabetically,
each tagged **Public** or **Private**, under a box that filters by name. Click
one: CaveWhere drops its clone URL into the **Clone from HTTP/SSH** field at the
top and pulses the **Clone** button, gray until that field holds text, and again
while a clone runs. When the cave is yours I recommend this over hunting down a
link, because CaveWhere fills the address in.

![The Remote page, titled "Connect to a Remote Caving Area", with an empty Clone from HTTP/SSH field, its Destination path, and the Account picker.](../images/collaboration-sign-in.png)
*Before sign-in: no repository list yet, and the Clone button gray.*

## The Clone Repository dialog

A link puts up the **Clone Repository** dialog; Open from Online skips it and
clones in place. Destination and Account work the same on both:

- The dialog names the repository as host / path, like
  `github.com / your-org/your-cave`.
- **Destination** is where the download lands: your Documents folder by default,
  with the repository name appended. Click the path to pick another.
- The **Account** picker only matters for a **private** repository: choose the
  [GitHub account](sign-in-to-github.md) that has access. Public repositories
  clone with no sign-in, and an SSH address hides the picker, since SSH
  authenticates with the key on your machine instead of a GitHub token.
- **Clone & Open** starts it, tracked by a progress bar. Cancel grays out once
  cloning begins, and neither Escape nor a click outside closes it.

### If it's private and you don't have access

Without a signed-in account CaveWhere clones with no credentials, so a private
repository fails the first attempt. Of the 4 failure kinds it recognizes, only one recovers
by itself: an authentication error raises the sign-in panel, then retries the
clone the instant a token arrives. A 404 stops and waits for you, saying

> This repository doesn't exist, or your GitHub account doesn't have access to it.
> If you were invited as a collaborator, check that you've accepted the pending
> invitation.

alongside a **Check pending invitations on GitHub** link. Accept it there, then
clone again. On any failure CaveWhere deletes the half-downloaded folder.

## What happens to the project you had open

Because CaveWhere [holds one project at a time](../projects-and-files/open-a-project.md),
it asks what to do with the current cave exactly as **Open** does. That prompt
lands *after* the download, so the new copy already sits on disk while you decide
about the old one.

A cloned cave arrives editable, its [remote](share-a-project.md) already set and
ready to [sync](sync-your-changes.md) from the first edit. It also joins the
[recent projects list](../projects-and-files/open-a-project.md#reopen-a-recent-project).

## Next steps

- [Sync Your Changes](sync-your-changes.md): send your edits up and pull the
  team's down.
- [Review Project History](review-history.md): see what everyone has changed.
- [Open a Project](../projects-and-files/open-a-project.md): opening local caves,
  and the recent projects list.
