# Git History Polish & Bug Fixes

Post-implementation issues found during testing of the Git Commit Detail Panel (see `GIT_COMMIT_DETAIL_PLAN.md`).

---

## Issues

### 1. ~~HEAD indicator in history view~~ ✅
~~The history view needs a visual indicator showing which commit is currently checked out (HEAD). Consider bolding the commit message or adding an icon/badge. Should be understandable to non-git users.~~

**Independent** — Done: Added IsHeadRole to GitGraphModel, "Current" RefBadge, and bold commit message for HEAD.

### 2. ~~Default to main branch~~ ✅
~~History should default to showing the main branch when the page is opened.~~

**Independent** — Done: `initRepository()` now uses `git_repository_init_ext` with `initial_head = "main"`.

### 3. ~~History doesn't update after sync~~ ✅
~~Pressing sync (pull or push) doesn't refresh the git history view. The new commits don't appear until navigating away and back.~~

**Independent — but fix before verifying 5 and 13** — Done: Added `refsChanged()` signal to `GitRepository`, emitted from `commitAll`, `resetHard`, `createBranch`, `push`, `pull`, `pullRebaseOrMerge`, `checkout`, `reset`. `GitGraphModel` connects to it for auto-refresh.

### 4. ~~Remove SHA copy button, make metadata selectable~~ ✅
~~Remove the dedicated SHA copy action. Make author, date, and SHA text selectable so native right-click copy works.~~

**Independent** — Done: Replaced copy button with full SHA in a selectable `TextEdit`. Author and date are also now selectable `TextEdit` fields.

### 5. ~~Working tree not updated on file change~~ ✅
~~Changing a file in the project doesn't cause the history to show uncommitted changes. The synthetic "Uncommitted Changes" row doesn't appear or update in response to file modifications.~~

**Depends on 3** — Done: Connected `refsChanged` → `checkStatusAsync()` in `GitRepository` constructor so modified file count auto-updates after any ref-changing operation. The synthetic row now appears/disappears without explicit `checkStatus()` calls.

### 6. ~~Add discard button~~ ✅
~~Add a discard-all-changes button to the working tree panel. Must be heavily guarded with a confirmation dialog to prevent accidental data loss.~~

**Independent** — Done: Added "Discard All" button with modal confirmation dialog. Calls `resetHard("HEAD")` + `cleanUntracked()` + `checkStatus()`.

### 7. ~~LinkBar navigation broken~~ ✅
~~History has a broken link setup in the LinkBar. The breadcrumb should be History -> Diff when viewing a file diff. Most other page links from within the history flow don't work.~~

**Independent** — Done: Made `fullPathRole` non-required in `LinkBarItem` (fixes delegate creation under `ComponentBehavior: Bound`), added `required property string fullPathRole` to the `LinkBar` delegate (enables model role injection), and simplified diff page names to "Diff" for clean breadcrumbs.

### 8. ~~Image compare broken~~ ✅
~~`image://gitcommit/` provider fails with "Invalid image provider". The `-1` parent index in the URL looks wrong.~~

**Independent** — Done: The `-1` was `imageProviderRepoId` never being set. Added `imageProviderId` Q_PROPERTY to `GitRepository` that auto-registers with `GitCommitImageProvider` when directory changes. `GitImageComparePage` now reads `repository.imageProviderId` directly.

### 9. Copy/open file path
Add ability to copy the file path or open the file/asset from the commit detail file list.

**Independent**

### 10. QML tests for GitHistoryPage
Need test coverage for the GitHistoryPage and its subcomponents.

**Independent** — can be written in parallel with bug fixes

### 11. ~~Selected commit needs more padding~~ ✅
~~Visual spacing issue on the selected commit row in the history list.~~

**Independent** — Done: Increased row height from 28→32px and added 4px horizontal margins to the RowLayout in GitHistoryRow.qml.

### 12. Merge tail rendering
Merges show dangling tails when they are the last commit visible in the git history graph.

**Independent**

### 13. ~~.cw bundle history~~ ✅
~~Make sure history shows correctly for bundled `.cw` archives.~~

**Depends on 3** — Verified: works correctly after fix 3.

### 14. Uncommitted Changes lane visualization
The synthetic "Uncommitted Changes" row copies the lanes from the HEAD commit verbatim. The lanes don't terminate, and the result looks wrong — especially when HEAD is a merge commit with multiple active lanes. The synthetic row should have its own lane rendering that cleanly connects to the HEAD commit's active lane without dangling tails from other branches.

**Independent**

### 15. Debounce checkStatusAsync on saveFlushCompleted
Rapid-fire `saveFlushCompleted` signals (e.g., editing multiple scraps quickly) each trigger a full `checkStatusAsync()` call — opening a new `git_repository`, running `git_status_list`, and filtering LFS entries on the thread pool. Use `AsyncFuture::Restarter` to coalesce rapid calls — it already has event-loop debouncing built in (queues the first start via `Qt::QueuedConnection`, subsequent `restart()` calls in the same event loop cycle just update the run function without re-queuing). This would collapse N rapid signals into a single status scan without needing a QML Timer.

**Independent**

### 16. Extract shared git status helper
`checkStatus()` and `checkStatusAsync()` in `GitRepository.cpp` duplicate the same status-options setup and `filteredStatusEntryCount` logic. Extract a shared helper that builds the status list and returns the filtered count, then have both functions delegate to it.

**Independent**

### 14. Download missing LFS objects for image diff
When viewing an image diff, the before image may be unavailable because LFS only downloads objects for the currently checked-out files. Add a download button to the "Previous version unavailable" placeholder so users can fetch the missing LFS object on demand. Needs LFS server auth, progress indication, and error handling.

**Independent**

---

## Dependency Graph

```
  3 (history refresh)
  ├── 5 (working tree update)
  └── 13 (.cw bundle history)

  All others are independent:
  1, 2, 4, 6, 7, 8, 9, 10, 11, 12, 14
```

## Parallel Work Streams

Stream A: **History refresh** — 3, then verify 5 and 13
Stream B: **History view visuals** — 1, 2, 11, 12
Stream C: **Commit detail panel** — 4, 6, 9
Stream D: **Navigation & pages** — 7, 8, 14
Stream E: **Testing** — 10
