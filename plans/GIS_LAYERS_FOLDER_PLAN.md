# GIS Layers Folder Plan

## Goal

Move LAZ point-cloud layers from a list embedded in `.cwproj` metadata to a project-level `GIS Layers/` folder discovered by directory scan. This:

- Fixes the bug where `cwLazLayer::sourcePath` is empty on save (sourcePath is currently a no-op for serialization).
- Eliminates manifest-style merge conflicts when multiple collaborators add layers in parallel.
- Sets up the project for future drag-and-drop additions (drop a `.laz` into `GIS Layers/`, restart, it shows up).
- Renames `globalCS` → `globalCoordinateSystem` while we're touching the proto.

No backwards-compatibility shims — `fall2026` is unreleased.

## Project Layout

`GIS Layers/` lives at the **project root**, sibling to the `.cwproj` file and the data root — NOT inside the data root. This avoids collision with a cave named "GIS Layers" (cave directories live under `dataRoot/`).

```
MyProject/                       <- project root, git repo
  MyProject.cwproj
  MyProject Data Root/           <- dataRoot (cave subdirs)
    Cave1/
    Cave2/
  GIS Layers/                    <- new, project-level
    foo.laz
    bar.las
```

`.laz` and `.las` are already in `cavewhereTrackedExtensions()` (`cavewherelib/src/cwSaveLoad.cpp:355-357`), so LFS picks them up automatically once they're committed.

## Proto / `.cwproj` Changes

In `cavewherelib/src/cavewhere.proto`:

- `ProjectMetadata.globalCS` (field 4): rename to `globalCoordinateSystem`. Keep field number 4.
- `ProjectMetadata.lazLayers` (field 6): **remove**.
- `message LazLayer { ... }`: **remove** the entire message.

In `cavewherelib/src/cwSaveLoad.cpp`:

- `toProtoProject` (~line 1813-1820): drop the `lazLayers->writeTo(protoMetadata)` call; rename `mutable_globalcs()` → `mutable_globalcoordinatesystem()`.
- Load side (~line 2859-2867): rename `metadataProto.globalcs()` → `metadataProto.globalcoordinatesystem()`; drop the `lazlayers_size()` loop entirely.
- `LoadData::region.lazLayerSourcePaths`: delete the field; nothing populates it anymore.
- `saveMetadata` connections (~line 3370-3372):
  - **Drop** the `rowsInserted → saveMetadata` connection. `cwProject::addFiles` already triggers `saveFlushImpl()` after copies, and a `rowsInserted`-based hook would fire redundantly on every load-time `rescan()`.
  - **Keep** the `rowsRemoved → saveMetadata` connection — it drives the flush+commit after a layer file is queued for deletion via `rowsAboutToBeRemoved`. Update the comment to reflect this new role.
  - **Add** a new `rowsAboutToBeRemoved` connection that queues an `Action::Remove` for each layer's `sourcePath()` before the row is gone. Mirrors `cwSaveLoad.cpp:3195-3279`.
- Rename the `globalCSChanged` (now `globalCoordinateSystemChanged`) connection — same hook, renamed signal.

## C++ Changes

### `cwCavingRegion` (`cavewherelib/src/cwCavingRegion.{h,cpp}`)

Rename throughout:
- `globalCS()` → `globalCoordinateSystem()`
- `setGlobalCS()` → `setGlobalCoordinateSystem()`
- `globalCSChanged` → `globalCoordinateSystemChanged`
- `m_globalCS` → `m_globalCoordinateSystem`
- `Q_PROPERTY(QString globalCS ...)` → `Q_PROPERTY(QString globalCoordinateSystem ...)`
- All internal users (`cwCavingRegion.cpp:274,277,297,345,375`, plus connection in `setRegionGlobalCS` paths through `cwLazLayerModel`).

### `cwLazLayerModel` (`cavewherelib/src/cwLazLayerModel.{h,cpp}`)

Mirrors the `cwSurveyNoteModel` pattern (`cavewherelib/src/cwSurveyNoteModel.cpp:60-90`): **the model orchestrates the add via `cwProject::addFiles`, then refreshes its own rows on the callback**. The model is the only QML-facing API surface for layer mutation; QML never calls `cwProject::addFiles` directly. Coupling is one-way (model → project), and `cwSaveLoad` watches the model for file deletions (also one-way: saveLoad → model).

