---
title: Set Up Your Identity
summary: The name and email CaveWhere asks for on first launch — why version control needs them, what they are used for, where they are stored, and how to change them later.
problem: Get past the "Let's set you up!" page on a fresh install, and understand what CaveWhere does with the name and email it asks for.
keywords: [welcome, first run, setup, identity, name, email, git, account, signature, author, settings, git identity, privacy]
related: [install-cavewhere.md, find-your-way-around.md, ../projects-and-files/save-a-project.md]
---

# Set Up Your Identity

## Why / when you need this

The first time you open CaveWhere, it asks for your name and email before it
shows you anything else. It needs them because
[every save is a version](../projects-and-files/save-a-project.md) recorded in
the project's history, and a version records *who* made it — the same way you
sign a survey book. Rather than interrupt your first save to ask, CaveWhere asks
once, up front.

## Fill in the setup page

A fresh install opens on **"Let's set you up!"**.

![The CaveWhere setup page. A heading reads "Let's set you up!", followed by a paragraph explaining that CaveWhere uses version control to track changes, an italic line about the information not being shared online, then two empty text fields with the placeholders "Your name" and "your@email.com", and a "Next" button.](../images/getting-started-welcome.png)
*The setup page is the whole app until it is filled in — there is no Skip past
it.*

Type your name and your email, then click **Next**.

The two fields have no labels; the grey text inside them — **"Your name"** and
**"your@email.com"** — is a placeholder that disappears as you type.

**Next only proceeds once both fields are valid.** Click it too early and the
button shakes and marks whatever is wrong:

- **"No name found. Enter a name"** — the name is empty. Any name is accepted;
  CaveWhere doesn't care whether it's your full name or what your survey partners
  call you.
- **"Enter a valid Email address ex. your@email.com"** — the email is empty or
  isn't shaped like an address.

CaveWhere checks only that the address *looks* like one. It never sends anything
to it and never verifies that it exists.

## What your name and email are used for

They are the signature on every version you save. In the app's own words:

> CaveWhere uses version control (Git) to keep track of changes to your cave
> data. Your name and email are used to record who made each change—just like
> signing your work. This information stays with your files and helps with
> collaboration, even if you're working offline.

Working alone, the name is what makes your project's
[history](../projects-and-files/save-a-project.md) readable — a list of saves
with a date and an author beside each one. Working with other people, it's what
tells you who changed the passage you're looking at.

The email is asked for because it is how Git identifies an author. It carries
across tools: it's what lets a service like GitHub recognize a commit as yours
rather than as an unattributed change.

**Nothing is sent anywhere.** Your name and email are written into your project's
history, on your disk. Again in the app's own words:

> This information is only used within your version history and is not shared
> online unless you choose to sync with a service like GitHub.

Syncing is opt-in and has its own chapter —
[Sign In to GitHub](../collaboration/sign-in-to-github.md) picks up where this
one leaves off, and explains how your identity differs from your GitHub account.
Until you set sync up, your identity goes no further than your own machine.

## Where it is stored

Your identity is stored **once per machine, in CaveWhere's own settings** — not
inside any project. Every project you open or create on that machine is signed
with it, and you are asked for it exactly once, no matter how many caves you go
on to make.

That also means it doesn't travel with a project you hand to someone else. When
a survey partner opens your `.cw` on their machine, CaveWhere asks *them* to set
up, and their saves are signed with their name — which is the point.

## Change your name or email later

**Settings → Git**, under the **"Git Identity"** heading, has the same two
fields. Reach Settings from the File menu (**Settings…**). Changes take effect as
you type them; there is no separate Save button.

Editing your identity changes how *future* saves are signed. Versions already in
the project's history keep the name and email they were saved with — history is a
record of what happened, so CaveWhere doesn't rewrite it. If you fix a typo in
your email, expect to see the old spelling on older saves.

## Next steps

- [Find Your Way Around](find-your-way-around.md) — what you're looking at once
  the setup page is out of the way.
- [Save a Project](../projects-and-files/save-a-project.md) — what a save
  actually is, and why it has an author at all.
