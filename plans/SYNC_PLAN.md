# CaveWhere Sync Plan

## 1. Scope

Implement safe, incremental post-sync reconciliation so `cwProject::sync()` updates in-memory models after git/LFS changes without blocking the UI thread.

This plan is based on:
- current `cwSaveLoad::sync()` behavior (save flush + commit + pull/push only)
- `cwDiff` as a merge PoC (tested, not yet integrated into sync)
- operation-level serialization to prevent sync/save/load races

## 2. Principles

1. Keep one logical project-scoped pipeline.
- All save/load/sync/reconcile work is sequenced through one operation queue.
- Existing low-level file `Job` execution remains, but only as internal sub-steps of operations.

2. Do heavy work off main thread.
- git, filesystem scans, parse, and merge planning run in worker threads.
- main thread only performs short guarded apply transactions.

3. Be conservative.
- If mapping/merge is ambiguous, fall back to full `load()`.
- Prefer correctness over partial-reload coverage.

4. Sync is orchestration, not merge logic.
- `sync()` coordinates `SaveFlush -> commit -> pull -> reconcile -> push`.
- merge semantics live in dedicated planner/merge components.

## 3. Required Model Changes

Top-level identity must be explicit and stable:
- Add `id` (UUID string) to proto messages:
  - `Cave`
  - `Trip`
  - `Note`
  - `NoteLiDAR`
- Add UUID fields to runtime data/object layers:
  - `cwCaveData`, `cwTripData`, `cwNoteData`, `cwNoteLiDARData`
  - `cwCave`, `cwTrip`, `cwNote`, `cwNoteLiDAR` accessors

Backfill/repair rules:
- Missing id on load/reconcile: generate UUID.
- Duplicate id: deterministic repair (keep canonical, reassign others).
- Persist repairs through queued `RepairSave` after successful apply/load.

## 4. Queue Architecture

## 4.1 Operation Queue (single public scheduler)

Introduce project-scoped FIFO operations:
- `SaveFlush`
- `SyncProject`
- `ReconcileExternal`
- `LoadProject`
- `RepairSave`

Rules:
- one operation at a time
- generation-token bound (cancel stale operations on close/switch)
- model mutation allowed only inside operation apply stage

## 4.2 Low-level File Jobs

Keep existing file `Job` executor for:
- write/move/remove/copy/custom tasks

Constraint:
- file jobs are only invoked by an active operation
- no direct external scheduling path bypassing operation queue
- outside an active operation, save paths may only record/coalesce save intent; they must not start file jobs
- import paths (`addFiles`, `addImages`, and related copy/write flows) must also enqueue through operation-owned execution only

## 4.3 Operation Contracts

Each operation contract defines required inputs, allowed side effects, failure behavior, and valid next operations.

### SaveFlush
- Inputs:
  - current project context
- Allowed side effects:
  - wait for pending low-level file `Job` completion
  - no model mutation
- Failure behavior:
  - if file `Job` fails, return error and stop current parent operation
- Valid next operations:
  - `SyncProject`
  - `ReconcileExternal`
  - `LoadProject`
  - `RepairSave`

### SyncProject
- Inputs:
  - explicit user sync request
  - project generation token
  - supported project file version (max supported `.cwproj` version for this app build)
- Allowed side effects:
  - run sync session: `SaveFlush -> commit -> Pull -> SyncReport -> ReconcileExternal -> (if apply mutated canonical model: SaveFlush + commit) -> Push`
  - enqueue/execute `ReconcileExternal` as part of session
  - read and validate pulled `.cwproj` file version before reconcile/apply
- Failure behavior:
  - on epoch-change or push-reject: retry within session cap
  - on retry-cap reached: return non-fatal sync-incomplete status and stop session
  - on pulled `.cwproj` version newer than supported: stop with incompatible-version error (no `ReconcileExternal`, no `Push`, no fallback `load()`)
  - on unresolved merge conflict in canonical project payload files: do not call `load()`; attempt deterministic `ours` conflict resolution, then restart session from `SaveFlush`
  - on merge conflict not eligible for deterministic `ours` handling (or recovery failure): stop and surface conflict error
- Valid next operations:
  - internal: `ReconcileExternal`, `SaveFlush`
  - fallback: `LoadProject`

