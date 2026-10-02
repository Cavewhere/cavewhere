---
title: Share a Project
summary: Give a project a remote, either by letting CaveWhere create the GitHub repository for you (private by default) or by connecting one that exists. Then send a share link with Copy link, and see how that link opens on each platform.
problem: Get a cave you've been working on to your team, or back it up online, and give people a link that opens it.
keywords: [share, remote, publish, github, gitlab, bitbucket, create repository, private, public, share link, copy link, deep link, cavewhere.com/open, collaborators, set up remote]
related: [sign-in-to-github.md, sync-your-changes.md, open-a-shared-project.md, how-sync-works.md]
---

# Share a Project

## Why / when you need this

You've built a cave locally and now other people need it: a survey partner who
will add their trips, a club archive, or an online backup you can pull down on
another machine. Sharing takes 2 steps. Give the project a **remote** (its
shared copy online), then send people a **link** to it.

Both steps need a GitHub sign-in, so start at
[Sign In to GitHub](sign-in-to-github.md) if you have not connected one yet.

## Step 1: give the project a remote

A brand-new project is local only. Adding a remote gives it the copy everyone
syncs against (see
[How Collaboration Works](how-sync-works.md#the-shared-copy-is-called-a-remote)).
The **Set Up Remote** wizard opens from 4 places:

- the **Sync button** at the top right, which wears the upload-cloud icon until
  the project has a remote (ringed in orange, see below);
- **Set up remote…** on that button's right-click menu;
- **Remote settings…** → **Add Remote**, on that same menu;
- the **Share** dialog's *"Add a remote repository to enable sharing."* link.

![The CaveWhere window with the Sync button ringed near the right end of the top bar, showing an upload-cloud icon.](../images/collaboration-set-up-remote.png)
*Until the project has a remote, the Sync button shows an upload cloud, and
clicking it opens the wizard.*

A project has at most one remote, so this is a one-time step.

### Let CaveWhere create the repository

The wizard opens on **Create repository**, and that is the path I recommend:
CaveWhere makes the GitHub repository for you, so you never leave the app.

1. **Repository name**, prefilled from your project name with spaces turned into
   hyphens and anything outside `A-Z a-z 0-9 . _ -` dropped. Adjust it if you
   like.
2. **Visibility**, **Private** (the default) or **Public**:
   - *Private*: *"Only you and collaborators can see this repository.
     Recommended for cave survey data."* Cave locations are sensitive, so take
     the default unless you have a reason not to.
   - *Public*: *"Anyone on the internet can view this repository and its survey
     data."*
3. Click **Create repository**. CaveWhere checks your own repository list before
   it asks GitHub for anything. A name you already own comes back as *"Choose a
   different name or use 'Connect existing' to wire it as your remote."* The link
   that does that is labeled **Already have a remote? →**, not "Connect
   existing".

### Or connect a repository that already exists

Follow **Already have a remote? →** to the connect screen:

- **GitHub repository**: filter your repositories by name and pick one. A 🔒
  marks the private ones.
- **Custom URL**: paste any clone URL, `https://…` or SSH. For SSH, *"ensure
  your SSH key is already configured"*. CaveWhere uses your system's key there
  rather than a GitHub sign-in.

Click **Connect** to wire it up. Either way the wizard finishes on *"Remote
configured. Push your project to GitHub to back it up."* Click **Sync now** to
push for the first time (see [Sync Your Changes](sync-your-changes.md)), or
**Close** and sync later.

## Step 2: send a share link

**File → Share…** opens the **Share Project** dialog with the link already
built. Click **Copy link**, then paste it into an email, a chat message, a wiki:

```
https://cavewhere.com/open?repo=https://github.com/your-org/your-cave.git
```

Anyone with CaveWhere can open it, and
[Open a Shared Project](open-a-shared-project.md) covers their end. For a
**private** repository the dialog warns *"Recipients need repository access for
private repositories."* The link points at the repo, but GitHub still decides
who may clone it, so invite your collaborators first. The dialog's **Invite
collaborators on GitHub** link names whichever host you use, and opens that
repository's access page, `/settings/access` on GitHub.

Share links cover 3 hosts: github.com, gitlab.com and bitbucket.org. Point the
remote anywhere else and **Copy link** stays disabled, with the dialog saying the
remote *"cannot be used for share links. Push to a GitHub, GitLab, or Bitbucket
repository to enable sharing."*

## How the link opens on each platform

Clicking the link lands on cavewhere.com/open, which redirects to
`cavewhere://open?repo=…`:

- **macOS and Windows**: installing CaveWhere registers that scheme, so the
  system hands the link to the app.
- **Linux**: the AppImage registers nothing, so the handoff fails. After 3 s the
  page gives up and offers **Download CaveWhere** instead. Open the link by hand
  with **File → Open from Link…**.

Windows carries its own limitation: no single-instance routing, so a link
clicked while CaveWhere is already open starts a second copy rather than reusing
the window you have.

However the link reaches CaveWhere, it lands in the **Clone Repository** dialog,
covered in [Open a Shared Project](open-a-shared-project.md).

## Next steps

- [Sync Your Changes](sync-your-changes.md): push your work to the remote and
  pull your team's down.
- [Open a Shared Project](open-a-shared-project.md): the other end of the link
  you just sent.
- [Review Project History](review-history.md): every version, once the team
  starts syncing.
