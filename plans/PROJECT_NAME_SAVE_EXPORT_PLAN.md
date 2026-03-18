# Project Name, Save, and Export Plan

Issues: #326, #327, #331

## Summary

This plan separates project identity from filesystem naming so that:

- `Save` means save in place.
- `Save As...` writes a copy in the chosen format without silently renaming the project.
- `dataRoot` stays stable after first save unless the user explicitly renames the project.
- Project name is user-owned metadata, editable in the UI, and synced like other renameable objects.

The current bug is that `Save As` derives too much from the destination basename: it rewrites the project file name, `dataRoot`, and `cwCavingRegion::name()`. That behavior is acceptable only for a brand-new unsaved project establishing its initial identity. It is not acceptable for already-saved projects, especially Git-backed ones.

---

## Current State

### Save and Save As coupling

- `cwProject::saveAs()` updates `cavingRegion()->setName(baseName)` and `m_saveLoad->setDataRoot(baseName)` after both bundle and directory Save As.
- `cwSaveLoad::transferProjectTo()` (used by `copyProjectTo()` / `moveProjectTo()`) derives a new `dataRoot` from the destination basename and writes it back into project metadata.
- Existing tests currently assert that Save As changes `dataRoot` and persists that rename.

### Default project name

New projects get a random temporary name via `randomName()` in `cwSaveLoad.cpp`:

```cpp
QString randomName() {
    quint32 randomValue = QRandomGenerator::global()->generate();
    return QStringLiteral("cavewhereTmp-") + QStringLiteral("%1").arg(randomValue, 8, 16, QChar(u'0'));
}
```

This name (e.g., `cavewhereTmp-a1b2c3d4`) is used as both the region name and the temporary directory name. It is never shown to the user in a friendly form.

### Project name model

- The project-visible name is `cwCavingRegion::name`.
- `cwCavingRegion` already exposes `name` as a writable QML property.
- `DataMainPage.qml` currently shows the static text `"All Caves"` (lines 35–39) — this is the natural location for the project name editor.
- `CavePage.qml` uses `DoubleClickTextInput` bound to `currentCave.name` as the established pattern for inline name editing.

### Save As dialog

`SaveAsDialog.qml` is a native `FileDialog` (mode `SaveFile`). It handles both bundle (`.cw`) and directory (`.cwproj`) formats by manipulating the selected path string, but uses a single native save-file dialog in both cases. For bundle saves this works well. For directory saves this is awkward — the user should pick a **parent folder**, not a filename, but a `FolderDialog` cannot offer format selection. A custom wrapper dialog is needed.