### ReconcileExternal
- Inputs:
  - `SyncReport` or external-change report
  - model snapshot/epoch token
- Allowed side effects:
  - worker-thread parse/plan/merge preparation
  - main-thread guarded apply transaction
  - may enqueue `RepairSave` if identity repair persistence is required
  - must return an explicit apply outcome: `no-op` (projection only, no canonical data mutation) or `mutated` (canonical model changed and must be persisted before push)
- Failure behavior:
  - if epoch changed before apply: abort apply and signal parent `SyncProject` retry
  - if plan/apply is ambiguous or invalid: request full reload fallback
- Valid next operations:
  - `RepairSave`
  - `LoadProject`
  - `SaveFlush` (required before `Push` when apply outcome is `mutated`)
  - return control to parent `SyncProject` for `Push` only when apply outcome is `no-op`

### LoadProject
- Inputs:
  - project file path and generation token
- Allowed side effects:
  - full in-memory model replacement from disk
  - enqueue `RepairSave` if load discovered id backfill/repair
- Failure behavior:
  - return load error; model remains at prior consistent state if replacement cannot complete
- Valid next operations:
  - `RepairSave`
  - `SaveFlush`
  - `SyncProject`

### RepairSave
- Inputs:
  - queued repair writes (id backfill/duplicate repair results)
  - generation token
- Allowed side effects:
  - persist deterministic identity repairs through normal file `Job` writes
  - optional commit when invoked from sync session
- Failure behavior:
  - keep repair-pending state and return warning/error for retry on next safe boundary
- Valid next operations:
  - `SaveFlush`
  - `SyncProject`
  - operation completion

## 5. Sync/Reconcile Flow

1. User explicitly triggers sync (button/save-triggered sync).
2. Enqueue `SyncProject` session.
3. `SyncProject` session runs:
- `SaveFlush`
- commit local changes
- `Pull` (no push yet)
- read `.cwproj` version from pulled state; if version > supported max, return incompatible-version status and stop session
- build `SyncReport`
- enqueue and execute `ReconcileExternal`
- pre-apply epoch check: if unchanged, apply plan on main thread under `RemoteApplyGuard`
- pre-apply epoch check: if changed, abort apply and restart session from `SaveFlush`
- if apply outcome is `mutated` (including non-repair reconcile results): `SaveFlush` + commit
- if apply outcome is `no-op`: no additional save/commit gate before `Push`
- `Push`
4. If `Push` is rejected (remote advanced), restart session from `Pull` (or `SaveFlush` if local state changed).
5. On planner/apply ambiguity or unrecoverable error, enqueue full `LoadProject`.

## 5.2 Sync Session Retry Policy

- Retries are finite per explicit user sync request (no infinite auto-loop).
- Retry trigger: epoch changed before apply.
- Retry trigger: push rejected due to remote advance.
- Recommended cap: 3-5 retries per sync session.
- If cap reached, end session with status explaining sync did not complete because local/remote state kept changing.
- If cap reached, require user to press sync again.

## 5.1 SyncReport Contract

`SyncReport` must include:
- `beforeHead`, `afterHead`
- merge/conflict state
- changed paths (backend summary)
- commit diff paths (when range known)
- hydration-delta paths (pre/post baseline comparison)
- source flags for diagnostics

Path assembly:
- always union backend + commit-diff + hydration-delta paths
- if `beforeHead != afterHead` and no deterministic targets -> full reload

## 6. cwDiff Integration Strategy

`cwDiff` is merge-capable for UUID-keyed protobuf structures and should be used as a merge helper, not the whole reconcile engine.

Use in reconcile planner:
- build base/ours/theirs protobuf triplets where available
- produce merged payload candidates for object-level apply
- if merge output cannot be mapped safely to runtime objects, escalate

Current gaps (must be addressed):
- one-sided deletes are currently effectively delete-preferred in repeated merge paths
- conflict policy currently handles value-level conflicts but not all structural conflicts

Target merge behavior (wanted behavior):
- delete-vs-modify must be governed by explicit structural policy, not implicit delete-preference
- move/rename/parent mapping must use deterministic identity+anchor rules
- unresolved structural ambiguity must return explicit ambiguous status (not silent best-effort merge)

