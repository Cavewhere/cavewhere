# Project Name, Save, and Export Plan

## Summary

This plan separates project identity from filesystem naming so that:

- `Save` means save in place.
- Exporting between bundle and directory formats does not silently rename the project.
- `dataRoot` stays stable after first save unless the user explicitly changes it.
- Project name becomes editable in the UI and syncs like other user-owned names.

The current bug is that `Save As` derives too much from the destination basename. It rewrites the project file name, `dataRoot`, and `cwCavingRegion::name()`. That behavior is acceptable only for a brand-new unsaved project establishing its initial identity. It is not acceptable for saved projects, especially Git-backed ones.

## Current State

### Save and Save As coupling

- `cwProject::saveAs()` updates `cavingRegion()->setName(baseName)` and `m_saveLoad->setDataRoot(baseName)` after both bundle and directory Save As.
- `cwSaveLoad::copyProjectTo()` / `moveProjectTo()` also renames the on-disk data root to match the destination basename and then writes the new basename back into project metadata.
- Existing tests currently assert that Save As changes `dataRoot` and persists that rename.

### Project name model

- The project-visible name is currently `cwCavingRegion::name`.
- `cwCavingRegion` already exposes `name` as a writable QML property.
- There is no dedicated project-name editor in QML today.

### Sync and merge model

- Cave rename reconciliation already exists and is stable-id based:
  - `cwCaveData` carries a `QUuid id`.
  - `cwCaveMergePlanBuilder` maps current/loaded/base caves by id.
  - `cwCaveMergeApplier` merges only the `name` bundle.
  - `cwCaveSyncMergeHandler` detects changed `.cwcave` descriptors and applies the merge plan.
- `cwCavingRegionData` currently has `name` and `caves`, but no stable id.

## Product Decisions

### 1. Remove generic rename semantics from Save As

The old generic `Save As` concept is too overloaded. We should stop using it as an implicit rename path.

Target behavior:

- `Save`
  - New unsaved project: opens first-save flow.
  - Existing saved project: saves in place only.
- `Export As Bundle...`
  - Writes a `.cw` copy.
  - Does not mutate project name, `dataRoot`, or sync identity.
- `Export As Directory...`
  - Writes a directory copy.
  - User chooses a parent folder, not a `.cwproj` filename.
  - Does not mutate project name, `dataRoot`, or sync identity.
- `Rename Project...` or inline project-name editing
  - Explicitly changes the project-visible name.
  - Sync/reconcile handles this like other renameable objects.

### 2. First save establishes identity

A brand-new unsaved project is the one case where path-derived defaults are acceptable.

First save should establish:

- initial project-visible name, if still using the temporary default
- initial `dataRoot`
- initial container format
- initial save location

After that first save:

- changing export destination must not change project name
- changing export destination must not change `dataRoot`
- container conversion is an export concern, not a rename concern

### 3. Project name is user-owned metadata

Project name should be treated like cave name:

- editable in the UI
- persisted as project metadata / region name
- merged during sync using current / loaded / base values
- not derived from filename after first save

### 4. Git-backed projects should favor duplication/export semantics

For Git-backed projects, users often want a safe experimental copy. The app should not interpret that intent as a rename of the tracked project.

Exports or duplication should preserve internal identity unless the user explicitly requests a rename or a disconnected copy.

## Implementation Plan

## Phase 1: Stop mutating project identity during export

Update save/export behavior so bundle and directory export do not rewrite project internals for already-saved projects.

Changes:

- Split first-save behavior from export behavior in `cwProject`.
- Remove the post-export mutation that currently changes region name and `dataRoot`.
- Update `cwSaveLoad::copyProjectTo()` / `moveProjectTo()` so existing projects preserve `projectMetadata.dataRoot` instead of deriving a new one from the destination basename.
- Preserve internal `.cwproj` payload metadata even when the outer exported filename or folder name differs.

Notes:

- Temporary/new projects still need initialization logic for the first committed save.
- Existing tests that assert Save As renames `dataRoot` will need to be replaced with stability tests.

## Phase 2: Introduce explicit first-save flow

Replace the current file-picker-first Save As behavior for new projects with a first-save flow that asks for:

- project name
- storage type: bundle or directory
- destination

Behavior:

- Directory save should choose a parent folder, then create the project directory within it.
- Bundle save should choose a file path.
- If the project name is still the temporary default, first save should replace it with the chosen name.
- `dataRoot` should be initialized once from that chosen project name and then remain stable.

UI impact:

- `FileMenu.qml` will need a simplified menu that distinguishes save from export.
- `SaveAsDialog.qml` likely needs to be retired or refactored into separate bundle/directory export flows.

