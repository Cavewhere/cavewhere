# Adding New Types to Incremental Merge

This guide describes the recommended pattern for adding a new datatype to sync merge/reconcile.

## Goals

- Keep incremental merge deterministic.
- Avoid full-model replacement unless mapping is ambiguous.
- Preserve local edits on conflict ("ours wins" unless explicitly defined otherwise).
- Keep IDs stable across save/load/sync.

## 1. Define Stable Identity First

Every mergeable object must have a stable ID.

- Add an `id` field in protobuf for the object (or nested object) if it does not already exist.
- Add an ID field in runtime data (`QUuid`) and runtime object type.
- Default new runtime objects to a non-null ID (usually `QUuid::createUuid()`).
- Do not key merge mapping by mutable fields (name/description/path). Use ID.

Current examples:

- `cwNote` / `cwScrap` / `cwNoteStation` / `cwLead`
- `cwNoteLiDARStation` (recently migrated to stable ID matching)

## 2. Update Serialization in `cwSaveLoad`

IDs must survive round-trip save/load.

- Write IDs in save path:
  - `toProto...()` functions should write non-null IDs to proto.
- Read IDs in load path:
  - `load...()` functions should parse ID if present.
  - If missing, set null ID and let repair phase assign a new one.

Typical places:

- `cwSaveLoad::toProto...`
- `cwSaveLoad::load...`

## 3. Register Identity Repair in Load Repair Pass

Repair must run after load data is parsed and before model hydration.

- Add ID repair for the new type in the load repair pass.
- Use the same duplicate/null handling pattern as existing types:
  - keep first unique valid ID
  - regenerate null/duplicate IDs
  - record repair actions in `identityRepair`

Current central location:

- `repairNestedScrapIds(...)` in `cwSaveLoad.cpp`

Note: keep repair at the data level (not deep inside per-item load loops) to keep load code readable.

## 4. Add/Update Merge Planner

Planner decides whether incremental merge is safe.

- Validate descriptor/object identity sets:
  - null ID or duplicate ID => ambiguous
  - missing current object for loaded ID => ambiguous/fallback
- If IDs match (including reorder), prefer incremental merge.
- If IDs do not map cleanly, return full replace/fallback.

Recommended shape:

- Plan builder: only mapping/ordering/ambiguity detection.
- Applier: only mutation application rules.

## 5. Add/Update Merge Applier

Apply 3-way merge by identity, not by position.

- Bundle fields by semantic coupling, then choose per bundle.
- For each ID:
  - if only ours exists: keep ours
  - if only theirs exists: add theirs
  - if both exist: 3-way resolve with base snapshot
- On ambiguous identity map (null/duplicate IDs), fail and let caller fallback.

Conflict policy should be explicit per bundle (for CaveWhere, usually ours-on-conflict).

## 6. Wire Through Sync Merge Handler

- Detect changed paths for the datatype.
- Load base snapshot from merge-base commit if needed.
- Build incremental plan and apply.
- Emit `cwReconcileMergeResult` with:
  - `mutated` set correctly
  - `fallbackToFullReload` when planner/applier reports ambiguity

## 7. Tests You Should Add

Add both unit and integration coverage.

- Plan/applier unit tests:
  - reorder without replacement
  - null/duplicate IDs => ambiguous/fail
  - conflict rule behavior (ours/theirs)
  - add/delete combinations by ID
- `cwProject` sync integration tests:
  - pulled remote updates apply incrementally
  - reorder does not force full replace
  - identity-based fields preserved correctly
- Repair tests:
  - duplicate/null IDs repaired on load
  - repaired data saved back with unique IDs

## 8. Implementation Checklist

1. Proto: add `id` field.
2. Runtime type: add `QUuid id`.
3. `cwSaveLoad`: write/read ID.
4. `cwSaveLoad` repair pass: regenerate null/duplicates.
5. Plan builder: ID-based mapping + ambiguity checks.
6. Applier: ID-based 3-way merge.
7. Sync handler: path detection + plan/apply/fallback reporting.
8. Tests: repair + unit merge + project sync integration.

## 9. Common Pitfalls

- Matching by name/path instead of ID.
- Repairing IDs in many local load loops instead of one repair stage.
- Mixing planner and applier responsibilities.
- Adding bool flags where outcome enum/state machine is clearer.
- Forgetting to mark reconcile result as mutated for non-repair canonical merges.

## 10. Practical Rule of Thumb

If a datatype cannot provide stable IDs for all mergeable children, treat it as ambiguous and fall back to full-model replacement until IDs are introduced.
