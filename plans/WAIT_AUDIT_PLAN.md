# Audit and replace bare `wait()` calls in cavewhere-qml-test

## Context

The nightly CI runs 4 `cavewhere-test` + 2 `cavewhere-qml-test` concurrently under `--platform offscreen` in Release mode. Bare `wait(ms)` calls are inherently flaky — they assume a fixed time budget that may be too short under heavy load or too long for fast runs. The CLAUDE.md convention is: "Prefer `tryVerify()` / `tryCompare()` over `wait()`. Fixed waits are flaky — poll for the condition you actually need."

**Scope:** 182 bare `wait()` calls across 24 files (23 `.qml` + 1 `.js`).

## Categories of wait() usage

After auditing every occurrence, the waits fall into 5 patterns:

### Category A: "Wait after navigation/click for UI to settle" (~90 calls)
**Pattern:** `mouseClick(button); wait(100)` or `pageAddress = "..."; wait(100)`
**Fix:** Replace with `tryVerify()` checking the expected effect of the action — e.g., object existence, visibility change, page item objectName, model count change. Every click should have a verifiable consequence.
**Files:** tst_KeywordTab (22), tst_SurveyDataEntry (several), tst_Leads (7), tst_ImportCSVData (several), tst_WelcomePage (4), tst_DiscardReload (2), tst_ScrapInteraction (several)

### Category B: "Wait between mouse interaction steps" (~40 calls)
**Pattern:** `mousePress(); wait(50); mouseMove(); wait(50); mouseRelease()`
**Fix:** Replace with `tryVerify()` checking the effect of the interaction — e.g., a property changed, an interaction became active, a position updated. If no observable state change exists between micro-steps, the wait can simply be removed (Qt processes mouse events synchronously in test mode).
**Files:** tst_Map (19), tst_MapMultiLayer (14), tst_CameraOptions (7), tst_NoteNorthInteraction (5), tst_TurntableInteraction (1), tst_ScrapInteraction (several)

### Category C: "Yield for event loop / 1ms waits" (~5 calls)
**Pattern:** `wait(1)` — minimal yield to process pending events
**Fix:** Replace with `tryVerify()` on the condition the code is actually waiting for (e.g., delegate loaded, cell available). If the wait is just yielding for an object finder, fold it into the existing `tryVerify` that follows.
**Files:** tst_SurveyDataEntry (lines 1854, 1868, 2039)

