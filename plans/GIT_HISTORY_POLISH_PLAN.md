# Git History Polish & Bug Fixes

Post-implementation issues found during testing of the Git Commit Detail Panel (see `GIT_COMMIT_DETAIL_PLAN.md`).

---

## Issues

### 1. ~~HEAD indicator in history view~~ ✅
~~The history view needs a visual indicator showing which commit is currently checked out (HEAD). Consider bolding the commit message or adding an icon/badge. Should be understandable to non-git users.~~

**Independent** — Done: Added IsHeadRole to GitGraphModel, "Current" RefBadge, and bold commit message for HEAD.

### 2. Default to main branch
History should default to showing the main branch when the page is opened.

**Independent**

### 3. History doesn't update after sync
Pressing sync (pull or push) doesn't refresh the git history view. The new commits don't appear until navigating away and back.

**Independent — but fix before verifying 5 and 13**

### 4. Remove SHA copy button, make metadata selectable
Remove the dedicated SHA copy action. Make author, date, and SHA text selectable so native right-click copy works.

**Independent**

### 5. Working tree not updated on file change
Changing a file in the project doesn't cause the history to show uncommitted changes. The synthetic "Uncommitted Changes" row doesn't appear or update in response to file modifications.

**Depends on 3** — both relate to the history view not reacting to repo state changes. May share a common fix (e.g., watching for ref/index changes).

### 6. Add discard button
Add a discard-all-changes button to the working tree panel. Must be heavily guarded with a confirmation dialog to prevent accidental data loss.

**Independent**

### 7. LinkBar navigation broken
History has a broken link setup in the LinkBar. The breadcrumb should be History -> Diff when viewing a file diff. Most other page links from within the history flow don't work.

**Independent**

### 8. Image compare broken
`image://gitcommit/` provider fails with "Invalid image provider". The `-1` parent index in the URL looks wrong.

Error:
```
Invalid image provider: image://gitcommit/-1/4b01cae.../path/to/image.jpg
Invalid image provider: image://gitcommit/-1/9cfe853.../path/to/image.jpg
```

**Independent**

### 9. Copy/open file path
Add ability to copy the file path or open the file/asset from the commit detail file list.

**Independent**

### 10. QML tests for GitHistoryPage
Need test coverage for the GitHistoryPage and its subcomponents.

**Independent** — can be written in parallel with bug fixes

### 11. Selected commit needs more padding
Visual spacing issue on the selected commit row in the history list.

**Independent**

### 12. Merge tail rendering
Merges show dangling tails when they are the last commit visible in the git history graph.

**Independent**

### 13. .cw bundle history
Make sure history shows correctly for bundled `.cw` archives.

**Depends on 3** — likely the same root cause as history not updating after sync. Fix 3 first, then verify.

---

## Dependency Graph

```
  3 (history refresh)
  ├── 5 (working tree update)
  └── 13 (.cw bundle history)

  All others are independent:
  1, 2, 4, 6, 7, 8, 9, 10, 11, 12
```

## Parallel Work Streams

Stream A: **History refresh** — 3, then verify 5 and 13
Stream B: **History view visuals** — 1, 2, 11, 12
Stream C: **Commit detail panel** — 4, 6, 9
Stream D: **Navigation & pages** — 7, 8
Stream E: **Testing** — 10
