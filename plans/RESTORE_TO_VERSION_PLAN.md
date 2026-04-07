# Restore to Version — Implementation Plan

## Context

Users can see CaveWhere's sync history (git commits) in the `GitHistoryPage` but have no way to roll back. This feature adds a "Restore to here" action (via right-click context menu on a commit) that creates a **new forward commit** whose tree matches a historical commit's tree. All history is preserved — nothing is rewritten — so the operation is safe for collaboration/sync (no force push needed).

**Git result:**
```
A → B → C → D → E (HEAD: "Restored to: B's subject")
```
where E's tree = B's tree, E's parent = D.

---

## Implementation

### 1. `GitRepository::restoreToCommit()` — pure git operation

**Files:** `QQuickGit/src/GitRepository.h`, `QQuickGit/src/GitRepository.cpp`

New async method following the `checkout()`/`reset()` pattern (`GitRepository.cpp:3961-4007`):

```cpp
Q_INVOKABLE GitFuture restoreToCommit(const QString& targetSha);
```

**libgit2 steps** (on background thread via `progressFuture` + `QtConcurrent::run`):
1. Open repo from path (thread-safe pattern — own `git_repository*`)
2. Look up target commit via `git_oid_fromstr` + `git_commit_lookup`
3. Get target's tree via `git_commit_tree`
4. Look up HEAD as parent via `git_revparse_ext(&parent, &ref, repo, "HEAD")`
5. Build commit message via helper function `buildRestoreCommitMessage()` (see below)
6. Create signature via `git_signature_now` (capture account name/email on main thread before dispatching, like `commitAll` at line 2855)
7. Create forward commit: `git_commit_create_v(&oid, repo, "HEAD", sig, sig, NULL, message, targetTree, 1, parent)`
8. Force-checkout target tree to working dir: `git_checkout_tree(repo, targetTree, &opts)` with `GIT_CHECKOUT_FORCE | GIT_CHECKOUT_REMOVE_UNTRACKED`
9. Update index: `git_index_read_tree(index, targetTree)` + `git_index_write(index)`
10. `buildLfsHydrationPlan(repo)` → `runLfsHydrationPipeline()` for LFS files
11. `emitRefsChangedOnSuccess(future)`

**Note:** Skip the `treeMatchesParent` early-return that `commitAll` does (line 2847). If the user explicitly restores to a commit whose tree matches HEAD, that's intentional.

#### `buildRestoreCommitMessage()` — static helper

Builds the commit message with audit trail. Extracted as a helper to keep `restoreToCommit()` readable.

**Subject:** `"Restored to: " + git_commit_summary(targetCommit)`

**Body:** Walk from HEAD to target using `git_revwalk` to enumerate rolled-back commits:
```
Target: abc123def4 — "Add new cave survey" (2026-03-15 14:32)
Previous HEAD: def456789a — "Fix station naming" (2026-04-01 09:15)

Rolled back 3 commits:
  def456789a Fix station naming
  890abcdef1 Rename Cave B entrance
  567890abcd Add second trip to Cave A
```

- Cap the commit list at 10 entries; if more, append `... and N more`
- Use `git_revwalk` from HEAD, hiding target with `git_revwalk_hide`
- **Edge case:** If target is not a direct ancestor of HEAD (e.g. merged branch), the revwalk may not terminate cleanly. Guard with `git_graph_descendant_of(repo, headOid, targetOid)`. If target is not an ancestor, skip the "Rolled back N commits" section and only include the Target/Previous HEAD lines.

### 2. `cwSaveLoad::restoreToCommitAndReconcile()` — flush/restore/reconcile pipeline

**Files:** `cavewherelib/src/cwSaveLoad.h`, `cavewherelib/src/cwSaveLoad.cpp`

New method modeled directly on `resetBranchAndReconcile()` (`cwSaveLoad.cpp:6063-6224`). Same structure:

```cpp
QFuture<Monad::ResultBase> restoreToCommitAndReconcile(const QString& targetSha);
```

**Pipeline stages** (identical to `resetBranchAndReconcile` but swapping `repo->reset()` for `repo->restoreToCommit()`):

1. **Validation** — git mode, sync enabled, project filename, repo available (lines 6067-6087 pattern)
2. **SaveFlush** — `d->enqueueOperation(... saveFlushImpl())` (line 6093)
3. **Restore prepare** — `d->enqueueOperation(... SyncProject ...)`:
   - Verify saveFlush succeeded
   - `repo->checkStatus()` — working dir must be clean after flush
   - Capture `beforeHead` via `headCommitOid(repoPath)`
   - Capture LFS snapshot via `captureLfsSnapshot(repoPath)`
   - Call `repo->restoreToCommit(targetSha)` <-- the only line that differs from `resetBranchAndReconcile`
   - On completion: `updateFileNameFromSingleCwproj()`, `loadProject()`, capture `afterHead`, `buildSyncReport()`
4. **Reconcile** — `enqueueReconcilePhase(... ReconcileApplyMode::TargetCommitWins)` (line 6201)
5. **Finalize** — `enqueueFinalizePhase(... FinalizeMode::CheckoutLocal)` (line 6205)

