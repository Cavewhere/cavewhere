# GLB render-queue follow-ups (post issue #629)

## Context

Issue #629 was a memory leak: stepping a trip's declination while the 3D View
page was hidden grew RSS without bound. Root cause was that
`cwRenderTexturedItems::m_pendingChanges` is drained only inside
`cwRhiTexturedItems::synchronize()`, which runs only when the View renders a
frame. With the View hidden, every re-triangulation stacked another full-mesh
payload onto the queue.

The landed fix coalesces `m_pendingChanges` per item id in
`cwRenderTexturedItems::addCommand()` (an Add later removed before a sync
annihilates; updates replace rather than stack), so the queue stays bounded by
the number of live items regardless of render cadence. Regression test:
`testcases/test_cwLinePlotGlbLeak_issue629.cpp`.

The fix closes the leak. These are the two follow-ups the code review surfaced —
neither is a live bug, both are efficiency/cleanliness that the fix makes worth
doing.

## Commit sequence

### Commit 1 (next) — Reuse render ids in `cwNoteLiDARManager::runBatch()`

`cavewherelib/src/cwNoteLiDARManager.cpp:636` — on every line-plot re-run,
`runBatch()` removes all of a note's old render ids and re-adds fresh ones (its
own comment: "For now just remove all the old ids / not very efficient"). Because
`cwRenderTexturedItems::addItem` mints a new monotonic id each call, the new
per-id coalescing can only annihilate the still-pending Add/Remove pair — it can
never collapse repeated geometry edits onto a *stable* id.

**Change:** when a note re-triangulates, reuse the existing render id via
`updateGeometry(existingId, newGeom)` (and the matching model-matrix / material
updates) instead of remove-all + re-add.

**Payoff:** repeated declination steps collapse to a single `UpdateGeometry`
payload per id (last-writer-wins), and the per-run churn of picker
re-registration (`registerPickable`/`unregisterPickable`) and visibility-store
re-publish disappears.

**Blast radius:** touches the note→render identity mapping (`m_noteToRender`),
keyword-item wiring (`addKeywordItemForNote`), and picking registration. Does
*not* avoid the re-triangulation itself (still done in the worker) — only the
downstream id churn.

### Commit 2 — Replace the hand-coalesced queue with an id-keyed map

`cavewherelib/src/cwRenderTexturedItems.cpp:127` — `addCommand()` maintains
coalescing by hand over a flat `QVector`, with an O(n) `pendingIndexOfType`
linear scan (called up to twice per command) plus bespoke Add/Remove
annihilation and same-type replacement logic. For N edits between frames this
drains in O(N²).

**Change:** replace the `QVector<PendingCommand>` with a
`QHash<uint32_t, PendingItemState>` — a per-id struct of optional dirty fields
plus an Add/Remove flag — so coalescing becomes a data-structure invariant
rather than maintained logic (the shape `cwRenderBillboards` already uses via its
`std::unordered_map<cwBillboardId, Entry>`). Removes the O(n) scan and the
O(N²) drain.

**Blast radius:** changes the on-the-wire contract with
`cwRhiTexturedItems::synchronize()` (`cavewherelib/src/cwRhiTexturedItems.cpp:64`),
which today iterates the vector and switches on `PendingCommand::Type`; that
drain loop must be rewritten to walk the keyed map.

## Not in this phase / considered and dropped

- **Base-level bounded-staging guarantee on `cwRenderObject`** (review item A1) —
  the "staging drained only on frame render" contract is enforced by convention
  across all `cwRHIObject` subclasses; a base-provided coalesced staging buffer
  or a debug bounded-queue assertion would make reintroducing #629 impossible.
  Deferred: touches every subclass's synchronize handshake, and all siblings are
  currently bounded by construction (`cwTracked` / id-keyed maps), so there is no
  live leak to chase. Revisit if a third render object needs staging.
- **Tightening the regression test's leak threshold** (review item F3) — the
  400 MB cap over 16 edits misses partial leaks under ~25 MB/edit, but tightening
  trades against RSS/ASan flakiness and the test already catches the 137 MB/edit
  #629 regression by edit ~3. Left as-is deliberately.
- **De-duplicating the per-type payload-field mapping** between
  `PendingCommand::mergeUpdatePayload` and `cwRhiTexturedItems::synchronize`
  (review item R3) — low drift risk; folds naturally into Commit 2's rewrite.