The no-extension handling bug (#327) was fixed in `cb124177` via `finalProjectPathForSelection()` in `SaveAsDialog.qml`. This logic must be preserved when the dialog is refactored in Checkpoint 2.

### Sync and merge model

Cave rename reconciliation is the established pattern:

- `cwCaveData` carries a `QUuid id` as stable identity.
- `cwCaveMergePlanBuilder` matches current/loaded/base caves by UUID, producing `cwCaveMergePlan` entries.
- `cwCaveMergeApplier::applyCaveMergePlan()` applies a three-way name merge (remote wins when local matches base; local wins on conflict).
- `cwCaveSyncMergeHandler` detects changed `.cwcave` descriptors and drives the plan/apply cycle.
- Cave rename also **physically moves directories on disk** via the Job queue (`Job::Action::Move` on `Job::Kind::Directory`). The old path comes from `m_objectStates[id].currentPath`; the new path is derived from the object's current sanitized name.
- An identical parallel system exists for trip renames (`cwTripMergePlanBuilder` / `cwTripMergeApplier` / `cwTripSyncMergeHandler`).

`cwCavingRegionData` has `name` and `caves` but no stable UUID today.

---

## Product Decisions

### 1. "Already-saved" definition

A project is considered **already-saved** when `isTemporaryProject()` returns `false`. This covers:

- Projects loaded from a directory project.
- Projects loaded from a bundled `.cw` archive (`LoadedFromBundledArchive` is set, `isTemporary` is false after load).

A project is considered **unsaved** (first-save flow applies) when `isTemporaryProject()` returns `true`. This covers:

- Newly created projects.
- Legacy `.cw` SQLite files, which are auto-converted to a temporary directory on load and require a re-save to establish a new identity.

### 2. Project directory structure

For a directory-format project named `MyCave`, the on-disk layout is:

```
MyCave/                    ← outer project directory (Git working tree root, no extension)
  MyCave.cwproj            ← descriptor file (project metadata + region name)
  MyCave/                  ← dataRoot directory (same name as project by default)
    cave-one/
      cave-one.cwcave
      ...
```

`projectRootDirForFile(filename)` returns the outer `MyCave/` directory. The `.cwproj` descriptor file lives inside it and is what `projectFileName` points to. The outer directory has **no file extension**.

The current code generates paths with a `.cwproj` suffix on the outer directory. Adopting this layout requires updating `parseDestination` and related path helpers in `cwSaveLoad.cpp`.

### 3. Menu shape

The file menu keeps the familiar `Save As...` label. Format selection moves inside the dialog:

```
Save             Ctrl+S
Save As...       Ctrl+Shift+S
─────────────────
Settings
About
```

`Save As...` opens a custom `QC.Dialog` containing:

- A format picker: `Bundle (.cw)` / `Directory (.cwproj)` — radio buttons or a compact selector.
- A destination path row with a native picker button:
  - Bundle mode: opens a native `FileDialog` (SaveFile mode) for a `.cw` path.
  - Directory mode: opens a native `FolderDialog` for a parent folder, then appends the project name as the subfolder automatically (e.g., chosen folder `~/Documents/` + project name `MyCave` → `~/Documents/MyCave`).
- For **new/temporary projects only**: a project name field, pre-filled from the name generator, that the user can edit before saving.
- For **already-saved projects**: no name field — the copy inherits the current project name and `dataRoot`.

The current `SaveAsDialog.qml` is refactored into this custom dialog. The native picker buttons inside stay native for platform integration.

### 4. Save As behavior for already-saved projects

`Save As...` on an already-saved project writes a copy at the new location **without** mutating the loaded project's identity:

- Project name (`cwCavingRegion::name`) is unchanged.
- `dataRoot` is unchanged.
- The loaded project file path is unchanged.
- Git sync state is unaffected.

### 5. First save establishes identity

For new or temporary projects, `Save As...` initializes:

- Project name — from the dialog name field; the temp name is replaced.
- `dataRoot` — derived once from the project name and never changed by subsequent Save As operations.
- Container format — bundle or directory.
- Save location.

After first save these values are stable.

### 6. Default project name — whimsical mountain name generator

Replace the current `cavewhereTmp-<hex>` scheme with a whimsical mountain-themed name generator used as the pre-fill in the first-save dialog. The user can edit it before confirming. Style: **adjective + mountain noun** — `"Misty Peak"`, `"Thunder Ridge"`, `"Obsidian Summit"`, `"Glacier Pass"`. Aim for ~30 adjectives × 20 nouns = 600 combinations. Collisions are low risk since the name is immediately editable.

The generator lives in C++ alongside `randomName()` and is used to pre-fill `cwCavingRegion::name` on `newProject()` so the UI immediately shows something readable. The old `randomName()` stays for temporary directory naming (directory names don't need to be human-readable).

### 7. Project name editor in the UI

Replace the static `"All Caves"` heading in `DataMainPage.qml` (lines 35–39) with a `DoubleClickTextInput` bound to `RootData.region.name`, following the exact pattern used for cave name editing in `CavePage.qml` (lines 68–77).

Changing the name here triggers the same full rename as sync reconciliation:

- `cwCavingRegion::name` updated in memory.
- `dataRoot` directory physically renamed on disk (move job queued immediately on `onFinishedEditing`).
- `.cwproj` descriptor file physically renamed on disk (move job queued immediately).

This is the intended replacement for the implicit rename-via-Save-As path removed in Checkpoint 1, and is consistent with how cave rename works.

### 8. Project rename — full physical rename

When the project name changes (whether via UI edit or sync reconciliation), the full rename applies:

1. `cwCavingRegion::name` → new name (in-memory).
2. `dataRoot` directory move: `MyCave/` → `NewName/` inside the project root.
3. `.cwproj` descriptor file rename: `MyCave.cwproj` → `NewName.cwproj` inside the project root.
4. `d->projectMetadata.dataRoot` → new sanitized name.

The **outer `MyCave/` directory is never renamed by CaveWhere**. It is the Git working tree root and may be bookmarked in the OS shell. Users may rename it outside the app without breaking project internals, since `dataRoot` and the descriptor file encode identity on disk.

### 9. Dedicated builder/applier/handler classes for project rename sync

Use dedicated `cwCavingRegionMergePlanBuilder`, `cwCavingRegionMergeApplier`, and `cwCavingRegionSyncMergeHandler` classes following the cave/trip pattern. The physical move jobs make the logic complex enough to warrant the full pattern. The cave/trip classes should be reviewed for code that can be refactored into shared utilities.

### 10. Git dirty-state guarantee

`Save As...` on an already-saved directory project writes to a path outside the tracked project directory. No tracked files are touched, so Git sees no diff. Bundle saves go to a user-chosen path and are similarly outside the project root unless the user deliberately chooses an inside path.

---

## Implementation Checkpoints

These are testable milestones within a single implementation pass, not separate releases. Implement and verify each checkpoint in order before moving to the next.

### Checkpoint 1 — Stop identity mutation + name editor + name generator

**Goals**: remove the Save As rename side-effect for saved projects; give users an explicit rename path before it disappears.

Changes to `cwProject::saveAs()`:

- After a successful bundled save, skip `cavingRegion()->setName(baseName)` and `m_saveLoad->setDataRoot(baseName)` when `!isTemporaryProject()`.
- After a successful directory save, same guard.
- Keep the mutation for temporary projects — first-save semantics still apply here until Checkpoint 2 lands.

Changes to `cwSaveLoad::transferProjectTo()`:

- For already-saved projects, preserve `d->projectMetadata.dataRoot` from the source instead of deriving a new value from the destination basename.
- Still write the existing `dataRoot` into the copied project's metadata so the copy is internally consistent.
- For temporary projects, keep current behavior.

Changes to `cwSaveLoad::parseDestination` and related path helpers:

- Outer project directory must be named without a `.cwproj` extension (e.g., `MyCave/`, not `MyCave.cwproj/`).

Add `friendlyProjectName()` in C++:

- Produces whimsical mountain-themed two-word names (adjective + mountain noun).
- Uses fixed word lists (~30 adjectives, ~20 nouns) with `QRandomGenerator` — no new dependencies.
- Used in `cwSaveLoad::newProject()` as the initial region name.

Update `DataMainPage.qml`:

- Replace `Text { text: "All Caves" }` (lines 35–39) with `DoubleClickTextInput` bound to `RootData.region.name`, matching the cave-name pattern in `CavePage.qml:68–77`.
- On `onFinishedEditing`, queue both move jobs: `dataRoot` directory rename and `.cwproj` descriptor file rename.

Update existing tests:

- Replace tests asserting Save As renames `dataRoot` with stability tests (Save As on a saved project must leave `dataRoot` and region name unchanged).
- Add tests confirming Save As on a temporary project still initializes `dataRoot` from the chosen basename.

### Checkpoint 2 — First-save dialog with inline format picker

**Goal**: replace the awkward single-native-dialog Save As with a custom dialog that correctly handles both formats.

Refactor `SaveAsDialog.qml` into a custom `QC.Dialog`. The no-extension path handling from `finalProjectPathForSelection()` (#327) must be carried forward into the new dialog — when the user types a name without an extension, the correct extension must be appended based on the selected format.

For **new/temporary projects**, the dialog shows:

- Project name field (pre-filled from `cwCavingRegion::name()`, which is now a friendly mountain name).
- Format picker: `Bundle (.cw)` / `Directory (.cwproj)`.
- Destination path row with a native picker button:
  - Bundle: `FileDialog` (SaveFile mode).
  - Directory: `FolderDialog` (picks parent folder; dialog displays the resolved full path, e.g., `~/Documents/MyCave`).

For **already-saved projects**, the dialog shows:

- Format picker.
- Destination path row.
- No name field — copy inherits the current project name.

`FileMenu.qml` entry point (`Save As...`) does not change.

### Checkpoint 3 — Project rename sync

**Goal**: sync reconciliation applies project renames with the same full physical rename as the UI editor.

Detection: triggered when the root `.cwproj` descriptor file changes in the working tree.

Three-way merge values:

- `currentName` — `cavingRegion()->name()` (in-memory).
- `loadedName` — region name read from the updated `.cwproj` on disk.
- `baseName` — region name from the merge-base commit (read via libgit2).

Merge policy (identical to cave rename):

- Remote rename applies when `currentName == baseName` (local unchanged).
- Local rename wins on concurrent conflict (`currentName != baseName` and `loadedName != baseName`).

On resolved rename, apply the full rename (Product Decision §8):

1. `cwCavingRegion::name` → merged name.
2. `dataRoot` directory move job.
3. `.cwproj` descriptor file rename job.
4. `d->projectMetadata.dataRoot` → new sanitized name.

New classes: `cwCavingRegionMergePlanBuilder`, `cwCavingRegionMergeApplier`, `cwCavingRegionSyncMergeHandler`. Review cave/trip counterparts for shared utilities to refactor.

---

## Testing Plan

### Save / Save As tests

- `Save` on an already-saved project writes in place; `dataRoot` and region name are unchanged.
- `Save As...` on an already-saved project writes a copy; `dataRoot`, region name, and original file path are unchanged.
- `Save As...` on a new/temporary project initializes `dataRoot` and region name from the dialog name field.
- Bundle-format copy preserves the source project's `dataRoot` in the copy's metadata.
- Directory-format copy preserves the source project's `dataRoot` in the copy's metadata.
- Converting bundle to directory does not rename loaded project internals.
- Converting directory to bundle does not mutate the loaded project state.
- Legacy `.cw` SQLite load auto-converts to temporary; subsequent Save As initializes identity correctly.
- Directory project outer folder is named without `.cwproj` extension.

### Name generator tests

- `friendlyProjectName()` returns a non-empty string on every call.
- Generated name contains exactly two words (adjective + mountain noun).
- Successive calls return different names with high probability (collision test over N samples).
- Generated name does not contain the `cavewhereTmp-` prefix.

### UI tests

- QML test: `DataMainPage` shows `DoubleClickTextInput` bound to region name.
- QML test: editing project name renames `dataRoot` directory and `.cwproj` descriptor file on disk.
- QML test: Save As dialog shows format picker for both new and saved projects.
- QML test: Save As dialog shows name field only for new/temporary projects.
- QML test: directory destination path appends project name as subfolder (no `.cwproj` on folder name).
- QML test: bundle destination path uses chosen filename directly.
- QML test: typing a name without extension appends the correct extension for the selected format (#327).

### Sync / merge tests

Mirror the cave rename test structure in `test_cwCaveMergePlanBuilder.cpp`:

- Remote project rename applies when local still matches base.
- Local project rename wins on concurrent conflict.
- Reconcile handler applies project name change without full reload.
- Reconcile handler renames `dataRoot` directory on disk when name changes.
- Reconcile handler renames the `.cwproj` descriptor file on disk when name changes.
- `dataRoot` metadata is updated to the new sanitized name after sync rename.
- Object paths inside the renamed `dataRoot` remain valid after the rename.

### Regression tests

- Existing cave rename sync tests pass unchanged.
- No Git dirty-state changes from bundle Save As on an already-saved directory project.

---

## Files Likely Affected

- `cavewherelib/src/cwProject.cpp`
- `cavewherelib/src/cwSaveLoad.cpp` (randomName, newProject, transferProjectTo, parseDestination)
- `cavewherelib/qml/FileMenu.qml`
- `cavewherelib/qml/SaveAsDialog.qml` (refactored to custom QC.Dialog)
- `cavewherelib/qml/DataMainPage.qml` (project name editor)
- `cavewherelib/src/cwCavingRegionMergePlanBuilder.h/.cpp` (new)
- `cavewherelib/src/cwCavingRegionMergeApplier.h/.cpp` (new)
- `cavewherelib/src/cwCavingRegionSyncMergeHandler.h/.cpp` (new)
- `cavewherelib/src/cwCaveMergePlanBuilder.cpp` / `cwTripMergePlanBuilder.cpp` (refactor shared utilities)
- `testcases/` and `test-qml/` for updated and new tests
