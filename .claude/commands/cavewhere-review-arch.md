---
description: Architecture & out-of-scope review — plan cross-referencing plus the architectural/out-of-scope agent, no in-scope lint fan-out, never edits code
argument-hint: "[optional target: PR#, branch, or path — defaults to current changes]"
---

# CaveWhere architecture review

A read-only pass focused entirely on the **architectural & out-of-scope** side of
`cavewhere-review`: what this diff makes obvious but is bigger than this commit, what it reveals
about CaveWhere's architecture, and whether the governing plan (if any) is still accurate. It
never touches code — no lint fan-out, no lifetime/simplify agents, no auto-fix. Use
`cavewhere-review` or `cavewhere-review-lite` for in-scope correctness/cleanup; use this one when
you want the bigger-picture read on a diff without paying for the full fan-out.

The shape is: **scope the diff → find the governing plan → one architectural agent (plan-aware if
a plan was found) → one report → interactive triage.**

## Rating

Out-of-scope items are rated on payoff-vs-cost, not correctness confidence — "confidence" doesn't
apply to work that hasn't been done:

| Icon | Meaning |
|---|---|
| 🔴 | High payoff and the diff has already made it urgent — actively costing something now. Do it soon, in its own commit. |
| 🟡 | Real payoff, no deadline. Worth a `plans/` entry or an issue. |
| 🟢 | Nice to have. Note it and move on. |

Never auto-fix or edit code for anything found here, whatever its icon.

### Plan markers (second icon, only when a plan was found and it applies)

| Icon | Meaning | Action |
|---|---|---|
| 📋 | Already in the plan (a later commit, or "Not in this phase"). Name the section/commit. | None. |
| ➕ | Not in the plan and should be. | Report it; say which plan and roughly where. Don't edit the plan. |
| ⚠️ | Plan is now stale/contradicted by this diff. | Report it; say what the plan claims and what's now true. |

The marker is optional — most items get none. Never edit the plan file during the review; that's
Phase 3's job, and only when the user explicitly picks "Add to plan."

## Phase 0 — Scope

- If `$ARGUMENTS` is given, treat it as the target (PR#, branch, or path).
- Otherwise: `git diff @{upstream}...HEAD` plus `git diff HEAD` for uncommitted changes
  (fall back to `git diff master...HEAD` if `@{upstream}` is unset).

State the scope and file count. If the diff is empty, say so and stop.

## Phase 1 — Find the governing plan (skip fast if there isn't one)

CaveWhere keeps per-feature plans in `plans/` (`.md` and `.html`). Check, in order, stopping at
the first solid hit:

1. A modified/added file under `plans/` in the diff itself.
2. The branch name matched against plan filenames (pick the phase recent commits are actually
   in via `git log --oneline -15`, not phase 1 by reflex).
3. Recent commit subjects quoting the plan's commit-sequence entries.

If nothing matches within a couple of cheap commands, say "no governing plan found" and skip
plan markers entirely — don't force a weak match. If two plans plausibly apply, say so and pick
the more specific one.

When found, read it and pull out: the commit sequence, "Not in this phase" / scope-exclusion
sections, acceptance criteria and per-commit test gates, and any architecture/naming decisions
the diff might have superseded. Name the plan and phase/commit before moving on.

## Phase 2 — Architectural & out-of-scope agent

One read-only agent (no lint pass needed — this isn't a correctness review). It reads the diff
in the context of CaveWhere's architecture (consult `CLAUDE.md`) and reports two lists, never
changing code:

1. **Out-of-scope refactors** this change makes worthwhile/obvious but bigger than this commit —
   what, `file:line`, why worth it, why out of scope now.
2. **Major design changes** that would improve CaveWhere's architecture — leaking subsystem
   boundaries, a pattern worth generalizing across renderers/providers, an eroding front-end/
   back-end split, etc. — the change, the benefit, rough blast radius, and that it's out of
   scope now.

Have it propose a 🔴/🟡/🟢 payoff-vs-cost rating per item with a one-line justification. Hold a
HIGH bar — only genuine follow-ups, not nitpicks a correctness review would already catch. An
item that can't clear 🟢 doesn't go in the list.

If Phase 1 found a plan, give the agent the plan's text and a third job — **plan
cross-reference**: for each item in lists 1 and 2, decide 📋/➕/⚠️/no-marker as above. Also report
plan drift the other direction: acceptance criteria or test gates for this phase the diff doesn't
appear to meet — that's a finding, not a marker. The plan is evidence, not authority: if the diff
is right and the plan is wrong, that's ⚠️ on the plan, never a finding against the code.

## Phase 3 — Report

```
🔴 urgent · 🟡 worth planning · 🟢 nice to have
📋 already planned · ➕ add to plan · ⚠️ plan now stale
IDs (R#/A#/P#) are for this report only — a re-run renumbers them.
```

- **Plan** — the plan file and phase/commit, or "no governing plan found."
- **📌 Out-of-scope refactors** — checklist, `**R#**`, then icon, `file:line`, plan marker where
  it applies, one-line why. All boxes unchecked — triage items, not work done.
  ```
  - [ ] **R1** 🔴 ➕ `cwLinePlotManager.h:70` — Attach orchestrator duplicated across 3 call
        sites; the next provider makes it 4. Not in the plan; belongs in Phase 3.
  ```
- **🏛️ Architectural improvements** — same format with `**A#**`, plus rough blast radius.
- **⚠️ Plan needs updating** — only if any ⚠️ markers came up; `**P#**`, what the plan claims vs.
  what's now true.

## Phase 4 — Interactive triage

Every R#/A#/P# item gets one interactive question via `AskUserQuestion`, batched up to 4 per
call. Header = item ID + icon. Four options, the fitting one marked `(Recommended)` and first:

| Option | Action |
|---|---|
| **Fix now** | Implement it — may be bigger than a typical commit; say so if chosen. |
| **File GitHub issue** | `gh issue create` (title = one-line description; body = detail + `file:line`). Capture the issue URL for the closing summary. Leave code untouched. |
| **Add to plan** | Append to the governing `plans/` entry. Ask which section (a later commit, or "Not in this phase"). Only sanctioned place this review edits a plan file. |
| **Skip** | Report note only. |

Default: urgent (🔴-axis) items → **Fix now** or **File GitHub issue**; no-deadline 🟡 → **Add to
plan**; 🟢 → **Skip**. The **Other** slot covers "explain R2" (give detail, re-ask) and a guided
disposition ("file issue for A1 but tag it perf").

After triage, print a short **Triage actions** block: what you fixed (and whether you rebuilt/
tested it), which issues you filed, which items you appended to the plan and where, what was
skipped.

Do not commit or push. The only writes this review makes are whatever the user explicitly chose
in Phase 4 (a fix, a filed issue, or a plan-file append).
