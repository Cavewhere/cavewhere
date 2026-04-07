# Save/Load Architecture Audit (v9 Format)

Date: 2026-04-03

## Context

The v9 save/load format is a complete redesign: directory-of-JSON-protobuf-files backed by git, replacing the monolithic SQLite blob. Once users start using it, the on-disk format lives forever. This audit reviews the design for extensibility, correctness, forward/backward compatibility, and dead code.

## Format Summary

```
project.cwproj              (Project JSON proto: metadata, version, UUID)
dataRoot/
  cave-name/
    cave-name.cwcave        (Cave JSON proto: name, id, units)
    trips/
      trip-name/
        trip-name.cwtrip    (Trip JSON proto: survey data, calibrations, team)
        notes/
          note.cwnote       (Note JSON proto: image ref, scraps, rotation)
          note.cwnote3d     (NoteLiDAR JSON proto)
          image.jpg          (raw image, git-LFS tracked)
```

All `.cwproj`, `.cwcave`, `.cwtrip`, `.cwnote`, `.cwnote3d` files are human-readable JSON (written via `MessageToJsonString` with `add_whitespace = true`).

---

## Strengths (keep as-is)

1. **Per-entity versioning with forward-compatibility guard** -- Every entity carries `FileVersion`. `checkEntityVersion()` tracks max version across all entities; `saveWillCauseDataLoss()` blocks saving when any entity was written by a newer CaveWhere.

2. **StationShot union pattern** -- Merging Station and Shot into a single `StationShot` message with well-spaced field numbers (100s for station, 1000s for shot) and interleaving in the `leg` repeated field makes JSON human-readable.

3. **UUID-based identity with repair** -- Every entity has a UUID. `repairTopLevelIds()` / `repairNestedScrapIds()` heal missing or duplicate UUIDs at load time, critical for git merge correctness.

4. **Atomic bundle writes** -- `.cw.tmp` -> rename -> `.cw.bak` cleanup in `saveBundledArchiveAtomic()`.

5. **Git-LFS for images** -- Binary assets go through LFS with an extension-based policy.

6. **Version checking at all load levels** -- `loadAll()` calls `checkEntityVersion()` for every cave, trip, and note. The sync pipeline has its own project-level version guard that checks `pulledVersion > supportedVersion` before entity-level loading.

---

## Recommendations

### 1. CRITICAL: `JsonStringToMessage` rejects unknown fields — forward compatibility broken

**Status:** DONE — Fixed in dev

**Files:** `cwSaveLoad.cpp:800`, `cwCaveSyncMergeHandler.cpp:48`, `cwCavingRegionSyncMergeHandler.cpp:26`

```cpp
// cwSaveLoad.cpp line 800
const auto status = google::protobuf::util::JsonStringToMessage(jsonPayload, &proto);
```

No `JsonParseOptions` is passed. The default has `ignore_unknown_fields = false`. This means:

1. CaveWhere v10 adds a new field (e.g., `optional string color = 10` to `Scrap`)
2. v10 saves the file — the JSON contains `"color": "red"`
3. CaveWhere v9 tries to load it — `JsonStringToMessage` **rejects the entire file** with a parse error because `color` is an unknown JSON key

The `saveWillCauseDataLoss()` guard is useless here because the file fails to parse before version checking (`checkEntityVersion`) ever runs. The user gets a cryptic "Failed to parse" error instead of the intended version mismatch warning.

The same issue exists in the sync merge handlers:
- `cwCaveSyncMergeHandler.cpp:48` — parses cave protos from git blob content
- `cwCavingRegionSyncMergeHandler.cpp:26` — parses project protos from git blob content

Both call `JsonStringToMessage` without parse options. A sync pull that introduces a newer-version entity would fail the merge handler entirely.

**Sync version guard is also broken by this:** The sync pipeline's version guard at `cwSaveLoad.cpp:5962-5969` checks `pulledVersion > supportedVersion` and returns `IncompatibleProjectVersion`. However, this check runs AFTER `loadProject()` at line 5956 — which calls `loadMessage<T>()` — which calls `JsonStringToMessage` without `ignore_unknown_fields`. If the pulled project contains unknown fields from a newer version, the load fails with a parse error before the version guard ever runs. Fixing `ignore_unknown_fields` is a prerequisite for the sync version guard to function correctly.

**The intended forward-compatibility architecture is:**
- Layer 1: Parse succeeds, ignoring unknown fields → file loads, data is viewable
- Layer 2: `saveWillCauseDataLoss()` blocks saving → no data loss from re-serialization

Layer 1 is currently broken.

**Fix:**
```cpp
google::protobuf::util::JsonParseOptions parseOptions;
parseOptions.ignore_unknown_fields = true;
const auto status = google::protobuf::util::JsonStringToMessage(jsonPayload, &proto, parseOptions);
```

Apply to all three call sites:
- `cwSaveLoad.cpp` `loadMessage<T>()`
- `cwCaveSyncMergeHandler.cpp` `loadCaveFromContent()`
- `cwCavingRegionSyncMergeHandler.cpp` `loadProjectFromContent()`

