# Sync on Close Plan

## Summary

When the user closes a project that has a remote configured and has local changes not yet
pushed, offer to sync in the same dialog that asks to save. This keeps the flow to a single
decision point rather than two sequential modals.

---

## Conditions for showing sync option

- Project has a remote (`!syncHealth.status.noRemote` — already exposed by `cwProjectSyncHealth`)
- `syncHealth.status.aheadCount > 0` OR `isModified()` is true (there are commits to push,
  or working-tree changes that will become a commit after save)
- `syncHealth.status` is not stale — call `syncHealth.refresh()` before opening the dialog,
  or treat a stale status as "assume sync needed" (err on the side of offering sync)
- Project is not a new empty project

If none of these conditions hold, the dialog behaves exactly as it does today.

---

## Checkpoints

### Checkpoint 1 — C++ foundation
Add `syncFinished()` and `errorModel` clear to `cwProject`. All changes are self-contained
in C++ and can be reviewed and tested without any QML changes.

**Deliverables:** `cwProject.h`, `cwProject.cpp`, C++ tests pass.

### Checkpoint 2 — QML dialog
Extend `AskToSaveDialog.qml` with the `offerSync` property, state machine, and new button
set. Depends on Checkpoint 1 being merged.

**Deliverables:** `AskToSaveDialog.qml`, QML tests pass.

---

## C++ changes (Checkpoint 1)

### 1. Add `syncFinished()` signal to `cwProject`

**Why:** The current completion path emits `syncAuthFailed()` from `completeSyncOperation`
and then `syncInProgressChanged` from the outer `SyncFuture` observer in `beginSyncOperation`.
These arrive as two separate signal emissions with no single "done" notification. QML listeners
must coordinate multiple signals with local state — fragile and easy to get wrong.

`syncFinished()` (no parameter) is emitted at the end of all code paths in
`completeSyncOperation` as a pure "operation complete — now check state" trigger. Error
details are already carried by the existing `syncAuthFailed()` signal and `errorModel`, so
there is no need to duplicate them in the parameter. This keeps error handling unified and
consistent with the existing pattern used for save errors.

**Changes to `cwProject.h`:**
```cpp
signals:
    // existing ...
    void syncFinished();   // ADD
```

**Changes to `cwProject.cpp` — `sync()`:**

Clear `errorModel` at entry so any errors present after `syncFinished()` are definitively
from this operation:

```cpp
bool cwProject::sync()
{
    if (!m_saveLoad || syncInProgress()) {
        return false;
    }

    ErrorModel->clear();   // ADD — clean state before sync begins

    auto* provider = m_saveLoad->authProvider();
    // ... rest unchanged
}
```

**Changes to `cwProject.cpp` — `completeSyncOperation`:**
```cpp
void cwProject::completeSyncOperation(const Monad::ResultBase& result)
{
    if (result.hasError()) {
        if (result.errorCodeTo<cwSaveLoad::SyncErrorCode>() == cwSaveLoad::SyncErrorCode::HttpAuthFailed) {
            emit syncAuthFailed();
            emit syncFinished();   // ADD
            return;
        }
        const QString message = result.errorMessage();
        const bool alreadyReported = ErrorModel
                                     && ErrorModel->count() > 0
                                     && ErrorModel->last().message() == message;
        if (!alreadyReported) {
            ErrorModel->append(cwError(message, cwError::Warning));
        }
        emit syncFinished();       // ADD
        return;
    }
    m_syncHealth->refresh();
    emit syncFinished();           // ADD
}
```

---

## QML changes (Checkpoint 2)

### 3. Extend `AskToSaveDialog.qml`

The dialog gains awareness of remote state and a multi-state flow.

#### New property on the `Loader`

```qml
readonly property bool offerSync:
    !RootData.project.syncHealth.status.noRemote
    && (RootData.project.syncHealth.status.aheadCount > 0
        || RootData.project.modified
        || RootData.project.syncHealth.status.stale)
```

Note: `syncHealth.refresh()` should be called when the dialog opens. `refresh()` is async,
so `offerSync` evaluates on the current (possibly stale) value first and re-evaluates
reactively as the refresh completes. This is correct behaviour — the binding is live.

#### Dialog states

