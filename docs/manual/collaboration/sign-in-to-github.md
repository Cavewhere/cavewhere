---
title: Sign In to GitHub
summary: Connect CaveWhere to GitHub with a one-time device-code sign-in (no password typed into CaveWhere, no SSH keys), install the CaveWhere GitHub app, manage more than one account, and sign out. Your token is kept in the OS keychain.
problem: Let CaveWhere sync on your behalf by connecting it to your GitHub account, once.
keywords: [github, sign in, login, account, device flow, oauth, keychain, credentials, install app, reconnect, log out, forget account]
related: [../getting-started/set-up-your-identity.md, how-sync-works.md, share-a-project.md, sync-your-changes.md]
---

# Sign In to GitHub

## Why / when you need this

To sync, CaveWhere has to talk to GitHub as you — to push your work up and pull
your team's down. Signing in is how you give it permission to do that, once, so
you never have to think about it again.

You only need this when you actually use a remote: sharing a project, opening one
your team hosts on GitHub, or syncing. Opening a *public* project (like the
[example cave](../getting-started/open-the-example-cave.md)) needs no sign-in at
all — CaveWhere only asks when a step genuinely requires your GitHub identity.

## Your identity and your GitHub account are two different things

CaveWhere already asked for a name and email when you first ran it (see
[Set Up Your Identity](../getting-started/set-up-your-identity.md)). That
**identity signs your saves** — it's the author stamped on each version in the
history, like signing a survey book. It lives on your machine and needs no
account or network.

Signing in to GitHub is a **separate** step that **authorizes the push**. Your
identity says *who made this version*; your GitHub sign-in is what lets CaveWhere
actually upload it to the shared copy. You can have your identity set and never
sign in to GitHub — you just won't be able to sync until you do.

## Signing in: the device code

CaveWhere doesn't ask for your GitHub password. Instead it uses GitHub's
**device flow**: CaveWhere shows you a short code, you type that code into GitHub
in your browser, and GitHub tells CaveWhere it's really you. Nothing sensitive
passes through CaveWhere.

The easiest place to sign in the first time is the **Online projects** page. Open
**File → Open from Online…** (`Ctrl+Shift+O`), then, in the **Account** picker,
choose **Add Account** — that reveals the GitHub sign-in panel.

![The Online projects page, titled "Connect to a Remote Caving Area." Below a "Clone from HTTP/SSH" box and an Account picker, a GitHub panel reads "Sign in to GitHub to access your repositories" above a "Connect to GitHub" button.](../images/collaboration-sign-in.png)
*The sign-in panel on the Online projects page, reached with File → Open from
Online… and the Account picker's Add Account. Every other place you sign in shows
the same Connect to GitHub button.*

From there the steps are always the same:

1. Click **Connect to GitHub**. CaveWhere shows *"Requesting a sign-in code from
   GitHub…"* while it fetches one.
2. A short code appears, with the instruction *"Enter the code below at GitHub to
   finish signing in."* Click **Copy and Open GitHub** — CaveWhere copies the code
   and opens GitHub's device page in your browser.
3. In the browser, paste the code and approve. CaveWhere is meanwhile polling
   ("*Trying connection in N s*") and notices the moment you approve.
4. You're signed in. The panel gets out of the way and shows whatever you were
   trying to do — your repository list, the create-repo form, or the sync itself.

## Install the CaveWhere app on GitHub

The first time you sign in, you'll likely be told:

> You're signed in, but CaveWhere isn't installed on any of your GitHub accounts
> yet. Install it to see your repositories.

Signing in proves who you are; **installing the CaveWhere GitHub app** is what
grants access to specific repositories. This split is deliberate — it lets you
give CaveWhere access to only the repos you choose, rather than your whole
account. Click **Install CaveWhere on GitHub**, pick the account (and, if you
like, the specific repositories) on GitHub, and finish. CaveWhere waits for the
install and updates on its own; once it's done, your repositories appear.

## Where you sign in

There is no single "sign in" page — CaveWhere raises the same panel in context,
at the moment a step needs it. Besides **File → Open from Online…** (shown above),
that's:

- The **Sync button** (top-right): its right-click menu has **Log in**, and
  clicking the button when it shows a lock icon starts sign-in.
- **Setting up a remote** to [share a project](share-a-project.md).
- The **reconnect prompt** that appears if your access later expires (below).

## More than one account

CaveWhere supports **multiple GitHub accounts** — a personal one and a caving-club
one, say. Each is listed as **GitHub (*username*)** in the **Account** picker that
appears on the sharing and online-project screens. Pick **Add Account** in that
list to sign a new one in; pick an existing account to make it the active one that
new work uses. One account is active at a time.

## Your credentials are kept safely

When you sign in, GitHub hands CaveWhere an access token, and CaveWhere stores it
in your **operating system's keychain** — macOS Keychain, Windows Credential
Manager, or the Linux secret service — under the name "CaveWhere", one entry per
account. The token is never written into your project file, so handing someone a
project never hands them your GitHub access. You never type or paste a personal
access token; the device flow is the whole of it.

Tokens refresh automatically in the background. Occasionally one expires for good
and CaveWhere shows **GitHub Access Expired** — *"Your GitHub session has expired.
Reconnect to continue syncing."* Click **Reconnect** and repeat the short device-code
step; it picks up exactly where it left off.

## Signing out

To disconnect an account, use **Log out** (on the Sync button's right-click menu
and on each remote's card under **Remote Settings**) or **Forget Account** on the
online-projects page. CaveWhere confirms with *"This will remove your saved GitHub
account from CaveWhere on this device"* and, on **Remove**, deletes the stored
token from your keychain and drops the account from CaveWhere.

This only affects CaveWhere on this machine. It doesn't change anything on GitHub,
and it doesn't remove a project's remote — unlinking a project from its shared copy
is a separate action on the [Remote Settings](sync-your-changes.md) page.

## Next steps

- [Share a Project](share-a-project.md) — now that you're signed in, put a cave on
  GitHub and send your team a link.
- [Sync Your Changes](sync-your-changes.md) — the Sync button and what it does.
- [Open a Shared Project](open-a-shared-project.md) — download a cave from your
  repository list or a link.