**Code reuse:** Do NOT duplicate `resetBranchAndReconcile`. Refactor the shared pipeline (validation → saveFlush → prepare → reconcile → finalize) into a common helper that accepts a `std::function` for the git operation. Both `resetBranchAndReconcile` and `restoreToCommitAndReconcile` call this helper, passing `repo->reset(refSpec, mode)` or `repo->restoreToCommit(targetSha)` respectively.

### 3. `cwProject::restoreToCommit()` — QML-facing wrapper

**Files:** `cavewherelib/src/cwProject.h`, `cavewherelib/src/cwProject.cpp`

```cpp
Q_INVOKABLE bool restoreToCommit(const QString& targetSha);
```

Implementation follows `resetBranchAndReconcile` exactly (cwProject.cpp:517-526):
```cpp
bool cwProject::restoreToCommit(const QString& targetSha)
{
    if (!m_saveLoad || syncInProgress()) {
        return false;
    }
    if (emitVersionGuardError(QStringLiteral("restore"))) { return false; }
    return beginSyncOperation(m_saveLoad->restoreToCommitAndReconcile(targetSha));
}
```

Errors route through `completeSyncOperation()` → `ErrorModel` automatically.

### 4. QML — Right-click context menu on `GitHistoryRow`

**File:** `QQuickGit/qml/GitHistoryRow.qml`

Add a right-click context menu to each commit row. The context menu emits a signal that bubbles up to GitHistoryView and then to GitHistoryPage.

**Signal chain:**
- `GitHistoryRow` → `restoreRequested(string sha, string subject)` signal
- `GitHistoryView` → forwards signal from delegate
- `GitHistoryPage` → connects to signal, opens confirmation dialog

The context menu item:
```qml
QC.Menu {
    id: contextMenu
    QC.MenuItem {
        text: qsTr("Restore to here")
        enabled: !root.isHead
        onTriggered: root.restoreRequested(root.commitSha, root.message)
    }
}

TapHandler {
    acceptedButtons: Qt.RightButton
    onTapped: contextMenu.popup()
}
```

**Disabled states:** The menu item is disabled (greyed out) when:
- The commit is HEAD (`isHead` role from `GitGraphModel`)
- The commit is the synthetic "Uncommitted Changes" row (`sha === ""`)

Note: `syncInProgress` disabling is handled at the `GitHistoryPage` level — the `onAccepted` handler in the confirmation dialog checks before calling `restoreToCommit()`.

### 5. QML — Dirty working directory handling + confirmation dialog

**File:** `cavewherelib/qml/GitHistoryPage.qml`

Both dialogs live at the `StandardPage` level (not inside the Loader) so they persist across panel swaps.

**Flow when user right-clicks → "Restore to here":**

1. If `RootData.project.modified` is true → open `AskToSaveDialog` with `taskName: "restoring"`. This shows the existing save/discard/cancel dialog ("Save before restoring?").
   - **Save:** saves the project, then on completion opens the restore confirmation dialog
   - **Discard:** discards changes, then on completion opens the restore confirmation dialog
   - **Cancel:** aborts the restore entirely
2. If working directory is clean → open the restore confirmation dialog directly

```qml
// Store restore target while dialogs are open
property string _pendingRestoreSha: ""
property string _pendingRestoreSubject: ""

function requestRestore(sha, subject) {
    _pendingRestoreSha = sha
    _pendingRestoreSubject = subject
    if (RootData.project.modified) {
        askToSaveForRestore.open()
    } else {
        restoreConfirmDialog.open()
    }
}
```

Wire the signal from `GitHistoryView`:
```qml
QG.GitHistoryView {
    id: historyView
    // ... existing properties ...

    onRestoreRequested: (sha, subject) => {
        page.requestRestore(sha, subject)
    }
}
```

**AskToSaveDialog** (reuse existing component with `taskName: "restoring"`):
```qml
AskToSaveDialog {
    id: askToSaveForRestore
    taskName: "restoring"
    onCompleted: restoreConfirmDialog.open()  // proceed after save or discard
}
```

**Restore confirmation dialog:**
```qml
QC.Dialog {
    id: restoreConfirmDialog
    anchors.centerIn: parent
    modal: true
    title: qsTr("Restore to this version?")
    standardButtons: QC.Dialog.Ok | QC.Dialog.Cancel

    contentItem: QC.Label {
        text: qsTr("This will create a new save that restores the project to:\n\n\"%1\"\n\nAll history will be preserved.")
              .arg(page._pendingRestoreSubject)
        wrapMode: Text.Wrap
    }

    onAccepted: {
        RootData.project.restoreToCommit(page._pendingRestoreSha)
    }
}
```

**C++ guard:** `cwSaveLoad::restoreToCommitAndReconcile` still checks `repo->modifiedFileCount() > 0` after the saveFlush stage and returns an error if the working dir isn't clean. This is a safety net — the QML flow above should always present a clean working dir by the time the C++ pipeline runs.

**Dependency:** The `AskToSaveDialog` integration depends on the discard rework landing first. The dialog needs a working discard flow to offer all three options (save/discard/cancel).

### 6. Progress/busy indicator

