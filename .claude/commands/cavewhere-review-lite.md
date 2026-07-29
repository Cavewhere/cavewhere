---
description: Lightweight pre-commit review — memory/lifetime check plus a simplify pass, no full lens fan-out, no plan cross-referencing
argument-hint: "[optional target: PR#, branch, or path — defaults to current changes]"
---

# CaveWhere lite review

A fast, cheap pass for everyday commits: skip the full `cavewhere-review` fan-out (six C++
lenses, six QML lenses, the architectural agent, plan cross-referencing) and run only two
things — a **memory & lifetime** check and a **simplify** pass. Use `cavewhere-review` instead
when the change is substantial (new subsystem, many files, touches threading/ownership deeply)
or you want the plan-aware, out-of-scope-tracking report.

The shape is: **scope the diff → lint (cheap) → one small parallel batch (lifetime + simplify
scan) → auto-fix confirmed 🔴 lifetime findings → apply simplify → one short report.**

## Rating

Same three-tier confidence rating as the full review, used only for the lifetime lens (simplify
findings are applied directly, not rated):

| Icon | Confidence | Action |
|---|---|---|
| 🔴 | >80 — real defect, concrete failure scenario | Investigate and fix now |
| 🟡 | 60–79 — plausible, depends on an unconfirmed fact | Report only, one line, say what settles it |
| 🟢 | <60 — nit | Report only, suppress if noise |

No plan markers, no out-of-scope/architectural section, no `R#`/`A#`/`P#` axes — this mode
doesn't run those agents. If you want that coverage, use `cavewhere-review`.

## Phase 0 — Scope

- If `$ARGUMENTS` is given, treat it as the target (PR#, branch, or path).
- Otherwise: `git diff @{upstream}...HEAD` plus `git diff HEAD` for uncommitted changes
  (fall back to `git diff master...HEAD` if `@{upstream}` is unset).

Classify C++ (`*.cpp/.cc/.cxx/.h/.hpp/.mm`) vs QML (`*.qml`) changed. State the scope and file
count. If the diff is empty, say so and stop. No plan lookup in lite mode.

## Phase 1 — Lint, then one small parallel batch

**1a. Lint (inline):** run the same bundled linters as the full review when the relevant
language changed (`qt_review_lint.py` for C++, `qt_qml_lint.py` for QML — glob the installed
plugin path, don't hard-code the version). Feed the output to the agents below so they don't
re-report mechanical findings.

**1b. Launch in one message, read-only:**

- **Memory & lifetime agent** — always, if C++ or QML changed. This is the `qt-cpp-review`
  skill's Ownership & Lifecycle domain, scoped to the diff (report only on changed lines, read
  ±50 lines for context, never edit). Look specifically for: dangling pointers/references,
  use-after-free, missing/incorrect parent ownership (`QObject` parenting, `deleteLater`
  timing), lifetime mismatches across thread boundaries, `AsyncFuture` callbacks capturing
  `this` without a context object, raw-pointer ownership that should be a smart pointer or
  `QObject` parent, and — for QML — `Loader`/`Component.destruction`, dangling `id` references
  across reloads, and JS closures over C++ objects that can outlive them. Weave in the CLAUDE.md
  rules that bear on lifetime: `.at()`/const-ref over non-const `operator[]`, no
  `std::shared_ptr<QList/QVector>`, `AsyncFuture::observe(...).context(this, ...)` not
  `waitForFinished` in production. Return each finding as `file:line`, a one-sentence defect
  statement, a concrete failure scenario, a confidence score 0–100, and — under 80 — the fact
  that would settle it. Empty list is a fine result.
- **Simplify scan agent** — always. Same mission as the `simplify` skill (reuse, simplification,
  efficiency, altitude cleanups) but scoped to just this diff's changed lines; read-only for now,
  Phase 3 applies its findings.

Two agents total (one if only one language changed and the simplify scan covers both — keep the
simplify scan as one agent regardless of language mix). Do not add the other C++/QML lenses or
the architectural agent — that's what `cavewhere-review` is for.

## Phase 2 — Investigate and fix 🔴 lifetime findings

For each finding scored >80: open the file, read the surrounding code and call sites, confirm
the failure scenario actually holds before touching anything.

- **Confirmed** → fix it, minimal and in-scope, matching surrounding style and CLAUDE.md. Stays
  🔴, checked box.
- **Doesn't hold up** → don't fix it; downgrade to 🟡 or drop it, say what the agent missed.
- **Real but bigger than this commit** → leave it, keep 🔴 unchecked, say why and what it'd take.

Don't fix 🟡/🟢 — report only, unless investigation turns up hard evidence that promotes a 🟡 to
🔴 (fix it, note the promotion).

Rebuild and run the affected tests after fixing (Release dir, tee to a log).

## Phase 3 — Simplify

Apply the simplify scan's in-scope findings (reuse Phase 1b's results — don't re-fan a redundant
agent). Behavior-preserving cleanups only, inside the diff. Rebuild and re-run affected tests
after applying.

## Phase 4 — Report

One short summary, no plan section, no out-of-scope section:

```
🔴 fix now · 🟡 needs a fact confirmed · 🟢 nit    [x] fixed · [ ] open
IDs (F#) are for this report only.
```

- **Scope** — files reviewed, C++/QML detected.
- **Memory & lifetime findings** — checklist, sorted 🔴 → 🟡 → 🟢:
  ```
  - [x] **F1** 🔴 `cwScrapManager.cpp:88` — Context object destroyed before the future
        resolves, callback touches freed `this`. Fixed: added .context(this, ...).
  - [ ] **F2** 🟡 `cwNote.cpp:140` — Possible dangling parent if caller doesn't own the
        note. Settles it: whether any non-owning caller exists.
  ```
- **Simplify** — what was applied, what was skipped, verification result.

If any 🔴 is still open, name it in the headline. If there are 🟡 findings, ask the user once
(batched, `AskUserQuestion`, up to 4 per call) whether to fix now, file a GitHub issue, or skip —
same dispositions as the full review's Phase 5, just without the plan-append option (lite mode
tracks no plan). 🟢 nits are report-only, no prompt.

Do not commit or push. Only writes are the Phase 2/3 in-scope fixes and whatever the user
explicitly chooses for a 🟡 finding.