Determinism requirements for `cwDiff` usage:
- IDs are required for key-merge repeated message paths; missing/duplicate IDs must be backfilled/repaired before merge.
- For ordered semantic lists (for example trip `chunks`), IDs are for identity matching only, not output sorting.
- For ordered semantic lists, preserve base order anchor and replay deterministic operations (delete/update/insert) relative to base.
- For concurrent inserts at the same anchor/position, use a fixed tie-breaker rule.
- If anchors/parent mapping are ambiguous after concurrent edits, return ambiguous-structure status and escalate instead of guessing.

Conflict-policy clarification:
- `UseOursOnConflict` / `UseTheirsOnConflict` / `UseBaseOnConflict` are value-level conflict policies.
- Structural conflicts (delete-vs-modify, move/rename+delete/add ambiguity, unresolved parent mapping) require explicit structural rules and may require fallback.

## 6.1 Object-Level Merge Architecture (Refactor Direction)

`cwSaveLoad` should remain orchestration-only (`SaveFlush -> Pull -> ReconcileExternal -> Push`) and not continue accumulating object-specific merge logic.

Add a reconcile merge registry with handler-based object strategies:
- `cwSyncMergeRegistry` stores ordered handlers and dispatches by changed object/path scope.
- `cwSyncMergeHandler` contract:
  - `plan(base, ours, theirs, context) -> PlanResult`
  - `apply(plan, modelContext) -> ApplyResult`
  - `fallbackReason()` for diagnostics
- Each handler owns object/field semantics and conflict policy; `cwSaveLoad` only coordinates handler execution and fallback.

Initial handlers:
- `cwNoteScrapMergeHandler`:
  - merge note metadata + scrap structure incrementally
  - support deterministic scrap reorder (non-ambiguous)
  - do not trigger broad note-model replacement only because note order differs; preserve/replay note order deterministically when IDs map cleanly
  - apply geometry/station/lead updates without broad full-model replacement when mapping is safe
- `cwScrapSyncMergeHandler`:
  - own nested scrap merge semantics for geometry/stations/leads
  - use identity-based matching for nested collections, with explicit fallback on ambiguous identity mapping
- `cwTripMergeHandler` and `cwCaveMergeHandler` for trip/cave payloads

Per-object/per-field strategy requirements:
- Do not rely on one global conflict rule for all fields.
- Define explicit rules per field category:
  - scalar metadata fields
  - ordered structural child lists
  - identity-bearing nested objects
  - geometry payloads
- Return explicit ambiguous/unsupported status when field semantics are unclear.

Identity requirements for robust nested merges:
- Add stable IDs where needed for structural children (for example `NoteStation`, `Lead`) before claiming deterministic reorder/move semantics for those lists.
- Until IDs exist, treat reorder/move on those lists as explicit-policy cases (deterministic fallback or guarded full reload), not silent best-effort.
- For unordered nested lists (stations/leads), use IDs for identity matching only; list order is not semantic.

## 7. Concurrency Controls

1. `RemoteApplyGuard` around main-thread apply:
- disable autosave writes
- suppress undo pollution for remote-applied mutations

2. `syncInProgress` / edit gating policy:
- allow UI interaction during background sync work
- block/defer edits only during short final apply window

3. Epoch check before apply:
- if model mutation epoch changed since plan capture, abort plan apply and restart sync session

4. App-originated write suppression for watchers:
- maintain pending-write fence window
- ignore watcher events for known self-writes

## 7.1 User Edit Interaction With Operation Queue

This is the common path and must remain explicit.

1. User edits always mutate memory immediately.
- UI/model edits are not globally blocked by sync background work.

2. Save execution is operation-gated.
- User edits do not enqueue low-level `cwSaveLoad::Job` writes directly.
- User/model callbacks only mark/coalesce a pending save request for the project.
- The operation queue schedules `SaveFlush` work to open the save gate and execute pending jobs.
- Operation queue remains the only scheduling path for file `Job` execution.

3. Epoch is incremented on user-originated model mutations.
- Planner captures `planEpoch` at plan-build start.
- Apply checks current epoch against `planEpoch`.

4. `SaveFlush` is the disk consistency boundary.
- Before commit/pull/reconcile boundaries, `SaveFlush` waits for pending save jobs.
- This guarantees disk reflects queued local edits at sync boundaries.