While the restore is running, `cwProject::syncInProgress` is `true` (set by `beginSyncOperation`). The UI should reflect this:

- **Context menu:** "Restore to here" is greyed out when `syncInProgress` (prevent double-trigger)
- **History page:** Show a `BusyIndicator` overlay or disable interaction while restoring. Follow the existing pattern used during sync — check how `GitHistoryPage` or `LinkBar`/`SyncButton` respond to `syncInProgress` and apply the same approach.
- **Confirmation dialog:** Disabled "OK" button if `syncInProgress` (race condition guard)

---

## Files Changed

| File | Change |
|------|--------|
| `QQuickGit/src/GitRepository.h` | Add `restoreToCommit()` declaration |
| `QQuickGit/src/GitRepository.cpp` | Implement `restoreToCommit()` + `buildRestoreCommitMessage()` helper (~90 lines) |
| `QQuickGit/qml/GitHistoryRow.qml` | Add right-click context menu with "Restore to here" |
| `QQuickGit/qml/GitHistoryView.qml` | Forward `restoreRequested` signal from delegate |
| `cavewherelib/src/cwSaveLoad.h` | Add `restoreToCommitAndReconcile()` declaration |
| `cavewherelib/src/cwSaveLoad.cpp` | Implement `restoreToCommitAndReconcile()` (~80 lines), refactor shared pipeline with `resetBranchAndReconcile` |
| `cavewherelib/src/cwProject.h` | Add `Q_INVOKABLE restoreToCommit()` |
| `cavewherelib/src/cwProject.cpp` | Implement `restoreToCommit()` wrapper (~10 lines) |
| `cavewherelib/qml/GitHistoryPage.qml` | Wire `onRestoreRequested`, add confirmation dialog, busy indicator |

---

## Test Cases

### Unit tests: `GitRepository::restoreToCommit` (in QQuickGit tests)

Create a temp repo programmatically with known commits (following `test_GitRepository.cpp` pattern).

| Test | Setup | Action | Verify |
|------|-------|--------|--------|
| Basic restore | Commits A → B → C | `restoreToCommit(A.sha)` | New commit D exists; D's tree = A's tree; D's parent = C; all 4 commits in log |
| Commit message audit trail | Commits A → B → C | `restoreToCommit(A.sha)` | Subject is "Restored to: A's subject"; body contains target SHA, previous HEAD SHA, lists B and C as rolled back |
| Restore to HEAD (no-op tree) | Commits A → B | `restoreToCommit(B.sha)` | New commit C exists with same tree as B; operation succeeds (no early return) |
| Invalid SHA | Commits A → B | `restoreToCommit("badc0ffee...")` | Future completes with error; HEAD unchanged |
| No HEAD (empty repo) | Empty repo | `restoreToCommit("anything")` | Future completes with error |
| Working dir updated | Commits A (file1="hello") → B (file1="world") | `restoreToCommit(A.sha)` | file1 on disk contains "hello" |
| Index updated | Commits A → B | `restoreToCommit(A.sha)` | `git status` shows clean working dir (index matches tree) |
| Large rollback cap | Commits A → B1 → B2 → ... → B15 | `restoreToCommit(A.sha)` | Body lists first 10 commits + "... and 5 more" |
| Restore across merge | A → B, branch: A → C, merge B+C → D | `restoreToCommit(A.sha)` | New commit E; E's tree = A's tree; E's parent = D; audit trail lists commits from both branches |
| Restore to pre-merge commit | A → B → merge(B,C) → D | `restoreToCommit(B.sha)` | New commit E; E's tree = B's tree; working dir matches B's state |

### QML tests: `tst_RestoreToVersion.qml` (in test-qml/)

| Test | Setup | Action | Verify |
|------|-------|--------|--------|
| Restore updates model | Create project, add cave, save, add trip, save | Restore to first save | In-memory model has cave but no trip |
| Git history after restore | Same as above | Restore to first save | Git log shows 3 commits (2 saves + 1 restore) |
| Restore with unsaved changes | Create project, add cave, save, modify cave (don't save) | Restore to save | Modification is flushed+committed first, then restore commit created |
| Error propagates to errorModel | Create project | `restoreToCommit("invalid")` | `errorModel` contains error message |
| Dirty working dir blocked | Create project, add cave, save, modify cave (don't save) | Call `restoreToCommit` directly (bypass dialog) | Returns error — working dir must be clean |

### Manual testing

1. Open a project with sync history
2. Go to history page, right-click an old commit, select "Restore to here"
3. Confirm dialog appears with correct commit subject
4. Accept — project state reverts, new commit appears in history
5. Verify "Restore to here" is greyed out on HEAD commit
6. Verify sync/push still works after restore

---

## Verification

1. **Build:** `cmake --build build/<preset> --target all`
2. **C++ tests:** `./build/<preset>/cavewhere-test "[RestoreToCommit]"`
3. **QML tests:** `./build/<preset>/cavewhere-qml-test --platform offscreen -testall tst_RestoreToVersion`
4. **Both suites:** `./build/<preset>/cavewhere-test && ./build/<preset>/cavewhere-qml-test --platform offscreen`