| State | Buttons shown |
|-------|---------------|
| `idle` | "Save & Sync" (visible only when `offerSync`) / "Save" / "Discard" / "Cancel" |
| `saving` | spinner, all disabled |
| `syncing` | spinner, all disabled |
| `syncError` | "Close anyway" / "Stay open" |

A single `idle` state with `offerSync` controlling button visibility replaces the previous
`idle` / `idle-no-remote` split.

#### How the dialog determines sync error vs success in `onSyncFinished`

`cwProject::sync()` clears `errorModel` before starting the operation. This means:

- Any errors present in the model before sync indicate a bad state — sync should not be
  attempted. The dialog should not call `project.sync()` if `errorModel.count > 0` after save.
- After `syncFinished()` fires, any errors in `errorModel` are definitively from this sync.
  No snapshot or count comparison is needed.

The dialog reacts in `onSyncFinished` by inspecting what was set during the operation:

- `syncAuthFailed` fired → `authFailed` local bool is `true` → show auth error message
- `errorModel.count > 0` → show general error message
- neither → proceed to close (`_privateAfterSave()`)

`cwProject::sync()` must also be updated to clear `errorModel` at entry (before the auth
credential check, so the model is clean regardless of which code path proceeds to the
actual sync operation).

#### Flow

1. Dialog opens → call `syncHealth.refresh()` → evaluate `offerSync`

2. User clicks **Save & Sync**
   - `onSaveConfirmed()` called, `project.save()` called
   - State → `saving`
   - `onFileSaved` → state → `syncing`, call `project.sync()`
   - `onSyncAuthFailed` → set `authFailed = true`
   - `onSyncFinished`:
     - `authFailed` → state `syncError`, auth message
     - `errorModel` has new entries → state `syncError`, error message
     - otherwise → `_privateAfterSave()` (proceeds to close)

3. User clicks **Save** (no sync)
   - Existing save path — proceeds to close after `onFileSaved`

4. User clicks **Discard**
   - Existing discard path — no sync offered

5. User clicks **Close anyway** (from `syncError`)
   - Calls `_privateAfterSave()` — closes without sync

6. User clicks **Stay open** (from `syncError`)
   - Calls `closeDialog()` — returns user to app to use `SyncButton` directly

7. Dialog opens while `syncInProgress` is already `true` (background sync running)
   - Start in `syncing` state immediately; do NOT call `project.sync()` again
   - On `onSyncFinished`: if `errorModel.count > 0` or `authFailed`, show `syncError` state
   - If the project is still `modified` after the in-flight sync completes (user had unsaved
     changes alongside the running sync), proceed to the normal `saving` → `syncing` flow
     from there rather than closing immediately

#### Error message text

- Auth error: `"GitHub access has expired. Your changes are saved locally."`
- General error: `"Sync failed: " + lastErrorMessage + "\nYour changes are saved locally."`

The "saved locally" note is essential — it reassures the user their work is not lost.

---

## What does NOT change

- `syncAuthFailed` signal is kept — `SyncButton` and other existing consumers still use it
- The existing `errorModel.countChanged` handler in `AskToSaveInteralDialog` is kept for save errors
- Temporary project path (Delete / Save As) is unaffected — no remote concept there
- The `SyncButton` in `LinkBar.qml` is unaffected
- Bundled archive projects (`.cw` zip) may have a remote if they contain a `.git` directory;
  `syncHealth.status.noRemote` handles this correctly since it derives from the same repository

---

## Test cases to add

### Checkpoint 1 — C++ tests (`testcases/`)

| Test |
|------|
| `syncFinished()` emitted on sync success |
| `syncFinished()` emitted after `syncAuthFailed()` on auth failure |
| `syncFinished()` emitted after `errorModel` append on general failure |
| `errorModel` is cleared at the start of `sync()` |

### Checkpoint 2 — QML tests (`test-qml/`)

| Test |
|------|
| Close with remote + `aheadCount > 0` → "Save & Sync" button visible |
| Close with remote + `modified` true + `aheadCount == 0` → "Save & Sync" button visible |
| Close with no remote → "Save & Sync" button not visible |
| Close with remote + `aheadCount == 0` + not modified → "Save & Sync" button not visible |
| Close while `syncInProgress` already true → dialog opens in `syncing` state |
| Save & Sync success → app closes |
| Save & Sync auth failure → `syncError` state shown, "Stay open" returns to app |
| Save & Sync general failure → `syncError` state shown with error message |
| "Close anyway" after sync error → app closes |
