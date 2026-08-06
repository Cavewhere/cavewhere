# The CaveWhere voice

Source material: the cavewhere.com blog, written by Philip Schuchardt by hand in
2020, before LLMs were involved in any of this. Plus the app's own shipped
strings, which mostly carry the same voice. Quotes below are verbatim, typos
included — they are evidence, not templates.

## Measured signature

Three hand-written corpora, 10,240 words total, spanning eight years and two
very different registers:

| corpus | words | what it is |
|---|---|---|
| cavewhere.com blog + home page | 6,250 | 2020, casual, instructional |
| *Quick 3D Cave Maps using Cavewhere* | 1,927 | ICS 2013 paper, formal academic |
| *Creating 3D Cave Maps by Warping…* | 2,063 | ICS 2021 paper, formal academic |

Control: `docs/manual/`, 50,490 words. Nothing was routed through a summarizing
fetch — the blog came from `curl` plus a local HTML parse, the 2021 paper from
the `.docx` XML, the 2013 paper from `pdftotext`. The check that extraction was
faithful is that the known typos survive it ("illunstrations", "maximun", "Each
vendors", "to image your computer").

Having two registers is what makes this usable. A feature that holds in *both* a
chatty blog post and a peer-reviewed paper is **voice**. A feature that swings
between them is **genre**, and copying it into a manual is cargo-culting.

Reproduce with `scripts/voiceprint.py`; target at `references/voiceprint-blog.json`.

### The invariants — voice, enforce everywhere

Stable across all three, and clearly separated from the manual. These are the
non-negotiable ones, marked `!` by `voiceprint.py`.

| per 1k words | blog 2020 | paper 2013 | paper 2021 | manual | gap |
|---|---|---|---|---|---|
| **em dashes** | **0.00** | **0.00** | **0.00** | **19.73** | see below |
| **points at a figure** | 4.64 | 4.67 | 3.39 | 0.16 | 21-29x |
| **digits / measurements** | 20.32 | 27.50 | 14.06 | 3.90 | 3.6-7x |
| **distinct named things** | 21.92 | 30.62 | 27.14 | 10.71 | 2-2.9x |
| **contractions** | 6.72 | 6.23 | 5.33 | 18.72 | manual 3x high |
| mean sentence | 14.2 | 16.9 | 16.6 | 20.9 | ceiling ~17 |

Three things worth pausing on.

**Zero em dashes in 10,240 words**, across a casual blog and two peer-reviewed
papers, eight years apart. Read that as rarity, not prohibition. Philip's own
account: he does use them, "extremely rarely. They really need to be emphasizing
something, basically screaming." A 10k-word sample simply does not catch one at
that rate.

That makes the em dash a **volume control**, and it is the single highest-yield
fix in the project. The manual runs 19.73 per 1000 words, every one of them the
routine appositive ("Sync disabled - upgrade CaveWhere", "Body copy - longer
descriptions"). Shouting twenty times per thousand words is the same as never
shouting. Keep at most the one clause on a page that genuinely needs screaming;
everything else becomes a comma, a period, or parentheses.

The two dashes that *do* appear in his corpus are both numeric ("2^31 - 1"), not
prose.

**Contractions land at 5.3-6.7 in all three**, including formal academic prose.
That tightness is the surprise: he does not code-switch. The manual sits at
18.72, nearly 3x too *high*. It is more casual than he has ever been in print.

**Pointing at the figure survives the register change**, it just changes clothes:
the blog says "see below" and "check out the graph below", the papers say "see
Figure 3" and "in Figure 4". Same habit, 21-29x the manual's rate. `docs/manual/`
has 60+ generated screenshots and its prose almost never refers to one.

### Register — genre, judge by document type

These swing hard between blog and paper, so there is no single right number. A
user manual is instructional like the blog rather than like a paper, so the blog
column is usually the better guide, but that is a judgment call and not a target.

| per 1k words | blog 2020 | paper 2013 | paper 2021 | manual |
|---|---|---|---|---|
| second person "you" | 18.08 | 0.00 | 0.48 | 25.13 |
| exclamation marks | 1.92 | 0.00 | 0.00 | 0.10 |
| first person "I" | 1.60 | 0.00 | 0.48 | 0.08 |
| hedges / admissions | 5.44 | 1.04 | 2.42 | 1.27 |
| makes a recommendation | 0.96 | 0.00 | 0.00 | 0.00 |
| sentence-initial But/And/So | 0.64 | 0.00 | 0.00 | 0.69 |
| parentheses | 6.08 | 7.78 | 9.69 | 6.40 |

Even here the manual is an outlier in both directions at once: 25.13 "you" is
above his most conversational register, while zero recommendations is below his
most formal one.

Headings phrased as a question: **6.5%** of blog headings, **0.2%** of the
manual's 450.

### What this says

**The manual's problem is not tone.** Contractions, "you", parentheses and
sentence-initial "But" are all at or above his rate in any register. Telling it
to loosen up makes it worse.

The gap is **substance**: numbers, named things, recommendations, admissions,
and pointers at the figures. He puts a measurement in roughly every other
sentence; the manual manages one in five. So it reads like an impression of a
friendly technical writer with everything worth reading left out. Fixing that is
not a rewrite, it is going and finding the numbers.

**Three traps.** Sentence-length *variance* is 6.0 in the blog and 10.3 in the
manual: "humans are burstier" is backwards here, and a rule built on it would
flag the real voice as fake. The curly apostrophes are WordPress and Word
autocorrect, not his typing. And **"question in body prose" is not a signal**: it
looks separating (0.32 / 0.00 / 0.00 against the manual's 0.65) but the counts
are far too small to carry weight, and the manual is the higher one. His
questions live in headings, which is real; the body-prose number is noise.

## A note on evidence

Two tiers, and they are not interchangeable.

**Verified**: the cavewhere.com blog (2020) and the two ICS papers (2013, 2021).
All three predate any AI assistance in this project, all three were extracted
from source without a model in the path. Every rate in the tables above comes
from these and nothing else.

**Unverified**: quotations below marked *(shipped string, unverified)*. They read
like the voice, which is why they are here, but `git log` dates every one of them
to 2026 - the same window that produced the 32 em dashes now sitting in the app's
tooltips ("Crop - keep points inside the polygon", "Sync disabled - upgrade
CaveWhere"). Some are certainly his; some are certainly not; the history cannot
tell them apart. Treat them as illustrations, never as measurements, and never
add a new one to this file without checking when it landed.

## Ten habits, with evidence

### 1. A question as a heading, answered immediately

> **It takes a long time to load images in CaveWhere. Why is that?**
> The short answer is because of DXT1 compression. But first, let's talk about
> compression in general.

> **Why is this important?**
> 2.0GB is a ton of space.

> **Is your cartographer too slow?**

> But sometimes this process goes wrong, but why?!

Note "but... but why?!" — a doubled conjunction and an interrobang. No model
writes that. Note also that "2.0GB is a ton of space." is a complete paragraph.

### 2. Exclamation marks, on technical facts

> Raw data takes tons of space in RAM!

> all the scraps images could be many, many megabytes or even gigabytes of data!

> it's recommended to keep compression on and reap its benefits!

> Using more memory than what's on computer my cause your computer to hang!
> *(shipped string, unverified)*

Getting excited about a memory footprint is a human move. The AI-drafted manual
contains almost no exclamation marks at all.

### 3. First person, for judgment and for provenance

> I recommend disabling Mipmaps or changing the Minification Filter to Linear

> I generated this benchmark with 6 core / 12 thread Intel i7-8750H and Nvidia
> 1070 Max-Q graphics card.

He owns the recommendation and he owns the measurement, naming his own laptop.
`docs/manual/` currently never uses "I". That absence is itself a tell.

### 4. Saturated with checkable specifics

> It was first patented in 1997 by S3 Inc.

> the RAW RGB data would now only take 3.91MB instead 23.44MB

> Squish takes 8.4 times slower than the GPU implementation

> Going to File->Settings (on Windows and Linux) or (CaveWhere -> Preferences) on MacOS

> The system in the screenshot above has 12 available threads

> This is an unscientific benchmark of compressing ~7.5mi of cave.

> CaveWhere was using about 250MB of RAM displaying an eight-mile cave.

> exporting 100in x 100in at 300pixel per inch map would require 3.35GB

Vendors get named (NVidia, AMD, Intel, S3, Qt, ANGLE, Squish, Wikipedia). Formats
get named (Walls, Survex, Compass, Illustrator, Inkscape). Versions anchor claims
in time — "In CaveWhere 0.08 and below", "In CaveWhere version 0.09 and above",
"Currently, in version 0.09" — where the manual would write "currently" or
"recently".

### 5. Admits limits, bluntly

> These localized bumps are undesirable, and will hopefully be fixed in future
> versions of CaveWhere.

> Currently, in version 0.09, the grid size is fixed but could possibly be an
> option to choose in the future.

> Although CaveWhere detects your rendering settings automatically, sometimes it
> fails to detect the best texture filtering technique.

> These are ballpark limitations. The limits will depend on the size of the cave
> and how much memory CaveWhere is already using. The results may vary.

> The main drawback to using all the stations is that unrelated stations can
> affect the final morphed result.

Never "CaveWhere ensures accurate results." Always the caveat, named.

### 6. Makes the call, with the loser named

> Squish (CPU compression) has slightly better image quality than the graphics
> card but isn't worth the wait.

> Magnification Filter is more of a visual preference than anything else.

> The Software Renderer will always work but does not have DXT1 compression
> support and is by far the slowest renderer.

> Smaller spacing gives smoother warps but costs performance. *(shipped string, unverified)*

> Lower is fresher; higher coalesces edit bursts. Defaults: 3 s desktop, 5 s
> mobile. *(shipped string, unverified)*

The shape is `X is better at A but worse at B, use Y`. Not "each approach has
its own strengths."

### 7. Talks to the reader and invites them to judge

> Can you tell the difference, see below?

> If you cannot tell the difference in the images above, here's a more concrete
> example below.

> If you are ever curious what OpenGL version and extension that CaveWhere is
> using, OpenGL Info can help.

> if you don't feel like waiting around for compression

> Hmm, you need to **check** either *front* or *back sights* box, or both,
> depending on your data. *(shipped string, unverified)*

> Still waiting for you to authorize on GitHub. *(shipped string, unverified)*

> GitHub asked us to slow down. Trying again in 5 s. *(shipped string, unverified)*

That last one is worth studying. It anthropomorphizes GitHub, states what
happened in four words, and says what happens next with a number.

### 8. Casual asides, self-interruption, parentheses

> and Linear is well Linear filtering

> the operating system (aka Windows)

> here's Wikipedia to the rescue

> just uncheck GPU compression in Settings

> Hopefully it goes. CaveWhere loves big caves, but it handles small ones too.

Parentheses do the aside work that the manual currently gives to em dashes:
"(such as NVidia, AMD, or Intel)", "(i.e. P1, P2, p3)", "(see settings)",
"(check out the graph below)", "(get early releases and more)".

### 9. One analogy, followed through

> It is recommended to image your computer's memory as a bunch of labeled
> buckets. The label is an offset from the first bucket, so the first bucket
> would be bucket 0. The labels on the buckets go from 0 to 2³¹ – 1 for a 32bit
> application (2,147,483,648 total labeled buckets). There are only 31 bits for
> labeling buckets, because the last bit stores the sign.

> Your name and email are used to record who made each change, just like signing
> your work. *(shipped string, 2026, em dash removed here - see the em-dash note)*

One homely image, sustained across a paragraph and carrying real arithmetic. Not
decorative metaphor scattered through the page.

### 10. "See below" as connective tissue

> See below as an example. · check out this tutorial · Checkout this article for
> details · see video below · Below shows the differences amongst all four
> filtering modes.

Slightly repetitive, informal, and it never pretends the prose is standalone.

## What the manual does that he does not

Real lines from `docs/manual/`, with the tell named:

| manual line | tell |
|---|---|
| "That choice buys two things worth having" | editorializing significance |
| "Under the hood, CaveWhere syncs with **Git**" | cliché; name the subsystem |
| "a merge engine robust enough that" | "robust" |
| "the difference is the key to the whole chapter" | telling the reader what matters |
| "## What the rest of this chapter covers" | outline-shaped closing section |
| "all of it makes more sense once the model here is clear" | over-explaining |
| 18 em dashes in 868 words | punctuation habit |

Rewrites in his voice:

- "That choice buys two things worth having: a complete, trustworthy history…"
  → "Why Git? Two reasons. You get a complete history, and you get a merge engine
  good enough that two people can edit the same cave at once."
- "Under the hood, CaveWhere syncs with **Git**"
  → "CaveWhere syncs with **Git**, the same version-control system programmers use."
- "the difference is the key to the whole chapter" → delete it.

## Things to get right that are not voice

- `docs/manual/AUTHORING.md` binds: front matter fields, problem-first opening
  sections, kebab-case filenames, relative links, `llms.txt` + `index.md` entries.
  This file governs how sentences sound, never those rules.
- Second person, present tense, active voice (AUTHORING.md § Style) — consistent
  with everything above.
- Project vocabulary is fixed: a *trip*, a *scrap*, a *lead*, a *station*,
  *carpeting*, *morphing*. The glossary is the source of truth.
- The shipped typos ("maximun", "illunstrations", "interperate", "my cause") are
  not the voice. Don't copy them; don't launch a spelling cleanup either.
