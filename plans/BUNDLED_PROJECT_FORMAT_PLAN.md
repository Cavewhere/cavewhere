# Bundled `.cw` Format Plan

## 1. Scope

Implement dual project persistence modes while keeping one internal runtime model (git repository directory):

- Bundled format: `.cw` (zip archive containing full project directory, including `.git` history)
- Unbundled format: `.cwproj` (project file inside a normal directory repo, current behavior)
- Legacy sqlite `.cw` load: auto-convert into a temporary git project directory, then save back to bundled `.cw` silently on Save

This plan is based on current code paths in:

- `cavewherelib/src/cwProject.cpp` (`projectType`, `loadOrConvert`, `save`, `saveAs`, sqlite conversion helpers)
- `cavewherelib/src/cwSaveLoad.cpp` (`load`, `transferProjectTo`, `moveProjectTo`, `copyProjectTo`, `commitProjectChanges`, queue model)
- `cavewherelib/src/cwZip.cpp` (`zipDirectory`, `extractAll`)
- `cavewherelib/qml/SaveAsDialog.qml` and `LoadProjectDialog.qml`

## 2. Decisions (Aligned With Discussion)

1. Use `.cw` for bundled format and `.cwproj` for unbundled format.
2. Keep full `.git` history inside bundled `.cw`.
3. No locking in this phase (applies to both `.cw` and `.cwproj` in future phase).
4. Add a migration flag named around sqlite origin (`migratedFromSqlite`) rather than `legacy`.
5. Keep zip packaging serialized with existing save pipeline semantics (no concurrent package writes for same project).

## 3. Format Detection Strategy

Current `cwProject::projectType()` identifies:

- `.cwproj` => git format
- otherwise tries sqlite open/query => sqlite
- else unknown

That is insufficient once `.cw` can be either sqlite or bundled zip.

### New detection order

1. If file extension is `.cwproj`: `GitDirectoryFileType` (existing behavior).
2. If extension is `.cw`:
- Check zip signature and attempt bundle probe.
- Bundle probe succeeds if archive contains exactly one `.cwproj` candidate (or a deterministic top-level project root containing one `.cwproj`).
- If bundle probe fails, fallback to sqlite probe.
3. Else fallback to existing sqlite probe and unknown handling.

### Marker discussion

A `.cwproj` inside archive is the phase-1 bundle marker/entrypoint. No separate manifest file is required for this phase.

## 4. Runtime Session Model

Add project persistence/session metadata (likely on `cwProject`, backed by `cwSaveLoad` state where needed):

- `StorageMode`:
  - `UnbundledDirectory`
  - `BundledArchive`
- `loadedPath`: user-facing file path (`.cw` or `.cwproj`)
- `workingProjectFile`: absolute path to active `.cwproj` in working directory (already represented by `cwSaveLoad::fileName()`)
- `bundleContext` (for bundled mode):
  - `bundleFilePath` (`.cw`)
  - `migratedFromSqlite` (bool)

Behavioral rule:

- App always edits `workingProjectFile`/working root repo.
- Save target is determined by `StorageMode`, not just current working filename.

## 5. Load Flows

## 5.1 Load `.cwproj` (unbundled)

No functional change:

- `loadOrConvert` -> `loadHelper` -> `m_saveLoad->load(filename)`
- `StorageMode = UnbundledDirectory`
- `migratedFromSqlite = false`

## 5.2 Load bundled `.cw`

1. Create dedicated temp working root (`QTemporaryDir`, `setAutoRemove(false)`).
2. Extract `.cw` using `cwZip::extractAll` into working root.
3. Locate project `.cwproj` deterministically.
4. Load via existing `loadHelper(workingProjectFilePath)`.
5. Set session mode to `BundledArchive`, keep original `bundleFilePath` as save target.

Notes:

- Use extraction hardening already in `cwZip` plus explicit path safety assertions (no traversal/outside-root writes).
- Keep `.git` directory intact so commit/sync features continue to work on working repo.

## 5.3 Load sqlite `.cw` (legacy)

Current behavior converts into temp repo via `convertFromProjectV6Helper` and marks temporary.

Adjust behavior:

1. Keep conversion implementation (`saveAllFromV6`) as core migration path.
2. Set session mode to `BundledArchive` with `bundleFilePath = original sqlite file path`.
3. Set `migratedFromSqlite = true`.
4. Keep UI silent on subsequent Save: save overwrites original path as bundled `.cw`.

This preserves your desired user flow while retaining existing conversion logic.

## 6. Save and Save-As Flows

## 6.1 Save (`cwProject::save`)

Current behavior:

- `waitForFinished()`
- `commitProjectChanges()`
- emit `fileSaved`

New behavior:

1. Flush pending file jobs (`waitForFinished`).
2. Commit working repo changes (`commitProjectChanges`).
3. If `StorageMode == BundledArchive`, enqueue a packaging `Custom` job in the existing filesystem job queue.
4. Ensure ordering at operation boundary: `saveFlush -> commit -> package job -> saveFlush`.
5. Keep queue serialization: if packaging is in progress, subsequent save requests queue behind it.

## 6.2 Save As

Support explicit target mode selection:

- Save As `.cwproj` => unbundled directory target
- Save As `.cw` => bundled target

Behavior matrix:

1. Unbundled -> Unbundled:
- Use/refactor existing `copyProjectTo` behavior.

2. Unbundled -> Bundled:
- Copy/move to staging working directory as needed.
- Create bundle `.cw` from repo root.
- Switch session mode to bundled and point target to new `.cw`.

3. Bundled -> Bundled:
- Repoint bundle target path and package working root there.
- No re-extract required.

4. Bundled -> Unbundled:
- Materialize working root to chosen directory `.cwproj` target.
- Switch session mode to unbundled.