## Phase 3: Add project-name editing in the UI

Add a visible project-name editor, likely on `cavewherelib/qml/CavePage.qml` or another top-level project page.

Recommended approach:

- Reuse the existing `DoubleClickTextInput` pattern used for cave names.
- Bind it to `RootData.region.name`.
- Keep this separate from any file/export path chooser.

Considerations:

- The location should make it clear this is the project title, not the filesystem name.
- Renaming here should not immediately rename `dataRoot` or move files on disk.

## Phase 4: Add project rename sync / merge / reconciliation

Project rename should follow the cave rename pattern conceptually:

- identify the changed object deterministically
- compare current / loaded / base values
- merge only the `name` bundle
- keep local value on conflict

### Proposed architecture

Add a project- or region-level merge path parallel to the cave rename path:

- `cwProjectMergePlanBuilder` or `cwCavingRegionMergePlanBuilder`
- `cwProjectMergeApplier` or `cwCavingRegionMergeApplier`
- `cwProjectSyncMergeHandler` or `cwCavingRegionSyncMergeHandler`

### Identity choice

Because there is only one region per project today, there are two viable approaches:

- Singleton approach:
  - treat the region/project as the single root object for the loaded project
  - no new UUID required
  - merge the root `name` directly using current / loaded / base snapshots
- Stable-id approach:
  - add a persistent root/project UUID to project metadata or `cwCavingRegionData`
  - mirror the cave merge architecture more literally

Recommended first step:

- Use singleton root-object semantics first, because the project currently has exactly one region.
- Add a UUID only if future multi-root scenarios or sync diagnostics demand it.

### Detection strategy

The handler should trigger when the root project descriptor changes:

- `.cwproj` changed in directory mode
- embedded `.cwproj` content changed when loading from bundle-derived sync state, if applicable

It should:

- load current, loaded, and base root project name values
- apply deterministic merge
- mark the root object as path-ready if page/address labels depend on the name

### Merge policy

Use the same rename conflict behavior as caves:

- remote rename applies when local still matches base
- local rename wins on concurrent rename conflict

## Phase 5: Update navigation and page naming

Project rename will affect page labels and addresses in the same way cave and trip renames do.

Work items:

- identify pages whose titles derive from project/region name
- ensure rename propagation updates current page address/history correctly
- add tests similar to `test_cwPageSelectionModel.cpp` if project name appears in page addresses

If the current page hierarchy does not include project name in addresses, this phase may be minimal.

## Testing Plan

### Save/export tests

- New project first save initializes project name and `dataRoot` once.
- Export bundle from saved project preserves project name and `dataRoot`.
- Export directory from saved project preserves project name and `dataRoot`.
- Converting bundle to directory does not rename project internals.
- Converting directory to bundle does not mutate the loaded project state.

### UI tests

- QML test for editing project name from the chosen page.
- QML test that first-save flow distinguishes bundle vs directory destination handling.

### Sync / merge tests

- project merge plan applies remote rename when local matches base
- project merge keeps local rename on conflict
- reconcile handler applies incremental project rename without full reload
- reconcile handler falls back cleanly when root descriptor identity cannot be resolved

### Regression tests

- existing cave rename sync tests continue to pass unchanged
- page-selection rename propagation still behaves correctly
- no Git dirty-state changes from bundle export alone

## Open Questions

### 1. Menu shape

Preferred simple menu:

- `Save`
- `Export As Bundle...`
- `Export As Directory...`

Alternative:

- `Save`
- `Export As...` with a small format picker dialog

### 2. Duplicate-project workflow

If users often want a disconnected experimental copy, we may also want:

- `Duplicate Project...`

That can come later. It is separate from fixing project identity coupling.

### 3. Root object identity

We need to decide whether singleton region merge is enough or whether to add a persistent project/root UUID now. The singleton approach is likely sufficient for the current architecture and lower risk for the first implementation.

## Recommended Order

1. Decouple export from rename/dataRoot mutation.
2. Replace first-save path selection with a dedicated first-save flow.
3. Add project-name editing in QML.
4. Add project/root rename sync merge handling.
5. Update tests and page/address propagation as needed.

## Files Likely Affected

- `cavewherelib/src/cwProject.cpp`
- `cavewherelib/src/cwSaveLoad.cpp`
- `cavewherelib/qml/FileMenu.qml`
- `cavewherelib/qml/SaveAsDialog.qml`
- `cavewherelib/qml/CavePage.qml`
- `cavewherelib/src/cwCavingRegion.h`
- `cavewherelib/src/cwCavingRegion.cpp`
- new project/root merge handler and tests under `cavewherelib/src/` and `testcases/`