5. Stale-plan behavior when user edits during sync.
- If user edits after plan capture and before apply, epoch mismatch occurs.
- `ReconcileExternal` apply is discarded (no partial apply from stale plan).
- Parent `SyncProject` session restarts from `SaveFlush` (subject to retry cap).

6. Apply window behavior.
- During short final main-thread apply transaction, edits are blocked/deferred.
- Outside apply window, edits are allowed.

7. Retry outcome for explicit user-triggered sync.
- If repeated epoch changes prevent apply, retry cap ends the session with sync-incomplete status.
- User can trigger sync again to start a new session.

## 8. Fallback Policy

Force full `load()` when:
- parse/schema incompatibility
- missing parent resolution
- ambiguous rename/delete/add mapping
- generation token mismatch
- failed transactional apply/rollback

Merge conflict policy (explicitly not `load()`):
- unresolved git merge conflicts must not trigger `load()` because conflict markers make project payload parse invalid
- when conflict is in canonical project payload files, use deterministic conflict recovery by taking `ours`
- after successful `ours` recovery, restart sync session from `SaveFlush`
- if deterministic recovery is not applicable or fails, return `Conflict` status and require manual conflict resolution
- expected frequency should be low once incremental reconcile/cwDiff path is primary, but this remains a safety guardrail

Project version compatibility policy (explicitly stop sync):
- after `Pull`, read project file version from `.cwproj`
- if pulled project version is newer than this build supports, stop sync with `IncompatibleVersion`
- on `IncompatibleVersion`, do not run `ReconcileExternal`, do not `Push`, and do not force fallback `load()`
- require opening with a newer CaveWhere build or manual downgrade/migration workflow before sync can continue

## 9. Phased Implementation

## Phase 1: Identity Foundation
- Add top-level UUID schema/runtime wiring.
- Implement missing-id backfill and duplicate-id repair.
- Define `RepairSave` persistence contract and queue integration requirements.
- Define load-time `RepairSave` gating policy (explicit sync context and/or user consent) to avoid unintended dirty repos on normal open.

Exit criteria:
- cave/trip/note/notelidar identity stable across reloads/renames.
- opening older projects does not auto-persist repair writes unless gating policy allows it.

## Phase 2: Queue Foundation
- Add operation queue and operation types.
- Route existing load/save/sync entry points through operation queue.
- Route import entry points (`addFiles`, `addImages`, and related file import/copy paths) through operation queue.
- Ensure file `Job` executor is only operation-internal.
- Add generation token cancellation.
- Wire `RepairSave` as an operation on top of the queue foundation.
- Implement coalesced save-intent capture outside active operations and operation-owned `SaveFlush` execution gate.
- Add explicit `SaveFlush`/`completeSaveJobs` error propagation channel so parent operations can stop on write failures.
- Define hard-cancel/quarantine behavior for in-flight git futures on close/switch so stale sessions cannot mutate active project state.
- Increment model mutation epoch for operation-applied mutations (reconcile apply/load/repair), not only user-originated edits.

Exit criteria:
- deterministic FIFO sequencing across save/load/sync.
- imports cannot schedule low-level `Job` work outside active operation boundaries.
- identity repair persistence runs only through operation boundaries.
- failed file `Job` writes surface as operation failures and block downstream commit/push.
- stale in-flight sync/git completions are quarantined from active project state after close/switch.
- epoch checks reject stale apply plans for both user and operation-originated mutations.

## Phase 3: SyncReport and Post-Sync Hook
- Split sync transport into explicit `Pull` and `Push` steps (no push before reconcile).
- Extend sync to produce `SyncReport` after `Pull`.
- Add hydration-delta and commit-diff union logic.
- Enqueue `ReconcileExternal` inside `SyncProject` session before `Push`.
- Add finite per-session retry handling for pre-apply epoch changes and push rejects.
- Add pulled `.cwproj` version compatibility check and return `IncompatibleVersion` when pulled version is newer than supported.
- Add unresolved-merge handling that prefers deterministic `ours` recovery for canonical payload conflicts and never calls `load()` on conflict markers.
- Enforce pre-push persistence rule: if reconcile apply outcome is `mutated`, require `SaveFlush + commit` before `Push`.