**Action items:**
- [ ] Add `JsonParseOptions` with `ignore_unknown_fields = true` to `loadMessage<T>()` in `cwSaveLoad.cpp`
- [ ] Add same to `cwCaveSyncMergeHandler.cpp` `JsonStringToMessage` call
- [ ] Add same to `cwCavingRegionSyncMergeHandler.cpp` `JsonStringToMessage` call
- [ ] Add test: create a JSON file with an extra unknown field, verify it loads successfully and triggers version warning (not parse failure)

---

### 2. Name-based path collisions can cause data loss

**Status:** DONE
**Depends on:** 3 (fix sanitizer bug before adding collision detection)

**Files:** `cwSaveLoad.cpp`, `cwCave.cpp`, `cwTrip.cpp`, `cwNote.cpp`

File paths are derived from entity names via `sanitizeFileName()`. Two caves named `"Big Cave!"` and `"Big Cave?"` both sanitize to `"Big Cave_"`, creating a directory collision. The second cave's `.cwcave` file silently overwrites the first. Same applies to trips and notes.

**Strategy: Hybrid prevent + repair**

Rather than embedding disambiguators or UUIDs in directory names (which hurts human-readable browsing), we prevent collisions at the source and add a safety net at load time.

**Layer 1 — Prevent in `setName()`:** The UI should sanitize names before calling `setName()`, so `setName()` receives already-sanitized input. `setName()` adds a guard: check siblings for duplicates of the sanitized name:
- `cwCave::setName()`: compare sanitized name against `parentRegion()->caves()` (skip self)
- `cwTrip::setName()`: compare sanitized name against `parentCave()->trips()` (skip self)
- `cwNote::setName()`: compare sanitized name against sibling notes in the same trip (skip self)
- `cwLiDARNote`: same pattern as `cwNote`

If a collision is detected, reject the rename (keep the old name). `setName()` stays as-is — a simple setter that silently no-ops (consistent with how it already handles empty strings).

Note: `cwNote::setName()` currently has no empty-string guard unlike `cwCave` and `cwTrip`. An empty note name would sanitize to `"untitled"`, potentially colliding with other untitled notes. Add an empty-string guard to match the other setters.

**UI validation helper:** Add a separate Q_INVOKABLE validation function (e.g., `QString validateName(const QString& proposedName) const`) that the UI calls before or after a rename attempt. It returns an empty string if the name is valid, or a human-readable reason for rejection (e.g., `"A cave named 'Big Cave' already exists"`, `"Name contains invalid characters that were removed"`). This keeps `setName()` simple and lets QML show inline error messages.

Note: `parentRegion()` / `parentCave()` may be null during construction or import. Guard with null checks — only enforce uniqueness when a parent exists.

**Layer 1b — Deduplicate in `addCave()` / `insertCave()` / `addTrip()` / `insertTrip()`:** The `setName()` guard alone doesn't protect the import path. Survey importers (Compass, Survex, Walls) call `setName()` *before* adding the entity to a parent, so the null-parent guard skips validation. By the time `addCave()` is called, the name is already set and unchecked.

Fix: When `cwCavingRegion::insertCave()` adds a cave, check if its sanitized name collides with an existing sibling. If so, auto-rename by appending `" 2"`, `" 3"`, etc. until a unique sanitized name is found. Same for `cwCave::insertTrip()` and note addition paths. This catches all entry points — UI renames, imports, programmatic adds — regardless of whether `setName()` was called before or after parenting.

**Auto-rename suffix convention (used consistently across all layers):** `"Name 2"`, `"Name 3"`, etc. — space followed by incrementing integer. Scan existing siblings to find the first unused suffix. Example: if `"Big Cave"` and `"Big Cave 2"` already exist, the next collision produces `"Big Cave 3"`.

Note: Layer 1b only applies to the name the entity *already has* at insertion time. It does not interact with subsequent `setName()` calls — those are guarded by Layer 1. If `setName()` is called *after* parenting (e.g., a user renames a cave in the UI), Layer 1 rejects the collision; Layer 1b does not fire again.

Note: `cwImportTreeDataDialog::updateImportErrors()` has a comment saying "check for name collision" but the body is empty — this was planned but never implemented. The `insertCave()`/`insertTrip()` deduplication makes that function unnecessary for name collision purposes.

**Layer 2 — Repair on load:** In `loadAll()`, after discovering all `.cwcave`/`.cwtrip` files, detect sibling entities with colliding sanitized names and auto-rename the later one using the standard suffix convention (`"Name 2"`, `"Name 3"`, etc.). "Later" means second in lexicographic order of absolute file path — this is how `loadAll()` sorts all entities via `filePathLess` (`absoluteFilePath() < absoluteFilePath()`), making the ordering deterministic and platform-stable. Report the rename to the user via `cwProject::errorModel()` (`cwErrorListModel`), which surfaces warnings in the main window's dialog. This catches edge cases from filesystem-level duplication (e.g., a user manually copies a cave directory). In that case, `repairTopLevelIds()` assigns a new UUID to the copy, but both caves still have the same name — on next save, one would silently overwrite the other. The existing save rename machinery (`enqueueRenameIfNeeded()` / `moveOrMergeDirectory()`) handles moving the old directory to the new name. Note: git merge collisions are already handled by the merge reconciliation system (`cwCaveSyncMergeHandler`, `cwTripSyncMergeHandler`) which resolves conflicts by UUID and cleans up orphaned directories.

