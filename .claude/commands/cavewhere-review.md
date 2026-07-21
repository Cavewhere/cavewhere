---
description: Pre-commit review of the current changes — Qt C++ review (if C++ changed), Qt QML review (if QML changed), auto-fix of high-confidence findings, simplify, plus out-of-scope/architectural notes
argument-hint: "[optional target: PR#, branch, or path — defaults to current changes]"
---

# CaveWhere pre-commit review

Review the changes under review with the right tools, fix what's clearly broken, then
surface anything worth doing later that's out of scope for this commit. Work the phases
in order.

The shape is: **scope the diff and find the governing plan → one cheap inline lint pass →
one big parallel batch of READ-ONLY agents (right-sized to the diff) → investigate &
auto-fix the 🔴 findings → simplify (it also edits files, so it follows the fixes) → one
consolidated report.**

## Rating system

Every finding from the C++ and QML review lenses gets exactly one icon. The icon encodes
**confidence that this is a real defect in the changed code**, which in turn decides what
happens to it:

| Icon | Confidence | Meaning | Action |
|---|---|---|---|
| 🔴 | >80 | Real defect, in scope, you can name the concrete failure it causes | **Investigate and fix now** (Phase 2) |
| 🟡 | 60–79 | Plausible defect, but depends on a fact you haven't confirmed (a call site, a thread context, a lifetime) | Report only — state exactly what would settle it. Cap ~10. |
| 🟢 | <60 | Style, taste, or a nit the reader can take or leave | Report only, one line each. Suppress entirely if it's noise. |

Rate on confidence, **not** severity: a certain-but-small bug is 🔴, a
catastrophic-if-true guess is 🟡. If you're torn between two icons, take the lower one —
a 🟡 that turns out real costs the user a follow-up, a 🔴 that turns out wrong costs them
a bad edit.

Out-of-scope refactors and architectural items use the same three icons on a **different
axis** — payoff vs. cost, since "confidence" doesn't apply to work that hasn't been done:

| Icon | Meaning |
|---|---|
| 🔴 | High payoff and the diff has already made it urgent — this is actively costing something (a bug farm, a boundary already leaking). Do it soon, in its own commit. |
| 🟡 | Real payoff, no deadline. Worth a `plans/` entry or an issue. |
| 🟢 | Nice to have. Note it and move on. |

Never auto-fix an out-of-scope item, whatever its icon.

### Plan markers (second icon, only when it applies)

When Phase 0b finds a governing plan, any item the plan has something to say about gets a
**second** icon after the `file:line`. This is a different question from the icons above —
not "is it real" or "is it worth it" but "is it already accounted for":

| Icon | Meaning | Action |
|---|---|---|
| 📋 | Already in the plan — a later commit in the sequence, or listed under "Not in this phase". Name the section or commit. | None. The plan has it. |
| ➕ | Not in the plan and should be. | Report it; say which plan and roughly where it belongs. Don't edit the plan. |
| ⚠️ | The plan is now **stale or contradicted** by this diff — it describes code, a class, or an approach the change has superseded. | Report it. Say what the plan claims and what's now true. |

Rules that keep this from becoming noise:

- **The marker is optional.** No plan, or the plan says nothing about the item ⇒ no second
  icon. Most lines will have one icon. That's the point — a second icon should mean
  something, so don't reach for one to fill the slot.
- **Never add a marker that merely restates the checkbox or the confidence icon.** There's
  no "fix now" / "fix later" marker: a checked box already means fixed now, and an
  unchecked 🔴 already means deferred.
- **📋 does not veto a fix.** A confirmed 🔴 that's cheap and inside the diff still gets
  fixed in Phase 2 — being on the plan's list doesn't make a real bug wait. Use 📋 to
  explain a *deferral you'd make anyway* (the fix is genuinely that commit's work), not as
  an excuse to skip work you could do now. When you do fix a 📋 item, say so, since the
  plan's commit sequence is now partly done.
- **⚠️ is the highest-value marker here and the easiest to miss.** A plan that describes
  superseded code will actively mislead the next session. Look for it deliberately.
- **Never edit the plan file** during the review and report phases. Even ⚠️ and ➕ are
  reported, not applied. The one exception is Phase 5 interactive triage, and only when the
  user explicitly picks "Add to plan" for an item — that's the user triaging the change, not
  the review deciding for them.

