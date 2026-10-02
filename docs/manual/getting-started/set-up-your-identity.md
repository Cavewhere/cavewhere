---
title: Set Up Your Identity
summary: The name and email CaveWhere asks for on first launch: why version control needs them, what they sign, where CaveWhere keeps them, and how to change them later.
problem: Get past the "Let's set you up!" page on a fresh install, and understand what CaveWhere does with the name and email it asks for.
keywords: [welcome, first run, setup, identity, name, email, git, account, signature, author, settings, git identity, privacy]
related: [install-cavewhere.md, find-your-way-around.md, ../projects-and-files/save-a-project.md]
---

# Set Up Your Identity

## Why / when you need this

The first time you open CaveWhere, it asks for a name and email before showing
you anything else. It needs them because
[every save is a version](../projects-and-files/save-a-project.md) recorded in
the project's history, and a version records *who* made it, the same way a survey
book gets signed. CaveWhere asks once, up front, instead of interrupting your
first save to do it.

## Fill in the setup page

A fresh install opens on **"Let's set you up!"**.

![The CaveWhere setup page. A heading reads "Let's set you up!", followed by a paragraph explaining that CaveWhere uses version control to track changes, an italic line about the information not being shared online, then two empty text fields with the placeholders "Your name" and "your@email.com", and a "Next" button.](../images/getting-started-welcome.png)
*The setup page is the whole app until you fill it in. No Skip button exists.*

Type your name and your email, then click **Next**.

The gray text inside the fields, **"Your name"** and **"your@email.com"**, is
placeholder text that disappears as you type.

**Next only proceeds once both fields validate.** Click it too early and the
button shakes and marks whatever went wrong:

- **"No name found. Enter a name"** flags an empty name. CaveWhere accepts any
  name, and it does not care whether you type your full name or whatever your
  survey partners call you.
- **"Enter a valid Email address ex. your@email.com"** flags an address that is
  empty or shaped wrong.

CaveWhere checks only that the address *looks* like an address. It never sends
anything to it, and it never verifies that the address exists.

## What CaveWhere does with them

They sign every version you save. In the app's own words:

> CaveWhere uses version control (Git) to keep track of changes to your cave
> data. Your name and email are used to record who made each change—just like
> signing your work. This information stays with your files and helps with
> collaboration, even if you're working offline.

Working alone, the name makes your project's
[history](../projects-and-files/save-a-project.md) readable: a list of saves,
each carrying a date and an author. Working with other people, it tells you who
changed the passage you are looking at.

Git identifies an author by email address, which is why CaveWhere asks for one.
The address carries across tools, and it lets a service like GitHub recognize a
commit as yours instead of an unattributed change. I recommend using the same
address you use on GitHub. A mismatched one still saves fine, but your commits
may show up there under no account at all.

**CaveWhere sends nothing anywhere.** It writes your name and email into your
project's history, on your own disk. Again in the app's own words:

> This information is only used within your version history and is not shared
> online unless you choose to sync with a service like GitHub.

Syncing stays opt-in and has its own chapter.
[Sign In to GitHub](../collaboration/sign-in-to-github.md) picks up where this
page leaves off and explains how your identity differs from your GitHub account.
Until you set sync up, your identity goes no further than your own machine.

## Where CaveWhere keeps it

CaveWhere keeps your identity **once per machine, in its own settings**, never
inside a project. Every project you open or create on that machine gets signed
with it, and CaveWhere asks exactly once no matter how many caves you go on to
make.

Qt writes those settings per user, under an `account` group holding 2 keys,
`name` and `email`:

| Platform | Where it lands |
|---|---|
| macOS | `~/Library/Preferences/com.cavewhere.CaveWhere.plist`, as `account.name` and `account.email` |
| Linux | `~/.config/Vadose Solutions/CaveWhere.conf`, under `[account]` |
| Windows | `HKEY_CURRENT_USER\Software\Vadose Solutions\CaveWhere` |

Storing it per machine also means it does not travel with a project you hand to
someone else. When a survey partner opens your `.cw` on their machine, CaveWhere
asks *them* to set up, and their saves carry their name, which is the point.

## Change your name or email later

**Settings → Git**, under the **"Git Identity"** heading, holds the same 2
fields. Reach Settings from the File menu (**Settings…**). Changes take effect as
you type them; no separate Save button exists.

Editing your identity changes how *future* saves get signed. Versions already in
the project's history keep whatever name and email signed them, because history
records what happened and CaveWhere does not rewrite it. Fix a typo in your
email and the old spelling may well stay on the older saves.

## Next steps

- [Find Your Way Around](find-your-way-around.md): what you are looking at once
  the setup page clears.
- [Save a Project](../projects-and-files/save-a-project.md): what a save actually
  does, and why it has an author at all.