- Add `Q_INVOKABLE void addFromFiles(QList<QUrl> urls)` — the entry point from QML, mirroring `cwSurveyNoteModel::addFromFiles`.
  ```cpp
  void cwLazLayerModel::addFromFiles(QList<QUrl> urls) {
      auto* project = this->project();          // walks parent chain (see below)
      if (project == nullptr || urls.isEmpty()) {
          return;
      }
      QPointer<cwLazLayerModel> self(this);
      project->addFiles(urls, m_gisLayersDir,
                        [self](QList<QString> /*relativePaths*/) {
          if (self) {
              self->rescan();
          }
      });
  }
  ```
- Add `cwProject* project() const` helper — same pattern as `cwSurveyNoteModelBase::project()` (`cavewherelib/src/cwSurveyNoteModelBase.cpp:196`): walk up the parent chain (`parent()` is `cwCavingRegion`, whose owner is `cwProject`) until you find a `cwProject*`, return `nullptr` if none.
- Add `void setGisLayersDir(const QDir& dir)` — called by `cwSaveLoad` whenever the project root path changes (newProject, load, save-as, bundle-load, bundle-resave). If the dir is the same as `m_gisLayersDir`, no-op. Otherwise, store and call `rescan()`.
- Add `Q_INVOKABLE void rescan()` — clears existing layers, enumerates `*.laz`/`*.las` in `m_gisLayersDir` (non-recursive, case-insensitive extension match, alphabetical sort by filename). Files that don't match `.laz`/`.las` are silently ignored (covers `.DS_Store`, future `.cwgis` sidecars, junk files). Constructs one `cwLazLayer` per file with the absolute path as `sourcePath`. If `m_gisLayersDir` doesn't exist on disk (older `fall2026` project from before this change, or brand-new project), treat as empty — don't error.
- Keep `Q_INVOKABLE void removeAt(int index)` — model-only mutation:
  1. `beginRemoveRows` / `takeAt` / `endRemoveRows` / `layer->deleteLater()` (same as today).
  2. **No signal needed** — `cwSaveLoad` observes via the standard `QAbstractItemModel::rowsAboutToBeRemoved`/`rowsRemoved` signals (see "cwSaveLoad" below).