**Layer 3 — Deep UUID regeneration for copied subtrees:** When `repairTopLevelIds()` detects a duplicate cave UUID (not just null — an actual collision), the entire subtree is a filesystem copy. Currently only the top-level entities (caves, trips, notes, NoteLiDAR) get new UUIDs. Nested entities keep their duplicate UUIDs:

| Entity | Repaired today? | Scope |
|--------|----------------|-------|
| Cave, Trip, Note, NoteLiDAR | Yes | Global set per region |
| Scrap | **No** | Not checked at all |
| NoteStation, Lead | **No** | Per-scrap local set — duplicates in different scraps go undetected |
| LiDAR Station | **No** | Per-LiDAR-note local set — same issue |

This is a correctness problem: two scraps in the same project sharing a UUID could confuse sync merge handlers, and any future feature that indexes by nested UUID globally would silently collide. Report deep UUID regeneration to the user via `cwProject::errorModel()` so they know a copied subtree was detected and repaired.

**Fix:** When `repairTopLevelIds()` regenerates a cave's UUID due to duplication, walk that cave's entire subtree and regenerate all child UUIDs — trips, notes, scraps, stations, leads, LiDAR stations. The trips and notes will already get new UUIDs from the global sets, but scraps and their children need explicit regeneration. This is a targeted "deep regenerate" only for detected copy subtrees — the existing per-scrap local set logic for normal loads remains unchanged.

**Action items:**
- [x] Add sibling duplicate check in `cwCave::setName()`, `cwTrip::setName()`, `cwNote::setName()`, `cwLiDARNote` — reject rename if sanitized name collides with a sibling (with null-parent guard)
- [x] Add name deduplication in `cwCavingRegion::insertCave()` — if the new cave's sanitized name collides with an existing sibling, auto-rename (append " 2", " 3", etc.)
- [x] Add name deduplication in `cwCave::insertTrip()` — same pattern for trips within a cave
- [x] Add name deduplication in note addition paths — same pattern for notes within a trip
- [x] Add empty-string guard to `cwNote::setName()` to match `cwCave`/`cwTrip` behavior
- [x] Add `Q_INVOKABLE QString validateName(const QString& proposedName) const` to `cwCave`, `cwTrip`, `cwNote` — returns empty string if valid, human-readable rejection reason otherwise
- [x] Add load-time collision repair in `loadAll()` — detect and auto-rename duplicates, emit warning via `cwProject::errorModel()`
- [x] In `repairTopLevelIds()`, when a cave UUID is regenerated as a duplicate (not null), walk the entire cave subtree and regenerate all nested UUIDs: scraps, NoteStations, leads, LiDAR stations
- [x] Add test: `setName()` rejects a name that collides with a sibling's sanitized name
- [x] Add test: `insertCave()` auto-renames a cave whose sanitized name collides with an existing sibling
- [x] Add test: importing two caves with names that differ only in forbidden chars (e.g., "Big Cave!" and "Big Cave?") produces two caves with distinct sanitized names
- [x] Add test: `validateName()` returns correct rejection reason for collisions and sanitization changes
- [x] Add test: two caves with names that sanitize identically both survive round-trip via load repair
- [x] Add test: two trips within the same cave with colliding sanitized names
- [x] Add test: `setName()` allows the rename when no parent is set (construction/import path)
- [x] Add test: `setName()` after parenting rejects collision even when `insertCave()` previously accepted the entity (Layer 1 vs 1b interaction)
- [x] Add test: auto-rename suffix increments correctly — inserting 3 caves that all sanitize to `"Big Cave"` produces `"Big Cave"`, `"Big Cave 2"`, `"Big Cave 3"`
- [x] Add test: auto-rename skips already-taken suffixes — if `"Big Cave"` and `"Big Cave 2"` exist, next collision produces `"Big Cave 3"`
- [x] Add test: filesystem-copied cave has all nested UUIDs (scraps, stations, leads) regenerated

---

### 3. `sanitizeFileName` strips both ends simultaneously

**Status:** DONE — Fixed in dev

**Files:** `cwSaveLoad.cpp` (`sanitizeFileName()`), `testcases/ProjectFilenameTestHelper.cpp` (duplicate implementation)

```cpp
while (input.startsWith('.') || input.endsWith('.')) {
    input = input.mid(1).chopped(1);  // BUG: strips from both ends each iteration
}
```

Input `".X"` strips leading `.` AND trailing `X` in one iteration, producing `""` -> `"untitled"`. Each direction should be handled separately:

```cpp
while (input.startsWith('.')) input = input.mid(1);
while (input.endsWith('.'))  input.chop(1);
```