## Phase 0 — Determine scope (do this first, in one step)

Figure out the diff to review:

- If `$ARGUMENTS` is given, treat it as the target (a PR number, a branch name, or a
  file/path) and scope the review to that.
- Otherwise scope to the current work: run `git diff @{upstream}...HEAD` for the branch's
  commits, and **also** `git diff HEAD` for uncommitted changes — this command is meant to
  run right before a commit, so include the working tree. If `@{upstream}` is unset, fall
  back to `git diff master...HEAD` (CaveWhere's main branch is `master`).

From the changed-file list, classify:

- **C++ changed** = any of `*.cpp`, `*.cc`, `*.cxx`, `*.h`, `*.hpp`, `*.mm` in the diff
- **QML changed** = any `*.qml` in the diff

State the scope, the file count, and what you detected (e.g. "17 files: C++ yes, QML yes")
before proceeding. If the diff is empty, say so and stop.

## Phase 0b — Find the governing plan (cheap; skip fast if there isn't one)

CaveWhere keeps per-feature plans in `plans/` (both `.md` and `.html`). Figure out whether
this diff is executing one, in roughly this order — stop at the first solid hit:

1. **The diff itself.** A modified/added file under `plans/` is the strongest signal —
   the user is working the plan and updating it as they go.
2. **The branch name.** Match it against plan filenames (`externalFiles` ⇒
   `EXTERNAL_FILE_PHASE*.html`). If a feature has numbered phases, pick the phase the
   recent commits are actually in (`git log --oneline -15`), not phase 1 by reflex.
3. **Recent commit subjects.** They often quote the plan's commit-sequence entries verbatim.

If nothing matches within a couple of cheap commands, **say "no governing plan found" and
skip all plan markers for the rest of the run.** Do not go fishing through 60 plans, and
do not force a weak match — a wrong plan is worse than no plan, because every marker it
produces is then noise. If two plans plausibly apply, say so and pick the more specific one.

When you do find one, read it and pull out the parts a review can be checked against:

- the **commit sequence** (which work is deliberately scheduled for a later commit)
- **"Not in this phase" / scope-exclusion** sections (which gaps are deliberate)
- **acceptance criteria** and **per-commit test gates** (what "done" means here)
- any **architecture or naming decisions** the diff might have since superseded

Name the plan and the phase/commit you think the diff is executing before moving on, so a
wrong guess is visible to the user immediately rather than silently coloring every marker.
Pass this material to the Phase 1b agents — especially the architectural agent.

## Phase 1 — Lint, then ONE parallel batch of read-only agents

### 1a. Deterministic lint (inline, first — it's cheap and feeds the agents)

Run the bundled linters and keep the output to pass into the agents (so they don't
re-report mechanical findings):

- C++ (if C++ changed): `qt-development-skills/.../qt-cpp-review/references/lint-scripts/qt_review_lint.py <files…>`
- QML (if QML changed): `qt-development-skills/.../qt-qml-review/references/lint-scripts/qt_qml_lint.py <files…>`

Use the installed plugin version (glob the cache for the current version dir — don't
hard-code it). Plain mode for C++; CaveWhere is an application, not a Qt framework/module
(only add framework rules if the changed files carry `Q_*_EXPORT` / `Q_DECLARE_PRIVATE` /
private-header includes / `qt_internal_add_module`).

### 1b. Launch all read-only agents in a SINGLE message so they run concurrently

These are all read-only and mostly touch different files, so fire them as one parallel
batch (not one-skill-at-a-time). Keep the batch to a **reasonable** size — aim for ~6–8
agents total, not a reflexive 6+6+1. **Right-size to the diff**: launch a review lens only
when the diff actually contains code it applies to, and collapse or drop lenses that have
nothing to chew on. Pass every agent the file list, the Phase 1a lint output, and a
strict diff-scope instruction (report only on changed lines; read ±50 lines for context;
read-only — never edit).

Require every agent to return each finding with: `file:line`, a one-sentence statement of
the defect, a **concrete failure scenario** (inputs/state → wrong result), a confidence
score 0–100, and — for anything under 80 — the specific fact that would settle it. You
assign the icons yourself from those scores; the agents just report honestly.
An agent that finds nothing returns an empty list, which is a fine result.

**C++ review lenses** (from the `qt-cpp-review` skill's six domains): Model Contracts,
Ownership & Lifecycle, Thread Safety, API & C++ Correctness, Error Handling, Performance &
Quality. Don't launch all six by reflex:
- **Substantial C++ change** (engine/subsystem, many files): launch the lenses that apply,
  typically 4–6. Skip a lens whose subject is absent — e.g. no `QAbstractItemModel`
  subclass in the diff ⇒ skip Model Contracts; no threading/`synchronize`/worker code ⇒
  skip Thread Safety.
- **Small or narrow C++ change** (a few files, one concern): collapse to 1–3 combined-mission
  agents instead of six thin ones.
- Always weave in the CLAUDE.md house rules (function-pointer connect, `.at()`/const-ref
  reads, no bindable properties in new code, QStringLiteral in hot paths, no `*ForTesting`
  scaffolding, never goto, the per-frame GPU re-upload trap, k-prefixed constants).

**QML review** (the `qt-qml-review` skill is six UI-domain agents: bindings, layout, loaders,
delegates, states, performance). Right-size hard:
- **Real component/UI QML** changed: run the fuller fan-out (its domains earn their keep).
- **Test-only or trivial QML** (only `test-qml/tst_*.qml` TestCase files, or a tiny change):
  run **one** focused agent on test-quality conventions (tryVerify over fixed wait,
  no `waitForRendering` under offscreen, PID/`QTemporaryDir` temp paths for concurrent CI,
  alphabetical-order state leakage, stale comments, drift/dup) instead of the six UI agents —
  bindings/layout/loaders/delegates don't apply to test logic. Say in the report that you
  scaled it down and why.

**Architectural & out-of-scope agent** (always, 1 agent, in the same batch). Read-only; reads
the diff in the context of CaveWhere's architecture (consult `CLAUDE.md`) and reports —
**without changing code** — two lists:
1. **Out-of-scope refactors** this change makes worthwhile/obvious but bigger than this commit
   (what, where `file:line`, why worth it, why out of scope now).
2. **Major design changes** that would improve CaveWhere's architecture (leaking subsystem
   boundaries, a pattern worth generalizing across renderers/providers, an eroding
   front-end/back-end split, etc. — the change, the benefit, rough blast radius, and that
   it's out of scope now).

Have it propose a 🔴/🟡/🟢 payoff-vs-cost rating per item with a one-line justification;
you make the final call in Phase 4. Hold a HIGH bar — only genuine follow-ups, not nitpicks
the in-scope review already catches. An item that can't clear 🟢 doesn't go in the list.

If Phase 0b found a plan, give this agent the plan's text and a third job:
3. **Plan cross-reference.** For each item in lists 1 and 2, and for each finding the other
   agents reported, decide whether the plan already covers it (📋, quote the section or
   commit), whether it should be added (➕, say where), whether the plan is now stale or
   contradicted by the diff (⚠️, quote the claim and state what's now true), or whether the
   plan simply says nothing about it (no marker — the common case, and a fine answer).
   Also report **plan drift in the other direction**: acceptance criteria or test gates for
   this phase that the diff doesn't appear to meet. That's a finding, not a marker.
   Tell it explicitly: the plan is evidence, not authority. If the diff is right and the
   plan is wrong, that's ⚠️ on the plan — never a finding against the code.

Collect all agents' findings as they complete and dedup across agents (same root cause
reported by two lenses = one finding, keep the clearer write-up and the higher confidence).

## Phase 2 — Investigate and fix the 🔴 findings

Runs after every read-only agent has finished, because it edits files.

For each finding the agents scored >80, **verify before you touch anything**: open the
file, read the surrounding code and the relevant call sites, and confirm the failure
scenario actually holds. Agents are confident in ways that don't survive contact with the
real code.

- **Confirmed** → fix it. Keep the fix minimal and inside the diff's scope, match the
  surrounding code's style, and follow CLAUDE.md. It stays 🔴 and gets a checked box.
- **Doesn't hold up** → don't fix it. Downgrade it to 🟡 (or drop it) and say in the report
  what the agent missed. Never edit code to satisfy a finding you couldn't confirm.
- **Real but the fix is bigger than this commit** → leave the code alone, keep it 🔴 with an
  *unchecked* box, and say plainly why you didn't fix it and what it would take. A 🔴 you
  chose not to fix must be called out in the summary, not buried in the list. If the plan
  already schedules that work, mark it 📋 and name the commit — that's the marker earning
  its keep.

Do not fix 🟡 or 🟢 findings — they're report-only by definition. If a 🟡 investigation
turns up hard evidence that promotes it to 🔴, fix it and note the promotion.

A 📋 marker is not a reason to skip a fix. If a confirmed 🔴 is cheap and inside the diff,
fix it now even though the plan lists it for a later commit, and note that the plan's
sequence has moved.

After the fixes, rebuild and run the affected tests (per CLAUDE.md: Release build dir, tee
output to a log, QML GPU tests under `QSG_RHI_BACKEND=metal`). If a fix breaks a test,
revert that fix rather than piling on — report it as an unchecked 🔴 with what broke.

## Phase 3 — Simplify (applies in-scope quality fixes)

`simplify` edits files too, so it runs after Phase 2's fixes are in and green. Do the
simplify pass over the diff (reuse, simplification, efficiency, altitude) and **apply** the
in-scope quality fixes. You already have the parallel agents' findings — reuse them to drive
the fixes rather than re-fanning a redundant agent batch; only spin up fresh agents if the
simplify lenses genuinely weren't covered. Apply only behavior-preserving cleanups inside
the diff; anything flagged out-of-scope stays as a report note, not an edit. Rebuild and
re-run the affected tests after applying.

## Phase 4 — Consolidated report

Finish with one summary. Lead with the headline: how many 🔴 were found, how many you
fixed, and whether the build and tests are green. If any 🔴 is still open, name it by ID
in that headline — an unfixed red should never be something the user finds by scanning.

- **Key** — print the icon legend, every run, before the first checklist. Drop the plan
  line if no plan was found:

  ```
  🔴 fix now · 🟡 needs a fact confirmed · 🟢 nit    [x] fixed · [ ] open
  📋 already planned · ➕ add to plan · ⚠️ plan now stale
  IDs (F/R/A/P#) are for this report only — a re-run renumbers them.
  ```

  On the out-of-scope sections, where the icons change axis, re-state that axis in place
  rather than making the reader remember it: `🔴 urgent · 🟡 worth planning · 🟢 nice to have`.
- **Every line item gets a short ID** so the user can refer to it in one token ("fix F3",
  "R2 is already done"). Prefix by section, numbered from 1: `F#` review findings,
  `R#` out-of-scope refactors, `A#` architectural improvements, `P#` plan-needs-updating.
  Assign IDs **after** sorting, so F1 is the top 🔴 and the order matches the icons.
  IDs are per-report, not global — they don't carry across runs, and a later run will
  number differently. Say that once in the key so nobody treats F3 as a durable handle.
  If the user comes back mid-session and names an ID, resolve it against the report you
  just printed; if you've since re-run the review, ask which report they mean rather than
  guessing — the numbers will have moved.
- **Plan** — the plan file and the phase/commit the diff is executing, or "no governing
  plan found". One line.
- **Reviews run** — which lenses ran, which were skipped/scaled and why (e.g. "Model
  Contracts skipped: no model code"; "QML scaled to one test-quality agent: test-only").
- **Review findings** — one checklist, sorted 🔴 → 🟡 → 🟢. Plan marker after the
  `file:line`, only where it applies:

  ```
  - [x] **F1** 🔴 `cwLinePlotTask.cpp:212` — Worker thread reads m_chunks via non-const
        operator[], detaching a shared QList mid-scan. Fixed: switched to .at(i).
  - [ ] **F2** 🔴 `cwProject.cpp:88` 📋 Commit 8 — Save path can drop the last chunk when …
        Not fixed: the plan's "async scan pipeline" commit reworks this queue wholesale.
  - [ ] **F3** 🟡 `cwScrapManager.cpp:140` — Possible dangling context if the manager
        outlives … Settles it: whether any caller passes a non-parented context object.
  - [ ] **F4** 🟢 `cwSurvexExporterRule.h:30` — Magic number 4096 could be a k-constant.
  ```

  Checked box = fixed in the working tree. Empty box = still open. 🟡/🟢 are always
  unchecked, since they're report-only. Give the fix (or the reason there isn't one) in
  the line itself — don't make the user cross-reference a separate section.
- **Simplify** — what was applied, what was skipped, and the verification result.
- **📌 Out-of-scope refactors** — checklist, each line `**R#**`, then its 🔴/🟡/🟢 payoff icon,
  then `file:line`, then the plan marker where it applies, then a one-line why. All boxes
  unchecked — these are triage items, not work you did.

  ```
  - [ ] **R1** 🔴 ➕ `cwLinePlotManager.h:70` — Attach orchestrator duplicated across 3
        call sites; the next provider makes it 4. Not in the plan; belongs in Phase 3.
  - [ ] **R2** 🟡 📋 `cwSurvexExporterRule.cpp:44` — Already listed under "Not in this
        phase". No action needed.
  ```
- **🏛️ Architectural improvements (out of scope)** — same format with `**A#**`, plus rough
  blast radius per item.
- **⚠️ Plan needs updating** — only if any ⚠️ markers came up; number them `**P#**`. List
  what the plan claims and what's now true, so the user can edit it in one pass. You never
  edit the plan yourself here — that's Phase 5's job, and only if the user chooses it.

Print this whole report **before** the Phase 5 questions, so the full list with IDs sits
above the interactive prompts and the user can read every item while deciding.

## Phase 5 — Interactive triage (act on what wasn't auto-fixed)

The 🔴 findings are already fixed (Phase 2) and shown checked in the report. This phase walks
the items that still need a human call and asks the user what to do with each, right below the
report that named them.

**What gets a prompt:**

- Every **🟡 finding** and every out-of-scope **R#**/**A#** item gets one interactive question.
- **🔴 findings** are already auto-fixed — never prompt for them.
- **🟢 nits** do *not* each get a prompt. Ask **one** up-front toggle: "Also triage the N green
  nits, or skip them all?" (default: skip all). Only fan them into questions if the user opts in.
- **P#** plan-stale (⚠️) items ride the same triage — usually "Add to plan" (apply the
  correction) or "Skip".

**How to ask** — use `AskUserQuestion`, batching up to **4 items per call** (one question each)
so a cluster is triaged on one screen. Header = the item ID + icon (e.g. `F2 🟡`). Offer these
four options, with the one that fits the item marked `(Recommended)` and placed first:

| Option | Action |
|---|---|
| **Fix now** | Verify-then-fix exactly as Phase 2: open the file, confirm the failure scenario, make the minimal in-scope edit, follow CLAUDE.md. For an out-of-scope R#/A# item this may be bigger than the commit — do it if chosen, but say so. |
| **File GitHub issue** | Run `gh issue create` (title = the one-line defect; body = failure scenario + `file:line`). Capture the issue URL/number for the closing summary. Leave the code untouched. |
| **Add to plan** | Append the item to the governing `plans/` entry. Ask a short follow-up for **which section** (a later commit, or "Not in this phase"). This is the only sanctioned place the review edits a plan file. |
| **Skip** | Leave it as a report note only — no code change, no issue, no plan edit. |

Pick the `(Recommended)` default from the icon: a plausible 🟡 → **Fix now**; a 🟢 nit →
**Skip**; an out-of-scope refactor with no deadline → **Add to plan**; an urgent (🔴-axis)
out-of-scope item → **Fix now** or **File GitHub issue**.

The automatic **Other** slot covers the two things that aren't dispositions: **Explain**
("explain F2" → give the detail, then re-ask that one item) and a **guided fix** ("fix F2 but
use `.at()`" → apply the fix the user's way). Read the free text, do what it says, then re-prompt
the item if it still needs a decision.

**After triage**, rebuild and re-run the affected tests **once** (not per item), then print a
short **Triage actions** block so the record is complete — the report above it is now partly
stale. List: what you fixed, which issues you filed (numbers/URLs), which items you appended to
the plan (and where), and what was skipped.

Do not commit anything. Do not push. The only writes this review makes are the Phase 2/3 in-scope
fixes and whatever the user explicitly chose in Phase 5 (a fix, a filed issue, or a plan-file
append). Anything skipped stays a note for the user to revisit — don't act on it.