- **Delete** `writeTo(ProjectMetadata*)`, `readFrom(const ProjectMetadata&)`, and the public `addLayer(QString)` invokable — and their callers in `cwSaveLoad`. The old `addLayer` is gone from the QML surface; production code goes through `addFromFiles`. (Internal helpers used by `rescan()` to construct a `cwLazLayer` from an existing path stay, but they're not `Q_INVOKABLE`.)
- Roles unchanged. `SourcePathRole` keeps showing the absolute path inside `GIS Layers/`.

### What QML sees

```qml
// add
RootData.region.lazLayers.addFromFiles(lazFileDialog.selectedFiles)

// remove
RootData.region.lazLayers.removeAt(layerIndex)
```

Symmetric, model-centric, matches the way `NotesGallery.qml` calls `noteGallery.notesModel.removeNote(container.index)` (`cavewherelib/qml/NotesGallery.qml:334`).

### Brand-new / unsaved projects

`cwProject` always has a project root (a temp dir on construction), so `m_gisLayersDir` is always valid once `setGisLayersDir` has been wired. There is **no in-place fallback** needed in production — `addFromFiles` always copies into a real `GIS Layers/` directory, whether that lives in a temp project dir or a user-chosen path. On save-as, `copyDirectoryRecursively` (`cwSaveLoad.cpp:1130`) brings `GIS Layers/` along automatically.

For unit tests that construct a bare `cwLazLayerModel` without a `cwProject`, `addFromFiles` returns early (no project found via parent traversal). Tests that need to populate layers without a project should either (a) write `.laz` files into a temp dir and call `rescan()`, or (b) construct a real `cwProject` (the existing roundtrip tests already do this).

### `cwLazLayer` (`cavewherelib/src/cwLazLayer.{h,cpp}`)

No behavior changes. `sourcePath` continues to be the absolute path to a LAZ on disk; the loader doesn't care that it now points inside `GIS Layers/`.

### `cwSaveLoad` (`cavewherelib/src/cwSaveLoad.cpp`)

- Whenever `d->projectFileName` changes (newProject, load, save-as, bundle-load, bundle-resave to .cwproj), compute `projectRootDir().absoluteFilePath("GIS Layers")` and call `region->lazLayers()->setGisLayersDir(...)`. Specific spots: end of `newProject` (~line 1308-1324), end of the load callback (~line 1426-1431), end of the save-as path (~line 1220-1227), plus bundle extraction in `cwProject.cpp:1064` and bundle resave in `cwProject.cpp:1141`.
- **Observe `lazLayers` for file deletions** (mirror the note remove pattern at `cwSaveLoad.cpp:3195-3279`). In the same place the existing `rowsInserted`/`rowsRemoved` connections live (~line 3370-3372):
  - Connect `lazLayers::rowsAboutToBeRemoved(parent, first, last)`: for each row in the range, read `layer->sourcePath()` while the layer still exists, and queue an `Action::Remove` job via `d->addExplicitFileSystemJob({ kind=File, action=Remove, oldPath=path }, this)`. This is identical in shape to `removeResolvedFile(...)` at `cwSaveLoad.cpp:3218-3228`.
  - Keep the existing `rowsRemoved → saveMetadata` connection — it triggers the save-flush-commit pipeline so the queued remove actually runs and gets committed. Update the comment to reflect its new role.
  - Drop the existing `rowsInserted → saveMetadata` connection. With folder-scan + model-orchestrated add, `cwProject::addFiles` already calls `saveFlushImpl()` itself, which triggers `localMutationOccurred` + commit. Adding a second save-trigger on `rowsInserted` would cause a redundant flush after every `rescan()` (including the rescan after a project load, which is even worse).
- **No new signals on `cwLazLayerModel`.** Add uses `cwProject::addFiles`; remove uses standard `QAbstractItemModel` row signals. Same coupling shape as notes.
- **Widen `cwSaveLoadPrivate::isInsideRoot` for GIS Layers**: `Action::Copy` (used by `cwProject::addFiles` → `copyFilesAndEmitResults`), `Action::Remove`, and `Action::EnsureDir` all call `ensureInsideRoot(path)` (`cwSaveLoadPrivate.cpp:447, 516, 547`), which currently allows only `dataRoot` or the project file's parent. The cleanest fix is to thread the project root (not just `dataRoot`) into the `Job` struct, and check `isInsideRoot(projectRoot, target)` — that single check covers `.cwproj`, `dataRoot/`, and `GIS Layers/`. Less-invasive alternative: add `gisLayersRoot` to `Job` and `ensureInsideRoot` accepts any of `{dataRoot, gisLayersRoot, projectFile}`. Either way, plumb the new path through `cwSaveLoad::enqueueJobs`-style helpers (`cwSaveLoadPrivate.cpp:812-816, 949-953`) where `job.dataRoot` is currently set from `context->dataRoot()`.
- **`cwProject::addFiles` works unchanged.** It uses `Action::Copy` via `copyFilesAndEmitResults` (`cwSaveLoad.cpp:1960`), runs `uniqueDestinationPath` for collision rename (built-in convention: `foo-1.laz`, `foo-2.laz` — match this, not `foo (1).laz`), and ends with `saveFlushImpl()` so the new file is staged + committed. The callback's returned relative paths are computed against `dataRootDir` and come out as `../GIS Layers/foo.laz`; the laz callback discards them and calls `rescan()` instead, so the ugly form has no observable effect.
- **Bundle (`.cw` zip) save**: `saveBundledArchiveAtomic` (`cwSaveLoad.cpp:146`) zips the entire project root via `cwZip::zipDirectory`. `GIS Layers/` is included automatically — no special handling required.
- **Bundle load**: bundles are extracted to a temp dir, then loaded as a regular `.cwproj`. As long as `setGisLayersDir` is wired into the bundle-load path, scanning happens against the temp extraction dir, then again against the real path after resave.
- **Save-as (`.cwproj` → `.cwproj`)**: `copyProjectTo` (`cwSaveLoad.cpp:2203`) calls `copyDirectoryRecursively` from project root. `GIS Layers/` is copied along with `dataRoot/` — no special handling required.

## QML Changes

### `cavewherelib/qml/GeospatialLayerPage.qml`

- Line 94: `RootData.region.globalCS === ""` → `RootData.region.globalCoordinateSystem === ""`.
- `addLazFiles` function: replace the per-file `lazLayers.addLayer(paths[i])` loop with a single `RootData.region.lazLayers.addFromFiles(urls)` call. The model handles URL→path conversion, copy queueing, and rescan on completion. `RootData.importPathsFromUrls` and the per-path loop go away from this QML file.
- The bug where `sourcePath` was empty on save is fixed by `addFromFiles` copying into `GIS Layers/` before the row appears in the model — every row in the model corresponds to a real file on disk.

### Other QML files referencing `globalCS`

Rename property accesses in:
- `cavewherelib/qml/DataMainPage.qml` (lines 101, 104, 139)
- `cavewherelib/qml/CoordinatePickerPopup.qml` (lines 27, 30, 31, 126, 144) — note: this file's `picker.globalCS` is a property on `cwCoordinatePicker`; if that C++ class exposes `globalCS` it must be renamed too. Check `cavewherelib/src/cwCoordinatePicker.{h,cpp}`.

### `objectName` for tests

`globalCSComboBox` in `DataMainPage.qml:100` is referenced from `tst_FixStations_RoundTrip.qml` and `tst_DataMainPage_globalCS.qml`. Rename to `globalCoordinateSystemComboBox` and update the tests.

## Edge Cases

### Filename collisions

Handled by the existing `uniqueDestinationPath` helper (`cwSaveLoad.cpp:1930`) inside `cwProject::addFiles`. Convention is `foo-1.laz`, `foo-2.laz`, etc. (NOT `foo (1).laz`). No custom collision logic in `cwLazLayerModel` — the project layer already does this for notes.

### Empty / not-yet-saved projects

`cwProject` always has a project root (a temp dir at construction). `setGisLayersDir` is called as part of that setup, so `m_gisLayersDir` is valid before the user adds any layer. On save-as, `copyDirectoryRecursively` (`cwSaveLoad.cpp:1130`) moves `GIS Layers/` along with `dataRoot/` to the user-chosen path. No in-place fallback, no migration step needed.

### Load order

`scan()` must run after `cwSaveLoad` knows the project root, but it doesn't depend on `globalCoordinateSystem` being set yet — layers reload themselves whenever the region's CS changes via the existing `setRegionGlobalCS` propagation in `cwLazLayerModel`. So ordering is: load proto → set CS on region → `setGisLayersDir` → scan → each `cwLazLayer` kicks its async load.

### Adding a large LAZ file

The copy runs through `cwProject::addFiles` → worker queue → `Action::Copy`, off the UI thread, so a multi-GB LAZ doesn't freeze the app. The row doesn't appear in the model until after the copy completes and `rescan()` runs — same UX as notes today (the gallery doesn't show an image row until copy completes). Progress is reported via `futureToken` (`cwSaveLoad.cpp:2331`, "Adding files"), so the task UI already surfaces in-flight copies.

### Symlinks and odd inputs in `GIS Layers/`

`QDirIterator` follows symlinks by default. If a user manually symlinks a file in, it'll be picked up. The folder scan filters strictly by `*.laz`/`*.las` extension, so unknown files (`.DS_Store`, `Thumbs.db`, future `.cwgis` sidecars, stray `.txt`) are ignored without error.

### Duplicate of an already-imported file

If a user re-adds the same external `~/foo.laz` they already imported, they get a second copy as `foo-1.laz` (via `uniqueDestinationPath`). We don't dedupe by content hash — simple, predictable, and the user can remove duplicates via the existing UI.

### Modified bit + commit guarantee

Adds: `cwProject::addFiles` calls `saveFlushImpl()` at the end of its pipeline (`cwSaveLoad.cpp:2326`), which triggers `localMutationOccurred` → `setModified(true)` → eventual `commitAll` (`cwSaveLoad.cpp:4370`). The new file in `GIS Layers/` gets staged and committed.

Removes: the `rowsRemoved → saveMetadata` connection (kept from existing code) calls `saveProject`, which queues the metadata write + drives the flush + commit. The deleted file is removed from the working tree and the deletion is committed.

The `rowsInserted → saveMetadata` connection is **dropped** — `cwProject::addFiles` already triggers the save flow, and keeping `rowsInserted` connected would also fire on every load-time `rescan()`, causing redundant flushes.

### Layer identity across save/load

`cwLazLayer` has a `QUuid m_id` field generated in the constructor. The current proto schema doesn't persist it (`LazLayer` proto only has `sourcePath`), so layer IDs were already per-session — folder-scan preserves that behavior. The `cwKeywordModel` use of the ID is session-scoped (not persisted), so no regression.

### Layer ordering caveat

Alphabetical-by-filename means renaming a file on disk reorders the layer list. There is no user-facing reorder UI today, so this is consistent with current behavior. Document the implication.

## Tests

### Existing tests that need updates

- `testcases/test_cwLazLayerSaveLoad.cpp:18` ("save/load round-trip preserves sourcePath"):
  - Replace `region->lazLayers()->addLayer(lazA)` with `region->lazLayers()->addFromFiles({QUrl::fromLocalFile(lazA), QUrl::fromLocalFile(lazB)})` followed by a wait for the project's task-future to drain (the file copies are queued).
  - Currently asserts `sourcePath() == lazA` — flip to compare basenames and `QFile::exists` on the path inside `GIS Layers/`.
  - Original files are not deleted by addFromFiles (it's a copy, not a move) — add an assertion that the source paths still exist.
- Same file's other test cases (pointSize override, missing source file): update from `addLayer` to `addFromFiles` and re-check sourcePath assumptions.
- `test-qml/tst_GeospatialLayerPage.qml`: update `globalCS` → `globalCoordinateSystem` (lines 176-177); update any `addLayer` calls to `addFromFiles`.
- `test-qml/tst_FixStations_RoundTrip.qml`, `test-qml/tst_DataMainPage_globalCS.qml`: rename `globalCS` → `globalCoordinateSystem` property accesses and the `globalCSComboBox` objectName lookup.
- Any other Catch2 test serializing `ProjectMetadata.lazLayers` via proto → delete (the field is gone).
- Any test still using `lazLayers->addLayer(QString)` directly → migrate to `addFromFiles(QList<QUrl>)`, or (for tests that don't need the copy path) write the file into `<projectRoot>/GIS Layers/` and call `rescan()`.

### New tests — Save/Load (critical, since these are hardest to change once released)

- **File copied into folder on add**: in `test_cwLazLayerModel.cpp` (or new test), construct a `cwRootData` (which gives us a project with a temp project root), call `lazLayers->addFromFiles({QUrl::fromLocalFile(externalLaz)})`, wait for the project's task-future to drain, then verify:
  - `<projectRoot>/GIS Layers/<basename>` exists.
  - The original external file is untouched.
  - `layerAt(0)->sourcePath()` points at the copy inside `GIS Layers/`.
- **Folder scan on load**: write two `.laz` files directly into `<projectRoot>/GIS Layers/` (without going through `addFromFiles`), open the project, verify both surface as `cwLazLayer` rows. Models the "drop a file in via Finder" flow that's deferred but should already work.
- **Folder doesn't exist on load**: open a project that has no `GIS Layers/` directory (e.g., a project from before this change on the `fall2026` branch) → empty layer list, no error, no crash.
- **Non-laz files ignored on scan**: drop `.DS_Store`, `random.txt`, `notes.cwgis` (future sidecar) into `GIS Layers/` → all ignored, only `.laz`/`.las` surface.
- **Save-as (`.cwproj` → `.cwproj`)**: project with two laz layers, `saveAs(newPath)` → new location has its own `GIS Layers/` with both files, original location is untouched. Confirms `copyDirectoryRecursively` picks up the folder.
- **Bundled save (`.cwproj` → `.cw`)**: project with two laz layers → `saveAs("foo.cw")` → unzip the archive, verify `GIS Layers/` and both files are inside, including any LFS pointer transformation if applicable.
- **Bundled load (`.cw` → memory)**: open a `.cw` bundle that contains `GIS Layers/foo.laz` → folder scan against the temp extraction dir surfaces the layer; verify `pointCount() > 0` after load completes.
- **Bundle modify-then-resave**: open `.cw` bundle, `addFromFiles` a layer, save → final bundle includes the new layer file. Tests the `BundledArchivePath` resave flow with the new dir wiring.
- **Modified bit flips on add**: after `addFromFiles` + future drain, `project->modified()` is true. After save, it's false again.
- **Git commit picks up the new file**: after `addFromFiles` + `saveAs`, query the repo HEAD tree and verify `GIS Layers/foo.laz` is committed, not just sitting untracked in the working directory.
- **Git commit picks up the deletion**: `addFromFiles` + save → `removeAt(0)` + save → verify the file is gone from disk AND the deletion is committed (working tree clean post-save, not showing a deleted-but-uncommitted file).
- **Collision auto-rename**: `addFromFiles` the same source file twice → second copy lands as `foo-1.laz`. Third → `foo-2.laz`. Matches `uniqueDestinationPath`'s convention.
- **`removeAt` deletes file from disk**: after `removeAt(0)` and queue drain, `QFile::exists(...)` returns false for the layer's previous path.
- **No project found**: bare `cwLazLayerModel` with no parent → `addFromFiles` returns early, model stays empty, no crash.
- **Round-trip + reprocess** (already exists at `test_cwLazLayerSaveLoad.cpp:18`, update to use `addFromFiles`): after save → reload, layers are present, file lives inside `GIS Layers/`, and `cwLazLoader` re-runs to produce `pointCount() > 0`.
- **Redundant-save guard**: instrument the save count, perform a project load that fires `rescan()`, verify zero extra `saveProject` calls beyond what the load itself triggers. Catches accidental reintroduction of the `rowsInserted → saveMetadata` hook.

### New tests — LFS

- **LFS attributes**: extend `test_cwProject.cpp:2197` (or `:2871`) to also assert `.gitattributes` contains `*.laz filter=lfs diff=lfs merge=lfs -text` and `*.las filter=lfs diff=lfs merge=lfs -text`. Trivial line additions to existing tests, no new fixture.
- **LFS pointerization for laz on disk**: after `addLayer` + save + commit on a repo with LFS configured, verify the committed blob for `GIS Layers/foo.laz` is an LFS pointer file (small text file referencing the OID), not the raw LAZ bytes.
- **LFS upload end-to-end for laz**: add a `.laz` to `test_cwProject.cpp:3012` ("uploads LFS objects through test LFS server") and assert it's uploaded as an LFS object. Confirms the laz extension goes through the LFS pointerization pipeline end-to-end, not just transitively.

### Migration / once-only tests

- **Project on `fall2026` with old `lazLayers` proto field**: not a supported scenario per the plan (no backwards compat), but the *load* path should at least not crash if it encounters an old proto with the now-removed field. Protobuf's unknown-field handling already covers this — proto3 silently ignores fields it doesn't know about. No test needed beyond confirming the proto definition removes the field number cleanly (don't reuse field 6 for something else).

Run both suites end-to-end before declaring done, per CLAUDE.md.

## Explicitly Deferred

- `.cwgis` sidecar files for per-layer metadata (visibility, ordering, CS override) — add when there's actual metadata to persist.
- Live folder watching (drop a `.laz` while the app is open and have it appear). For now, picked up only on project load.
- Migration of existing `fall2026` projects that have `lazLayers` in their proto — branch is unreleased, no migration needed.
- Richer error UX when `addLayer` fails to copy (disk full, permissions, etc.).

## Order of Implementation

1. Proto rename + `lazLayers` removal; regenerate protos; fix compile errors in `cwSaveLoad`.
2. `cwCavingRegion` rename pass (`globalCS` → `globalCoordinateSystem`).
3. `cwSaveLoadPrivate`: widen `isInsideRoot` (thread project root or add `gisLayersRoot` to `Job`); update `enqueueJobs` helpers to populate the new field.
4. `cwLazLayerModel`: add `project()` parent-walker, `setGisLayersDir`, `rescan`, `addFromFiles(QList<QUrl>)` that wraps `cwProject::addFiles`; drop public `addLayer` invokable, `writeTo`, `readFrom`.
5. `cwSaveLoad`: wire `setGisLayersDir` from newProject/load/save-as/bundle paths; add `rowsAboutToBeRemoved` connection that queues `Action::Remove` for each layer's `sourcePath()`; **drop** the `rowsInserted → saveMetadata` connection; **keep + repurpose** the `rowsRemoved → saveMetadata` connection.
6. `cwCoordinatePicker`: rename `globalCS` Q_PROPERTY and member.
7. QML pass:
   - `globalCS` → `globalCoordinateSystem` everywhere.
   - `GeospatialLayerPage.qml::addLazFiles`: replace the per-path `addLayer` loop with a single `lazLayers.addFromFiles(urls)`.
8. Test updates + new tests.
9. Full C++ + QML test suites (`tee` both to `/tmp/` logs per CLAUDE.md).