`ProjectFilenameTestHelper.cpp` contains a duplicate copy of `sanitizeFileName()` with the same bug. Remove the duplicate and have the test helper call `cwSaveLoad::sanitizeFileName()` directly.

**Action items:**
- [ ] Fix the loop to strip leading and trailing dots independently
- [ ] Remove duplicate `sanitizeFileName()` from `ProjectFilenameTestHelper.cpp`, replace with calls to `cwSaveLoad::sanitizeFileName()`
- [ ] Add unit tests for `cwSaveLoad::sanitizeFileName()`: `".X"` -> `"X"`, `"..foo.."` -> `"foo"`, `"..."` -> `"untitled"`

---

### 4. Save `lengthUnit` / `depthUnit` in `toProtoCave()`

**Status:** DONE — Fixed in dev

**File:** `cwSaveLoad.cpp`, `toProtoCave()` and v9 load path

`toProtoCave()` only writes name, id, and fileVersion. The `lengthUnit` and `depthUnit` fields defined in the Cave proto are never written in v9. `cwRegionSaveTask::saveCave()` writes them, but that code only runs in the legacy `saveCavingRegion()` path. These properties were likely forgotten during the V6-to-v9 port.

Both the write AND read paths are missing: the v9 `loadAll()` path also never reads `lengthUnit`/`depthUnit` from loaded cave data. The legacy `cwRegionLoadTask::loadCave()` does read them, but that only applies to V6 SQLite files.

If caves carry unit preferences, those are silently lost on save and get proto3 defaults (0 = Inches) on load. Confirmed: `Units::LengthUnit` enum value 0 is `Inches` in both `cavewhere.proto` and `cwUnits.h`. A user who sets their cave to Meters will silently get Inches after a save/load cycle.

When loading existing v9 files that lack `lengthUnit`/`depthUnit`, default to **Meters** (not the proto default of Inches). Most cave survey data worldwide is metric, and defaulting to Inches would silently corrupt unit interpretation for the majority of users. The v9 load path should check `has_lengthunit()` / `has_depthunit()` and apply the Meters default when absent.

**Action items:**
- [ ] Add `lengthUnit` and `depthUnit` writes to `cwSaveLoad::toProtoCave()`
- [ ] Add corresponding reads in the v9 `loadAll()` cave-loading section — default to Meters when fields are absent
- [ ] Add a round-trip test for cave units (specifically: set to Meters, save, load, verify not Inches)
- [ ] Add test: loading a v9 file without unit fields defaults to Meters

---

### 5. Remove per-shot calibration overrides (dead feature)

**Status:** DONE — Fixed in dev

**Files:** `cwSurveyChunk.h`, `cwSurveyChunk.cpp`, `cwRegionSaveTask.cpp`, `cwRegionLoadTask.cpp`, `cwSaveLoad.cpp`, `cavewhere.proto`, `cwSurveyChunkData.h`, `cwSurveyChunkSignaler.h/cpp`, `cwLinePlotManager.cpp`, `cwSurveyDataArtifact.h/cpp`, `cwSurvexImporter.cpp`, `cwSurvexExporterTripTask.cpp`, `cwSurvexExporterRule.cpp`, `cwCompassExporterCaveTask.cpp`, `cwSurveyEditorModel.cpp`, `cwTripMergeApplier.cpp`

Per-shot calibration overrides (`SurveyChunk.calibrations`, proto field 3) were over-engineered for a Survex import path that never materialized. The feature was poorly designed, no UI exists to create or edit per-shot calibrations, and the v9 load path never reads them — so any overrides written to disk are silently dropped on reload and permanently lost on re-save. Rather than implementing the missing load code, remove the feature entirely.

**Action items:**
- [x] Remove `cwSurveyChunk::calibrations()`, `addCalibration()`, `calibrationsChanged` signal, and related members
- [x] Remove calibration write loop in `cwRegionSaveTask::saveSurveyChunk()`
- [x] Remove calibration loading in `cwRegionLoadTask::loadSurveyChunk()`
- [x] Remove `calibrationsChanged` signal connection in `cwSaveLoad.cpp`
- [x] In `cavewhere.proto`: mark `SurveyChunk` field 3 (`calibrations`) as `reserved`; add comment
- [x] Remove `ChunkCalibration` message from `cavewhere.proto`
- [x] Remove `cwSurveyChunkData::calibrations` member
- [x] Remove chunk calibration support from all consumers: signaler, exporters, editor model, data artifact, merge applier
- [x] Update survex importer to apply mid-survey calibrations to trip-level calibration
- [x] Remove chunk calibration tests, update survex import test

---

### 6. Remove binary protobuf loading from v9 path