## 7. Atomic Bundled Write Strategy

Implement archive write helper (new utility in `cwSaveLoad` or a dedicated bundle helper):

1. Build archive at `target.cw.tmp` in same directory as target.
2. Ensure bytes flushed/closed successfully.
3. Optional safety backup via rename strategy:
- If `target.cw` exists, rename to `target.cw.bak` (same directory).
4. Rename `target.cw.tmp` -> `target.cw` (atomic on same volume).
5. If step 4 fails and `.bak` exists, attempt rename-back `.bak` -> `target.cw`.
6. Best-effort cleanup of stale `.tmp` / `.bak`.

Packaging mode:

- Default to ZIP store (no compression) for faster save latency.
- Keep a future option to support compressed packaging when smaller file size is preferred.

Rationale:

- Prefer rename over copy for backup path due to speed and lower partial-write risk.
- Yes, rename can also fail; rollback path still improves recoverability vs direct overwrite.

## 8. API and Type Changes

## 8.1 `cwProject` changes

1. Extend `FileType` enum:
- `BundledGitFileType`
- `SqliteLegacyFileType`
- `GitDirectoryFileType`
- `UnknownFileType`

2. Update `projectType(QString)` to implement detection order above.
3. Add session state fields for storage mode + bundle context.
4. Update `loadOrConvert`, `loadFile`, `save`, `saveAs` to branch by storage mode.

## 8.2 `cwSaveLoad` changes

1. Keep existing project-in-directory semantics unchanged.
2. Add helper entrypoints for bundle-specific operations:
- `ResultBase packageProjectDirectoryToBundle(const QDir& projectRoot, const QString& bundlePath)`
- `Result<BundleLoadResult> extractBundleToWorkingDir(const QString& bundlePath, const QDir& workingRoot)`
3. Keep `transferProjectTo` for directory targets; do not overload it with bundle logic unless refactor is clean.

## 8.3 Runtime migration state

- Keep `migratedFromSqlite` as runtime session state only.
- Do not change `cavewhere.proto` in this phase.
- Save target mode remains runtime session state, not proto-driven.

## 9. QML/UI Plan

## 9.1 Load dialog

`LoadProjectDialog.qml` currently treats `.cw` as sqlite conversion path.

Update:

- `projectType` branching must include bundled `.cw` path and load directly (no conversion dialog).
- Only show conversion dialog for actual sqlite legacy files.

## 9.2 Save As dialog

`SaveAsDialog.qml` currently filters only `*.cwproj`.

Update:

- Add both filters:
  - `CaveWhere Bundled Project (*.cw)`
  - `CaveWhere Project Directory (*.cwproj)`
- Preserve user’s chosen extension and pass it to `RootData.project.saveAs(...)`.

## 10. Concurrency and Queueing

- Reuse existing save/load operation queue semantics in `cwSaveLoad::Data::enqueueOperation`.
- Packaging job should be serialized with save flush + commit.
- For now, no parallel package writes per project instance.
- Keep room for future background packaging + progress UI, but correctness first.

## 11. Tests

## 11.1 Unit/integration tests (C++)

1. Detection:
- `.cwproj` recognized as git directory type.
- zipped `.cw` recognized as bundled type.
- sqlite `.cw` recognized as sqlite legacy type.
- corrupted `.cw` handled as unknown/error.

2. Bundled load/save round-trip:
- Load bundled `.cw` -> modify -> `save()` -> reopen `.cw` and verify model + git history exists.

3. Save atomicity:
- simulate failure before final rename, ensure original `.cw` remains readable.
- simulate failure after backup rename, ensure rollback path restores target if possible.

4. Save As matrix:
- `.cwproj -> .cwproj`, `.cwproj -> .cw`, `.cw -> .cw`, `.cw -> .cwproj`.

5. Sqlite conversion:
- load sqlite `.cw` -> no explicit Save As needed -> `save()` writes bundled `.cw` at original path.
- verify runtime `migratedFromSqlite` session state behavior.

6. Git history preservation:
- multiple commits in working repo survive bundling/unbundling cycle.

## 11.2 QML tests

1. Load dialog chooses direct open for bundled `.cw`.
2. Load dialog shows conversion chooser only for sqlite `.cw`.
3. Save As supports both extensions and writes expected target format.

## 12. Rollout Plan

## Phase 1A: Detection + bundled load

- Add bundled detection and extraction/load path.
- Keep save behavior unchanged except blocking unsupported save target combinations with explicit errors.

## Phase 1B: Detection/load fixtures and validation

- Add `.cw` bundle fixture coverage for detection and open paths.
- Validate that bundled load works before implementing bundled save.

## Phase 2: Bundled save + atomic writer

- Add packaging write pipeline for bundled sessions.
- Add recovery/rollback handling around temp/backup rename.

## Phase 3: Save As dual-format

- Implement full format conversion matrix and QML Save As updates.

## Phase 4: sqlite silent migration completion

- Wire runtime `migratedFromSqlite` state and forced bundled save target.
- Add regressions for readonly/original-path overwrite behavior.

## Phase 5: Hardening

- Performance profiling for large bundles.
- Retry/backoff and user-visible diagnostics for package failures.
- Expand bundled fixture coverage for malformed archives:
  - no `.cwproj` entry
  - multiple `.cwproj` entries
  - corrupted archive payload/metadata
- Add broader UI/open-path regressions beyond the core `cwProject` detection/load test.
- Optional future lock design (deferred by decision).

## 13. Deferred Items

1. Cross-process lock files for `.cw` and `.cwproj`.
2. Background packaging progress UI and cancelation policy.
3. Periodic/prunable backup retention policy.
4. Optional bundle-level metadata file only if `.cwproj` marker proves insufficient later.
