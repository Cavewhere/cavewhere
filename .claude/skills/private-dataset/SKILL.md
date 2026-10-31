---
name: private-dataset
description: Keep private cave data out of the CaveWhere repository — real GPS fixes, cave and survey names, survey shots, owner file paths and exact project stats. Use when the owner says a bug or task involves a private data source (record its names in the local denylist), when writing a fixture, test or plan derived from real data (reproduce it with fake data), when reviewing a diff or a plans/ file for private data, and when a pre-commit or commit-msg hook reports privatecheck findings.
---

# Private datasets

CaveWhere is developed against real survey projects that belong to cavers, not to the
repository. Bugs found in them get reproduced in the repo with **fake data that keeps the
shape and drops the identity**. Everything committed or written under `plans/` (fixtures,
tests, code comments, plans, prototypes, commit messages, screenshots) holds only fake or
public data.

## What counts as private

| Kind | Private | Fake replacement |
|---|---|---|
| Location | Real fixes: UTM, lat/long, DMS, Compass `.mak` `[m,…]`, Walls `#FIX`, Survex `*fix`, map links | Round values in an invented place: `500000 3000000` UTM, `0.0 0.0` or other whole-tenth degrees, or a local frame (`0 0 0`) |
| Names | Cave, survey, book, trip, station-prefix, project and file names; surveyor and team names; region names | NATO or Greek words (`alpha`, `BRAVO.DAT`), descriptive names (`passage.svx`, `case_pairs.dat`), `Phake Cave` |
| Survey data | Shots copied from a real project, real declinations and dates | Hand-written round readings, or generated shots (`writeFlatSurvex`-style helpers) |
| Paths | `/Users/<owner>/Desktop/<project>`, `~/Downloads/...` | `/Users/friend/survex/alpha.svx`, a `QTemporaryDir` |
| Shape stats | Exact counts: "1,247 surveys", "37 books", "21,406 stations" | One significant figure below a thousand ("~50 books", "~300 stations"), two above ("~1.2k surveys", "~21k stations") |
| Screenshots | A real cave's name, plot, map or note scan | A fixture opened in CaveWhere |

**Public data is allowed** when its source is obviously published: OpenStreetMap or
Grottocenter. Say so where the data lives: a `public-source: <url>` line in the file
(as a comment in the file's own syntax), or a `PUBLIC_SOURCE` file next to it with that
line. Screenshots of CaveWhere showing a public dataset are fine.

## Recording a private source

When the owner says a task involves a private data source, record every identifying
name it carries before doing anything else: cave names, survey and book names, project
and file stems, station prefixes that are words (not `A`, `B1`), people. Then confirm
the list back to the owner.

```sh
python3 .claude/skills/private-dataset/scripts/privatecheck.py --add-names "Name One" name_two ...
```

The denylist is `~/.config/cavewhere/private-names.txt` (override with
`CAVEWHERE_PRIVATE_NAMES`), one name per line, `#` comments allowed. It lives outside the
repository on purpose: a committed list of real names would itself be the leak. Never
quote it into a file, a plan, a commit message or a subagent prompt that will write to
the repo. When briefing a subagent about private data, give it the rules here and a
code name, never the real names.

Match is case-insensitive on word edges, where `_`, `-` and `.` count as edges, so
`alpha` also catches `alpha_north.svx`. Add short or common-word names with care,
since they will match ordinary text.

## Reproducing a bug with fake data

Keep what the bug depends on; replace everything that identifies the project.

1. **Name the shape.** Write down which properties trigger the bug: loop topology,
   `*begin` nesting depth, include structure, a case pair, a fix in a different CS,
   a station that is first in its survey, rough scale. That list is the fixture spec.
2. **Build the smallest fixture with that shape** using fake names. Prefer generating
   scale in the test (a 1,000-trip fixture built by a helper) over committing a big file.
3. **Fixes:** use round fake values. When the bug needs real relative geometry between
   fixes, translate the whole set into the fake zone and round each value to 10 m; the
   shots carry the geometry, the fixes only anchor it.
4. **Plans and prose:** refer to the real project by a code name ("the owner's large
   Walls project (P1)") and its rounded shape ("~1.2k surveys across ~40 books").
5. Run the scanner on the new files before handing back.

## Scanning

```sh
S=.claude/skills/private-dataset/scripts/privatecheck.py
python3 $S --staged                 # what is about to be committed
python3 $S --range origin/dev..HEAD # a branch's added lines
python3 $S --worktree --exclude plans/ --exclude QuickQanava/
                                    # uncommitted work vs HEAD, plus untracked files
python3 $S plans/FOO_PLAN.md        # a plan, fixture dir, or any files
```

It reports `file:line: KIND: message`:

- **NAME**: a denylist name, in the text or in a file path. Always a real finding;
  there is no inline override.
- **COORD**: a fix or a lat/long (decimal pair on a line with a geographic word, or DMS)
  that is not round. Fixes under 10,000 in every coordinate are a local frame and pass.
- **STAT**: an exact count in prose (`.md`, `.html`, `.txt`, commit messages); code is
  exempt because its counts assert on our own fixtures.
- **PATH**: a personal-data directory (`Desktop`, `Downloads`, `Dropbox`, iCloud, ...).

`private-ok` on a line accepts a deliberate COORD, STAT or PATH finding, for example an
invented-but-precise coordinate a parser test needs. Use it sparingly and only for data
you know is fake. It has no effect on NAME.

The scanner is a floor, not the review. It cannot recognize a real name missing from the
denylist or real shots with fake names. Reviewing by eye is still part of the job, below.

## The commit gate

`scripts/install-hooks.sh` installs a `pre-commit` hook (scans staged added lines) and a
`commit-msg` hook (scans the message) into the clone's shared hooks directory, so every
worktree gets them. A finding blocks the commit. Fix the data; do not reach for
`--no-verify`, which is the owner's call alone.

The pre-commit hook lists staged images as a reminder; it cannot read them. Before
committing a screenshot, check it against the checklist below.

## Reviewing a diff or a plan

When asked to review for private data, or as part of a code review or plan review:

1. Run the scanner on the target (`--range`, `--staged`, or the plan file).
2. Read the target by eye for what the scanner cannot see:
   - Names that look like real places, caves or people and are absent from the denylist.
     Ask the owner when unsure; add confirmed ones with `--add-names`.
   - Fixture shots with realistic, non-round readings and no generator: likely copied.
   - Declinations, dates or survey team lines that match a real trip.
   - Descriptions precise enough to identify the project without its name.
3. Report each finding as `🔒 file:line — what is private — suggested fake replacement`,
   alongside the review's other findings. In the review rating system a 🔒 finding is 🔴:
   it blocks the commit.
4. Fix findings in your own uncommitted work directly. Report findings in already
   committed history without rewriting it; history rewriting is the owner's decision.

### Screenshot checklist

- [ ] The project open in the screenshot is a repo fixture, a `Phake`-style demo, or a
      public dataset (OpenStreetMap / Grottocenter source).
- [ ] No real cave, survey, surveyor or file name in titles, trees, tabs or recent-file lists.
- [ ] No map tiles or coordinates centered on a real private cave.
- [ ] No real note scans or LiDAR captures.