**Status:** DONE — Fixed in dev (combined with #1)

**Files:** `cwSaveLoad.cpp` (`loadMessage<T>()`), `cwCaveSyncMergeHandler.cpp` (`parseProtoCave()`), `cwCavingRegionSyncMergeHandler.cpp` (`parseProjectProto()`)

The v9 format is JSON-only. `loadMessage<T>()` tries `ParseFromArray` (binary) first, then falls back to `JsonStringToMessage`. Since files are always JSON, the binary attempt always fails. The theoretical risk of `ParseFromArray` succeeding on JSON bytes as valid wire format is near-impossible in practice, but the binary-then-JSON fallback pattern is confusing — it suggests binary files are expected when they aren't, and a future developer might try to "fix" it by making binary loading actually work. Remove for code clarity.

The same binary-before-JSON pattern exists in the sync merge handlers:
- `cwCaveSyncMergeHandler.cpp:42` — `parseProtoCave()` calls `ParseFromArray` before `JsonStringToMessage`
- `cwCavingRegionSyncMergeHandler.cpp:22` — `parseProjectProto()` calls `ParseFromArray` before `JsonStringToMessage`

**Action items:**
- [ ] Remove `ParseFromArray` call from `loadMessage<T>()` in `cwSaveLoad.cpp`
- [ ] Remove `ParseFromArray` call from `parseProtoCave()` in `cwCaveSyncMergeHandler.cpp`
- [ ] Remove `ParseFromArray` call from `parseProjectProto()` in `cwCavingRegionSyncMergeHandler.cpp`
- [ ] Parse JSON directly via `JsonStringToMessage` in all three

---

### 7. Fix `ensurePathForFile` silent-failure bug

**Status:** DONE — Fixed in dev

**File:** `cwSaveLoad.cpp`, `Data::ensurePathForFile()` (~line 1448)

```cpp
if (!dir.exists()) {
    bool success = dir.mkpath(".");
    if(success) {
        ResultBase();        // missing return
    } else {
        ResultBase(...);     // missing return
    }
}
return ResultBase();         // always returns success
```

Both branches construct a `ResultBase` without returning it. The function always reports success even if `mkpath` fails. This is called from the `WriteFile` job handler (~line 1188), so directory creation failures are silently swallowed and the subsequent file write fails with a misleading error.

**Action items:**
- [ ] Add `return` before both `ResultBase(...)` calls

---

### 8. `TripCalibration` boolean fields lack presence checks

**Status:** DONE — Fixed in dev

**File:** `cwSaveLoad.cpp:4202-4218`

`fromProtoTripCalibration()` reads boolean fields without `has_*()` checks:

```cpp
tripCalibration.setFrontSights(proto.frontsights());   // defaults to false if absent
tripCalibration.setBackSights(proto.backsights());     // defaults to false if absent
```

Since these fields are declared as `optional bool` in proto3, `has_frontsights()` and `has_backsights()` ARE available but not used. If a `.cwtrip` file is missing these fields (corruption, hand-editing, partial write, or a third-party tool), both front and back sights are disabled. The trip would appear to have no survey processing enabled.

For `frontSights`, `false` is an incorrect default — most surveys have front sights enabled. A missing field should default to `true` for `frontSights`. The fix is cheap and defensive.

**Action items:**
- [ ] Add presence checks with sensible defaults:
  ```cpp
  tripCalibration.setFrontSights(proto.has_frontsights() ? proto.frontsights() : true);
  tripCalibration.setBackSights(proto.has_backsights() ? proto.backsights() : false);
  ```
- [ ] Review other boolean fields in `TripCalibration` for correct absent-field defaults

---

### 9. `saveImage()` page=0 — verify proto3 `optional` preserves zero

**Status:** DONE — Verified via test (no fix needed, proto3 optional preserves zero)

**File:** `cwRegionSaveTask.cpp:277-279`

```cpp
if (image.page() >= 0) {
    protoImage->set_page(image.page());
}
```

In proto3, integer field default is `0`. When `page == 0` (first page of a multi-page PDF), the value IS set, but `MessageToJsonString` may **omit it from JSON** because it matches the default. On reload, page 0 would be indistinguishable from "no page set."

However, since `cavewhere.proto` uses `optional` on all fields (`optional int32 page = 8;`), proto3 explicit presence tracking IS enabled. Fields marked `optional` in proto3 are serialized even when they hold the default value, because `has_page()` is true.

**Analysis:** Since `cavewhere.proto` declares the field as `optional int32 page = 8;` (proto3 with explicit `optional`), the field has presence tracking — `has_page()` returns true even when the value is 0. `MessageToJsonString` serializes fields with presence set, so `page = 0` should appear in JSON output as `"page": 0`. This is likely a non-issue, but a quick empirical test confirms it.

**Action items:**
- [ ] Verify with a test: save a note with `page = 0`, check the JSON output contains `"page": 0`
- [ ] If (unexpectedly) omitted, consider using `always_print_fields_with_no_presence` in `JsonPrintOptions`

---

### 10. Fix proto typos via version bump migration

**Status:** DONE

**Files:** `cavewhere.proto`, `cwSaveLoad.cpp`, `cwRegionIOTask.cpp`, `cwRegionSaveTask.cpp/h`, `cwRegionLoadTask.cpp/h`, `cwSaveLoad.h`

**Action items:**
- [x] Add `backCompassCalibration` field 14 to `TripCalibration`, rename field 6 to `legacy_backCompasssCalibration` with `json_name` for backwards compat
- [x] Rename `NoteTranformation` message to `NoteTransformation`, update all field type references in `cavewhere.proto` and C++ proto type refs
- [x] Fix "Depercated" -> "Deprecated" in proto comments
- [x] Update `cwSaveLoad::fromProtoTripCalibration()` to read new field with fallback to legacy field 6
- [x] Update `cwRegionLoadTask::loadTripCalibration()` with same fallback
- [x] Update save paths to write corrected field name (`set_backcompasscalibration`)
- [x] Bump `cwRegionIOTask::protoVersion()` to 9, add version string mapping
- [x] No explicit migration script needed — legacy field 6 uses `json_name` to accept old JSON key; data migrates naturally on next save
- [x] All 580 tests pass

---

### 11. Remove GitMode enum (NoGit, ExistingRepo are dead code)

**Status:** DONE

**Files:** `cwSaveLoad.h`, `cwSaveLoad.cpp`, `cavewhere.proto`, `tools/convert_v7_zip_to_v8.py`, `testcases/test_cwProject.cpp`

**Action items:**
- [x] Remove `GitMode` enum from `cwSaveLoad.h`
- [x] Remove `gitMode` field from `ProjectMetadataData`
- [x] Remove `setGitMode()` / `gitMode()` accessors
- [x] Remove 3 `NoGit` early-return checks in `commitProjectChanges()`, `sync()`, `resetBranchAndReconcile()`
- [x] Remove `NoGit` guard around `initializeRepositoryForCurrentFile()` call (always called now)
- [x] Remove `toProtoGitMode()` / `fromProtoGitMode()` converter functions
- [x] In `cavewhere.proto`: remove `GitMode` enum from `ProjectMetadata`, mark field 2 as `reserved`
- [x] Remove `fromProtoGitMode()` call in project load path
- [x] Remove `gitMode` from Python converter (`tools/convert_v7_zip_to_v8.py`)
- [x] Remove `gitMode` from test fixture JSON in `test_cwProject.cpp`
- [x] All 580 tests pass

---

### 12. Migrate `qt.proto` from proto2 to proto3

**Status:** DONE (c3123919)

**File:** `cavewherelib/src/qt.proto`

`qt.proto` uses proto2 with `required` on every field (QVector3D, QPointF, QDate, QSize, etc.). `required` is a forward-compatibility bomb: if a future version stops writing a field, older CaveWhere will reject the entire message rather than gracefully defaulting.

Proto3 makes all fields optional by default. The wire format is compatible — existing files parse identically.

**Risk assessment:**
- No active code calls `has_*()` on qt.proto scalar fields (one commented-out line in `sketch/src/PenLineModelSerializer.cpp:117` — sketch proto is not in the wild, no constraint).
- All access is simple getters/setters (`set_x()`, `.x()`) which work identically in proto3.
- qt.proto types are geometric primitives where every field is always meaningful (a QVector3D always has x, y, z). A zero value is valid, not "unset," so implicit proto3 semantics are correct. Do NOT add explicit `optional` keyword — in proto3, `has_*()` only exists on scalar fields marked `optional`, and these fields never need "set vs unset" distinction.
- `cavewhere-sketch.proto` also imports qt.proto but is prototype code not shipped in v9 — no backward-compatibility constraint.
- v9 format is JSON-only, so no binary wire-format concerns.

**Action items:**
- [ ] Change `syntax = "proto2"` to `syntax = "proto3"` in `qt.proto`
- [ ] Remove all `required` and `optional` keywords (fields become implicitly optional in proto3; do NOT add explicit `optional` — it would enable `has_*()` which is unnecessary for geometric primitives)
- [ ] Update `cavewhere-sketch.proto` if it has any `required` references to qt.proto types
- [ ] Verify existing test files still load correctly
- [ ] Run both test suites

---

### 13. Add `reserved` statements for deprecated proto field numbers

**Status:** DONE

**File:** `cavewherelib/src/cavewhere.proto`

Many fields are marked `legacy_` but only one `reserved` statement exists (Image field 9). Accidental reuse of a legacy field number would cause silent data corruption in older clients.

**Resolution:** Station fields 2-9 and Shot fields 1-10 cannot be replaced with `reserved` because `cwRegionLoadTask` still reads them for V5/V6 binary protobuf import from SQLite blobs. The existing `legacy_` field definitions already prevent accidental number reuse — `reserved` is only needed when a field definition is **removed entirely** (as was done for Image field 9, SurveyChunk field 3, and ProjectMetadata field 2).

**Action items:**
- [x] ~~Add `reserved` for V5-era fields~~ — Not possible: fields are still read by `cwRegionLoadTask`; definitions themselves prevent reuse
- [x] Document field number allocation strategy in a comment at the top of `cavewhere.proto`

---

### 14. Mark `TriangulatedData` as legacy

**Status:** DONE

**File:** `cavewhere.proto:255`

The `Scrap` message contained `optional TriangulatedData triangleData = 5;` with the comment "Should be cached, and not saved here." No save/load code reads or writes this field.

**Action items:**
- [x] Replace `triangleData` (field 5) in `Scrap` with `reserved 5;` — no code reads/writes it
- [x] Mark the `TriangulatedData` message itself with a legacy comment (kept for compile compat with cwRegionLoadTask/SaveTask signatures)

---

### 15. Remove cwRegionSaveTask, consolidate into cwSaveLoad

**Status:** DONE

**Files:** `cwRegionSaveTask.h`, `cwRegionLoadTask.h`, `cwRegionSaveTask.cpp`, `cwRegionLoadTask.cpp`

Two parallel serialization stacks existed:
- **Old:** `cwRegionSaveTask` / `cwRegionLoadTask` -- serialize full object graphs through `CavingRegion` monolith, handle V5/V6 legacy fields
- **New:** `cwSaveLoad::toProto*` / `cwSaveLoad::fromProto*` -- serialize individual entities for v9 file-per-entity format

**Resolution:** `cwRegionSaveTask` deleted. All actively-used static save helper functions (saveString, saveDate, saveTripCalibration, saveSurveyChunk, saveTeam, saveImage, saveScrap, etc.) moved into `cwSaveLoad` as private static methods. Dead functions (saveCave, saveTrip, saveCavingRegion, saveStation, saveShot, saveTriangulatedData, etc.) dropped. `cwRegionLoadTask` kept — it handles V5/V6 binary import which will always be supported.

**Action items:**
- [x] Move used cwRegionSaveTask functions into cwSaveLoad
- [x] Delete cwRegionSaveTask.h and cwRegionSaveTask.cpp
- [x] Clean up all includes and forward declarations

---

### 16. `SurveyChunk` dual representation (`stations`+`shots` vs `leg`)

**Status:** DONE

`SurveyChunk` has both old-style `stations` (field 1) + `shots` (field 2) and new-style `leg` (field 4). The v9 save path only writes `leg`. The V6 load path only reads `stations`+`shots`. Neither checks for the other representation.

The `leg` array also relies on positional convention (even index = station, odd = shot) with no discriminator field. A third-party tool or future developer could write the array incorrectly.

Since v9 files are always JSON-only and always use `leg`, this is not a practical risk for normal v9 files. The old `stations`+`shots` fallback concern is moot — no third-party converter exists. Additionally, line ~4269 had a commented-out `// Q_ASSERT(legCount % 2 == 0)` — an even-length `leg` array would indicate corruption (valid arrays are always odd: station, shot, ..., station).

**Action items:**
- [x] Replace the commented-out `Q_ASSERT(legCount % 2 == 0)` with a `qWarning` + early return to catch malformed `leg` arrays in both debug and release builds
- [x] In `cwRegionLoadTask::loadSurveyChunk()`, add a comment noting it only handles legacy V6 data
- N/A: `stations_size() != 0` check — no third-party converter exists, zero practical risk
- N/A: Discriminator field on `StationShot` — adds noise to human-readable format with no practical benefit

---

### 17. Annotate dead fields in Cave and Trip protos

**Status:** DONE

**File:** `cavewhere.proto`, `Cave` and `Trip` messages

The `Cave` message contains fields that are never written in v9: `trips` (field 2), `stationPositionLookup` (field 5), `stationPositionLookupStale` (field 6), `network` (field 7). The `Trip` message has `noteModel` (field 3) which is also V6-only (notes are loaded from `.cwnote` files in v9). Without annotation, a future developer may try to use them.

**Action items:**
- [x] Add `// Legacy v6 only, not written in v9+` comments to Cave fields 2, 5, 6, 7
- [x] Similarly annotate `Trip.noteModel` (field 3)
- [x] Rename Cave fields 2, 5, 6, 7 and Trip field 3 with `legacy_` prefix; update cwRegionLoadTask accessors
- [x] Rename SurveyChunk.stations/shots to `legacy_stations`/`legacy_shots`; update cwRegionLoadTask accessors
- [x] Rename Image.OBSOLETE_clipArea to `legacy_clipArea`
- [x] Remove no-op `clear_legacy_stations/shots/notemodel` calls from cwTripMergeApplier

---

### 18. Consider adding cave/trip sort order field

**Status:** DEFERRED (nice-to-have)

`loadAll()` sorts caves and trips by filesystem path. If a user reorders caves in the UI, that order is not persisted -- it comes from directory names on disk. Consider adding an `order` or `sortIndex` field to Cave and Trip protos.

---

## Testing Strategy

### Missing comprehensive v9 round-trip test — HIGH PRIORITY

**File:** `testcases/test_cwSaveLoad.cpp`

The existing round-trip test ("cwSaveLoad should save and load old projects correctly") only tests V7 → v9 conversion. There is no test that:

1. Creates a project with all field types populated (including edge cases)
2. Saves it as v9
3. Reloads the v9 files
4. Verifies every field matches

This test would have caught #4 (cave units) automatically.

**Action items:**
- [ ] Add a v9 round-trip test that creates a fully-populated project:
  - Cave with non-default `lengthUnit` (Meters) and `depthUnit` (Feet)
  - Trip with `frontSights = true`, `backSights = true`, non-zero calibrations
  - Team members with names and multiple jobs
  - Notes with scraps, note stations, leads, all scrap types (Plan, RunningProfile, ProjectedProfile)
  - NoteLiDAR entities with stations and custom transformation
  - Multi-page PDF note with `page = 0`
- [ ] Save as v9, reload, compare every field
- [ ] Add a forward-compatibility round-trip test: inject an unknown JSON field, load, verify data loads with version warning

### Other test recommendations

**Write failing tests first, then fix the code.** For each recommendation, add integration round-trip tests to `test_cwProject.cpp` and `test_cwSaveLoad.cpp` that expose the bug before implementing the fix.

Key integration tests to add:
- [ ] Full save-load-save round-trip that verifies all entity properties survive (would have caught #4 automatically; #5 is now removal, not a round-trip concern)
- [ ] Round-trip with colliding sanitized names (#2)
- [ ] Round-trip with filesystem-duplicated cave directory (#2 Layer 2 + Layer 3)
- [ ] Round-trip with Meters unit preference (#4)
- [ ] Round-trip with `sanitizeFileName` edge cases like `".X"` (#3)

**Existing test coverage strengths (no action needed):**
- Version compatibility: 8+ test cases covering future-version detection, save blocking, error surfacing
- 3-way merge: 10+ subsections testing trip-level conflict resolution
- Write optimization: last-write-wins, rename coalescing, add-then-remove cleanup
- Git integration: flush-and-commit, temporary project conversion

**Additional test gaps (lower priority):**
- No Unicode/international character tests for entity names
- No NaN/Infinity value tests for survey measurements
- No tests for concurrent save+load race conditions

---

## Recommended Implementation Order

1. **#1** (ignore_unknown_fields) — 3-line fix that unblocks the entire forward-compatibility story
2. **#3** (fix sanitizer bug + remove duplicate) then **#2** (add collision detection) — data loss prevention
3. **#4** (save/load cave units) — silent data loss of unit preferences on every save cycle
4. **#5** (remove per-shot calibrations) — dead feature removal, prevents confusion
5. **Round-trip test** — write the test first to expose #4, #8 as failing assertions, then fix
6. **#7** (fix missing returns in `ensurePathForFile`) — misleading errors on directory creation failure
7. **#8** (TripCalibration boolean defaults) — fix to make round-trip test pass
8. **#6** (remove binary-first parse) — code clarity cleanup
9. **#9** (page=0 verification) — quick verification, fix only if needed
10. **#10, #11, #12, #13** (proto hygiene, version bump) — batch together
11. **#14, #15, #16, #17** — annotations and low-priority hardening
12. **#18** — deferred

---

## Resolved Investigations

### Import path name collisions (#2-related)

**Status:** Investigated — addressed in #2 Layer 1b

**Finding:** All survey importers (Compass, Survex, Walls) call `setName()` *before* `addCave()`, so entities have no parent when named — the Layer 1 `setName()` null-parent guard skips validation. `cwCavingRegion::addCave()` / `insertCave()` perform no name uniqueness checks. `cwImportTreeDataDialog::updateImportErrors()` was intended to check for name collisions (per its comment) but the body is empty.

**Resolution:** #2 Layer 1b adds deduplication in `insertCave()` / `insertTrip()` / note addition paths, catching all entry points regardless of call order.

### `TeamMember.legacy_name` fallback in v9 loader

**Status:** Investigated — not an issue

Old files with `legacy_name` are loaded via `cwRegionLoadTask::loadTeamMember()`, not the v9 path. The v9 save path always writes the new `name` field (`cwRegionSaveTask::saveTeamMember`, line 511). The v9 loader (`cwSaveLoad::fromProtoTeamMember`) never encounters `legacy_name`.

### Individual load methods missing version checks

**Status:** Investigated — already handled

`loadAll()` calls `checkEntityVersion()` for every cave (line 3861), trip (line 3905), and note (line 3939) during the main load path. The sync pipeline has its own project-level version guard (line 5962-5969) that checks `pulledVersion > supportedVersion` before any entity-level loading. The standalone load methods are always called in contexts where version has already been validated at a higher level.

### Sync merge handlers use custom merge logic, not `cwDiff::mergeMessageByReflection`

**Status:** Investigated — no action needed

The sync merge handlers (`cwCaveSyncMergeHandler`, `cwTripSyncMergeHandler`, `cwCavingRegionSyncMergeHandler`, etc.) use custom `MergePlanBuilder` + `MergeApplier` classes — not `cwDiff::mergeMessageByReflection`. That function is only called recursively within `cwDiff.cpp` itself and from unit tests. The custom handlers are well-structured with clear fallback-to-full-reload behavior when a deterministic merge plan cannot be built.

### `cavewhere-sketch.proto` uses proto2 with `required`

**Status:** Out of scope — prototype code

The sketch module (`sketch/cavewhere-sketch.proto`) is prototype code and is not part of the v9 release. The proto2 `required` fields will need to be addressed before the sketch format ships, but this is not a v9 concern.