Exit criteria:
- sync produces deterministic change summary even when HEAD unchanged (hydration case), and only pushes after successful reconcile/apply.
- newer pulled project versions are blocked from sync safely (no reconcile/push/load fallback).
- unresolved merge conflicts never trigger parse/load of conflict-marker payloads.

## Phase 4A: V1 Incremental Reconcile (Notes Quick Win)
- Implement planner/apply for `Note` and `NoteLiDAR` add/remove and non-structural updates.
- Handle note metadata/image refresh paths that do not require structural scrap merge.
- Add image-only refresh action for hydration/asset changes.
- Add `RemoteApplyGuard` and short-window edit gating.

Exit criteria:
- failing sync hydration visibility test passes without full reload.

## Phase 4B: Note Structural Merge (Scraps)
- Integrate `cwDiff`-assisted payload merge for `Note` updates that include `scraps`.
- Use deterministic identity+ordering rules for scrap-level structural edits.
- Keep note-level reconcile incremental when note IDs are stable, even if note order differs; do not force full note-model replacement on order-only differences.
- Add stable IDs for `NoteStation` and `Lead` (schema + runtime + serialization) to enable deterministic nested merge.
- Add explicit 3-way nested conflict policy for stations/leads:
  - match by stable ID, treat ordering as non-semantic
  - value conflict: take ours
  - delete-vs-modify (either side deleted, other side modified): take ours (preserve local object)
- Geometry policy for scraps:
  - prefer wholesale geometry replacement when non-conflicting
  - on geometry conflict, keep ours and emit reconcile diagnostic (no silent overwrite)
- Escalate to full reload when note/scrap structural mapping is ambiguous.

Implementation checklist:
- [x] 4B.1 Note-order tolerance in `cwNoteSyncMergeHandler`
- [x] Detect note reorder (same stable note IDs, different order) as reorder-only, not `FullModelReplace`.
- [x] Apply deterministic note reorder while preserving existing note QObject identity where mapping is unambiguous.
- [x] Keep fallback to full reload for duplicate/null note IDs or unresolved mapping.
- [ ] 4B.2 Scrap merge extraction
- [ ] Add `cwScrapSyncMergeHandler` (or equivalent merge component) and move nested scrap merge logic out of note handler.
- [ ] Keep note handler responsible for note-level routing/dispatch only.
- [ ] 4B.3 Stable identity for stations/leads
- [ ] Add stable IDs for `NoteStation` and `Lead` in proto schema, runtime data structs, and serialization.
- [ ] Add load-time backfill/repair for missing/duplicate station/lead IDs with deterministic repair policy.
- [ ] Include station/lead identity repair persistence in normal `RepairSave` flow.
- [ ] 4B.4 3-way stations/leads policy
- [ ] Use base/ours/theirs keyed by stable IDs (order ignored).
- [ ] Conflict matrix: value-vs-value -> ours; delete-vs-modify -> ours; delete-vs-delete -> deleted.
- [ ] Concurrent add with different IDs -> keep both.
- [ ] 4B.5 Scrap geometry policy
- [ ] Use wholesale geometry replacement only when no local geometry conflict is detected.
- [ ] If both sides changed geometry from base, keep ours and add a reconcile diagnostic.
- [ ] 4B.6 Diagnostics and fallback
- [ ] Emit explicit diagnostics for: note reorder applied, station/lead merge conflict resolved by ours, geometry conflict kept ours, and ambiguous mapping fallback.
- [ ] Keep strict fallback to full reload on ambiguous structural mapping.

Test checklist (add to `testcases/test_cwProject.cpp`):
- [x] `TEST_CASE("cwProject sync reconciles note order changes without full note-model replacement", "[cwProject][sync]")`
- [x] `TEST_CASE("cwProject sync preserves note QObject identity on reorder-only reconcile", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync merges scrap station updates by stable id with order-insensitive matching", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync merges scrap lead updates by stable id with order-insensitive matching", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync prefers local station values on value conflicts", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync prefers local lead values on value conflicts", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync keeps local station when remote deletes and local modifies", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync keeps local lead when remote deletes and local modifies", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync keeps local station when local deletes and remote modifies", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync keeps local lead when local deletes and remote modifies", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync keeps both independently added stations by id", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync keeps both independently added leads by id", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync replaces scrap geometry when only remote geometry changed", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync keeps local scrap geometry on concurrent geometry conflict", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync emits diagnostic when geometry conflict resolves to ours", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync falls back to full reconcile when station id mapping is ambiguous", "[cwProject][sync]")`
- [ ] `TEST_CASE("cwProject sync falls back to full reconcile when lead id mapping is ambiguous", "[cwProject][sync]")`