### Category D: "Wait after positionViewAtIndex for delegates to load" (~10 calls)
**Pattern:** `view.positionViewAtIndex(row, ...); wait(50); let cell = ObjectFinder.find(...)`
**Fix:** Replace with `tryVerify()` that the target delegate object exists (it's created asynchronously by ListView).
**Files:** tst_SurveyDataEntry (lines 279, 2308, 2326, 2350, 2363, 2384, 2428)

### Category E: "Cleanup/setup waits and wait(0)" (~5 calls)
**Pattern:** `obj.destroy(); wait(0)` or `newProject(); wait(100)`
**Fix:** `wait(0)` can be removed (it's a no-op). Setup waits like `newProject(); wait(100)` should use `tryVerify()` on the expected post-state (e.g., page loaded, model reset).
**Files:** tst_GitImageComparePage (1), tst_GitWorkingTreePanel (1), tst_SurveyDataEntry (line 18)

### IMPORTANT: Do NOT use `waitForRendering()`
`waitForRendering()` can hang indefinitely under `--platform offscreen` when no frame is produced. **Always use `tryVerify()` with a concrete, observable condition instead.** Every action in a test should have a verifiable effect — if it doesn't, question whether the wait is needed at all.

### Exception: ListView delegate recycling
When a ListView model changes (e.g., removing an OR boundary in KeywordTab), the delegates are recycled asynchronously. `ObjectFinder` can return stale delegate references during this period, and clicks on stale refs are silently dropped. In these cases, a `wait()` is the only reliable approach — `tryVerify` finds the item immediately (it hasn't been destroyed yet) but the reference is stale. Mark these with a comment explaining why `wait()` is needed.

### Commented-out waits (DO NOT touch)
There are ~15 commented-out `// wait(1000000)` calls that serve as debugging breakpoints. Leave these as-is.

## Implementation plan — ordered by file, highest impact first

### Priority 1: Files with nightly failures or highest count

| File | Count | Primary fix pattern |
|------|-------|-------------------|
| `tst_SurveyDataEntry.qml` | 30 | Categories A, C, D — `tryVerify` for cell/delegate existence |
| `tst_KeywordTab.qml` | 22 | Category A — `tryVerify` for UI element existence after click |
| `tst_Map.qml` | 19 | Category B — `tryVerify` or remove between mouse steps |
| `tst_ScrapInteraction.qml` | 18 | Categories A, B — `tryVerify` for state changes |
| `tst_MapMultiLayer.qml` | 14 | Category B — `tryVerify` for layer/visibility changes |
| `tst_LiDARNotes.qml` | 15 | Categories A, B — `tryVerify` for note/image state |

### Priority 2: Medium count files

| File | Count | Primary fix pattern |
|------|-------|-------------------|
| `tst_ImportCSVData.qml` | 9 | Category A — `tryVerify` for model/column changes after drag |
| `tst_Leads.qml` | 7 | Category A — `tryVerify` for page state |
| `tst_CameraOptions.qml` | 7 | Category B — `tryVerify` or remove between interactions |
| `tst_TripSync.qml` | 6 | Category A — `tryVerify` for sync state |
| `tst_ScrapSync.qml` | 6 | Category A — `tryVerify` for sync state |
| `tst_NoteNorthInteraction.qml` | 5 | Category B — `tryVerify` for rotation/position |
| `tst_NoteZeroDPI.qml` | 5 | Category A — `tryCompare` for text values |

### Priority 3: Low count files (1-4 each)

| File | Count |
|------|-------|
| `tst_LidarSync.qml` | 4 |
| `tst_WelcomePage.qml` | 4 |
| `tst_DiscardReload.qml` | 2 |
| `tst_NoteScaleInteraction.qml` | 2 |
| `tst_TurntableInteraction.qml` | 1 |
| `tst_GitWorkingTreePanel.qml` | 1 |
| `tst_GitImageComparePage.qml` | 1 |
| `tst_GitFileDiffPage.qml` | 1 |
| `tst_ScrapSelectionReset.qml` | 1 |
| `tst_QuoteBox.qml` | 1 |
| `SyncTestHelper.js` | 1 |

### SyncTestHelper.js special case
The `wait(50)` on line 76 is inside `tryVerifyWithDiagnostics()` — a custom polling loop. This is already polling behavior (not a bare fixed wait), so it's **low priority** and can be left as-is. The 50ms sleep inside a retry loop is the expected pattern.

## Replacement rules (quick reference)

**Never use `waitForRendering()` — it can hang under offscreen. Always use `tryVerify()` with a concrete condition.**

| Old pattern | New pattern |
|-------------|-------------|
| `mouseClick(x); wait(N); let y = find(...)` | `mouseClick(x); tryVerify(() => find(...) !== null)` — verify the click's effect |
| `mouseClick(tab); wait(N)` | `mouseClick(tab); tryVerify(() => expectedPanel !== null)` — verify tab content loaded |
| `mouseDrag(...); wait(N)` | `mouseDrag(...); tryVerify(() => property changed)` |
| `mousePress/Move/Release; wait(50)` | Remove the wait if no observable state change; otherwise `tryVerify(condition)` |
| `view.positionViewAtIndex(...); wait(N); let cell = find(...)` | `view.positionViewAtIndex(...); tryVerify(() => find(...) !== null)` |
| `wait(1)` | Fold into the nearest `tryVerify` or remove if followed by one |
| `wait(0)` | Remove entirely |
| `pageAddress = "X"; wait(N)` | `pageAddress = "X"; tryVerify(() => currentPageItem.objectName === "expected")` |

## Verification

After each file is modified, run it 10 times with `--platform offscreen` in Release mode:
```bash
for i in $(seq 1 10); do ./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_FILE.qml && echo "Run $i: PASS" || echo "Run $i: FAIL"; done
```

## Completed files

### tst_KeywordTab.qml — 22 → 7 wait() (15 replaced)
- All Category A waits (after mouseClick for UI elements) replaced with tryVerify
- 7 waits retained: all involve ListView delegate recycling where recycled delegates have valid refs but stale bound properties (sourceRow, index), causing button clicks to act on wrong model rows
- Investigated ObjectFinder window() check and ListView.onPooled/onReused — neither solves the stale-binding issue
- Each retained wait() has a comment explaining why

### tst_SurveyDataEntry.qml — 15 → 0 wait() (15 replaced)
- Category A: wait after openContextMenu removed (triggerMenuItemFromLoader now uses tryVerify for loader); wait after blocked insert → tryCompare; focus hack wait → tryVerify visible
- Category C: 3x wait(1) removed (redundant before existing tryVerify or no async dependency)
- Category D: wait after positionViewAtBeginning → tryVerify(visible); wait after positionViewAtIndex removed (followed by tryVerify); scrollToBottom wait → tryVerify cell exists
- Category E: init() wait → tryVerify page loaded
- dataBoxAt() manual 50-iteration polling loop simplified to single tryVerify
- 6x triggerMenuItemFromLoader + 1x menuItemEnabled updated to tryVerify for async Loader
- Verified 10/10 passes

## Execution approach

Work through files in priority order. For each file:
1. Replace all `wait()` calls using the replacement rules above
2. Verify the test still passes 10/10
3. Commit per-file or per-batch (user preference)

Skip `SyncTestHelper.js` line 76 (already a polling loop).
Skip all commented-out `// wait(...)` lines.