Exit criteria:
- concurrent note+scrap edits merge deterministically or explicitly fallback with no silent data loss.

## Phase 4C: Reconcile Refactor (Handler Registry)
- Extract object-level merge/apply logic out of `cwSaveLoad` into `cwSyncMergeHandler` implementations.
- Add `cwSyncMergeRegistry` dispatch in `ReconcileExternal`.
- Keep `cwSaveLoad` focused on session orchestration, retries, and persistence gates.
- Add diagnostics for handler selection, fallback reason, and apply outcome per handler.

Exit criteria:
- adding a new object merge path requires a new handler registration, not editing `cwSaveLoad` merge internals.
- `cwSaveLoad` reconcile code size/complexity is reduced and object-level logic is testable in isolation.

## Phase 5: Trip/Cave Incremental Reconcile
- Integrate `cwDiff`-assisted payload merge for trip/cave-level data.
- Apply field-safe updates; avoid destructive broad `setData` where payload incomplete.
- Keep strict fallback for structural ambiguity.

Exit criteria:
- common trip/cave updates reconcile incrementally with no child-loss regressions.

## Phase 6: External Change Detection
- Add desktop watcher debounce and self-write suppression.
- Mobile/manual triggers enqueue reconcile operations (no direct mutation).

Exit criteria:
- external edits converge without watcher loops.

## Phase 7: Hardening
- stress tests for rapid local edits + remote updates
- diagnostics for fallback reasons and timing
- performance tuning for large projects
- migrate caller surfaces (`cwProject::sync()` and QML call sites) to structured sync outcome handling instead of bool-only result

Exit criteria:
- stable behavior under concurrency stress, acceptable sync/apply latency.
- UI/API surfaces expose and handle `IncompatibleVersion`/`Conflict`/sync-incomplete outcomes without silent downgrade to bool semantics.

## 10. API Changes (Proposed)

`cwSaveLoad::sync()` should return structured result:

```cpp
struct SyncResult {
    enum class Outcome {
        NoChange,
        PartialApplied,
        FullReloadRequired,
        IncompatibleVersion,
        Conflict,
        Error
    };

    SyncReport report;
    QString errorMessage;
};
```

Additional boundaries:
- `ReconcilePlan planFrom(const SyncReport&, const ModelSnapshot&)`
- `ApplyResult applyPlan(const ReconcilePlan&)`

## 11. Test Plan

1. Regression:
- pulled LFS note appears in-memory after sync without explicit full reload

2. Queue/ordering:
- `SaveFlush -> SyncProject -> ReconcileExternal` ordering enforced
- no overlapping model mutation operations

3. Identity:
- top-level UUID persistence/backfill/duplicate repair

4. Concurrency:
- local edit during background sync
- local edit during apply window (blocked/deferred)
- epoch mismatch restarts sync session from `SaveFlush`
- finite retry cap behavior for explicit user-triggered sync
- push-reject restart behavior (`Pull`/`SaveFlush` path)
- project close/switch cancels stale operations

5. Merge/apply safety:
- no autosave writes during remote apply
- no undo entries created by remote apply
- ambiguous deltas trigger full reload
- unresolved merge-conflict markers never trigger `load()`; `ours` recovery path or explicit `Conflict` status is used
- pulled newer `.cwproj` version returns `IncompatibleVersion` and blocks reconcile/push

6. Hydration/path:
- hydration-only changes detected with unchanged HEAD
- image refresh without unrelated full reload

## 12. Key Risks and Mitigations

1. Complexity risk from many moving parts.
- Mitigation: strict operation boundaries and small phased rollout.

2. Data-loss risk in partial apply.
- Mitigation: conservative fallback + transactional guarded apply.

3. Identity drift.
- Mitigation: UUID backfill/repair with deterministic persistence.

4. Reconcile loop risk.
- Mitigation: self-write suppression and operation-only mutation path.
