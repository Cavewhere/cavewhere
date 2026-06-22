# Brush structure model — position-free `internalId` (stable node ids)

**Goal:** stop `cwBrushStructureModel` from calling `beginResetModel()` for
layer add / remove / move. Model resets destroy every delegate, drop scroll /
expansion / selection state, and are a performance cliff on every structural
edit. Replace the positional `internalId` ancestry with **stable identity** so
proper `beginInsertRows` / `beginRemoveRows` / `beginMoveRows` are safe and
`QPersistentModelIndex` survives across edits.

Status: **Phase A done** (stable layer ids + reactive `layerCount`, green).
Phase B and the `reuseItems` verification remain. Pause for review between phases.

---

## Root cause

`internalId` packs *positional* ancestry (`cwBrushStructureModel.cpp:20-58`):

```
bits [0,1]   node kind (Layer / Category / Rule)
bits [2,32]  owning layer index    ← a POSITION
bits [33,63] owning category index ← a POSITION (rule nodes only)
```

When a structural edit happens at the **root** (layer level), Qt's
persistent-index machinery only re-homes the **direct children of the change
parent** — the layer rows — by shifting their `row()` while keeping their
`internalId`. Layer nodes survive (their identity is `row()`; the layer field in
their id is always 0).

But categories (grandchildren) and rules (great-grandchildren) are never touched
by Qt on a root-level change — both their `row()` and `internalId` are left
as-is. Because their `internalId` names the owning layer **by position**, every
layer after the edit point ends up with descendants pointing at the wrong layer.
That corruption is exactly what the current `beginResetModel()` dodges.

The same mechanism has a **latent** instance in the rule path: `insertRule` can
insert a brand-new category block *in the middle* of a layer
(`cwBrushStructureModel.cpp:534-538`). That shifts later category positions,
whose **rules** encode `categoryIndex` positionally and are grandchildren of the
layer — so Qt strands them too. It mostly hasn't bitten because the tree is
always fully expanded and re-read, but a held `QPersistentModelIndex` to such a
rule would resolve to the wrong rule.

## Why stable ids fix it

When a *whole layer* moves, nothing inside any layer changes — categories keep
their order within each layer, rules keep their order within each category. So
the descendants' `row()` values are already correct after a layer move; Qt
leaving them untouched is right. The only thing wrong is the *positional* layer
reference in their `internalId`.

Encode the owning layer by a **stable id** (position-independent) instead of its
index. Then a layer move/insert/remove changes positions, but every descendant's
`internalId` still names its layer by the same id and resolves correctly via an
id→row lookup. Grandchildren need no adjustment (Qt's default), and it is now
*correct*. Proper row signals replace the reset; persistent indexes survive.

The encoding is fully encapsulated: QML only touches `layerIndexOf()` /
`ruleIndexOf()` (positions), never raw `internalId`, and the editor addresses
everything by `(layerIndex, ruleIndex)`. This is an internal model change with no
public-API ripple.

**The id must live on the model, not the data.** `cwDecorationLayer` is a pure
value struct, `cwLineBrush` has a defaulted `operator==`, and both are
proto-serialized — adding an id to the struct would pollute equality and the
on-disk format. The model owns the id↔position mapping.

---

## Phase A — stable **layer** ids (the focused fix)

New model state in `cwBrushStructureModel`:

```cpp
QList<quintptr> m_layerIds;   // aligned 1:1 with m_brush.decorations
quintptr m_nextLayerId = 0;   // monotonic; 31-bit field = ~2B edits, ample
```

Position lookup via `m_layerIds.indexOf(id)` — `n` is a handful, so no hash
member to keep in sync. Invariant: `m_layerIds` stays aligned with
`decorations`; every mutator and `setBrush` maintain it.

Encoding changes (layer field now holds a stable id, not a position):

- Add `int layerRowForId(quintptr id) const`.
- `index()` / `parent()` just propagate the id.
- Map id→position at the `encodedLayer(index)`-as-position call sites:
  `rowCount` (Category branch), `data` (Category + Rule branches),
  `layerIndexOf`, `ruleIndexOf`.

Mutators (the payoff — proper row signals, no reset):

- `insertLayer`: `id = m_nextLayerId++`; `beginInsertRows(QModelIndex(), pos, pos)`;
  insert into `decorations` + `m_layerIds`; `revalidate()`; `endInsertRows()`.
- `removeLayer`: `beginRemoveRows(QModelIndex(), pos, pos)`; `removeAt` both;
  `revalidate()`; `endRemoveRows()`.
- `moveLayer`: `beginMoveRows(root, from, from, root, from < to ? to + 1 : to)`;
  `decorations.move` + `m_layerIds.move`; `revalidate()`; `endMoveRows()`.
  **Note the `to + 1` destination convention** — `beginMoveRows` wants the
  pre-move insert-before position, different from the editor/QML's already-
  collapsed final index.
- `setBrush`: keeps its reset (wholesale load), but also (re)assigns
  `m_layerIds` for the new layer count.

Error roles (`m_layerErrors`) are positional, so `revalidate()` rebuilds them
aligned to the new order — no extra `dataChanged` (per-layer content is
unchanged; inserted/removed rows are read fresh).

### Required co-requisite — reactive `layerCount` (model-owned)

Dropping the layer-op reset **breaks the current grip bindings**, so this ships
*with* Phase A, not after. Today `editorId.layerCount()` is a non-reactive
`Q_INVOKABLE`; the bindings at `BrushEditorPanel.qml:349` (grip opacity) and
`:367` (`DragHandler.enabled`) only refresh because `beginResetModel` recreates
every delegate. After Phase A, `beginInsertRows`/`beginRemoveRows` leave
pre-existing delegates in place, so their `layerCount() >= 2` test keeps a stale
value: cross the 1→2 threshold and only the *new* layer grows a grip; the
original layer's grip stays hidden and its `DragHandler` disabled. Going 2→1
leaves the survivor wrongly enabled.

Fix on the **model** (the source of truth — root `rowCount` == `layerCount` —
and the can't-miss chokepoint, co-located with the row signals):

- Add `Q_PROPERTY(int layerCount READ layerCount NOTIFY layerCountChanged)` to
  `cwBrushStructureModel` (read-only computed property + `NOTIFY` getter — the
  standard pattern `dirty`/`loaded` already use; **not** a `QBindable`, so no
  house-rule conflict).
- `emit layerCountChanged()` in `insertLayer` / `removeLayer` (definitionally
  change the count) and in `setBrush` **only when the count actually differs**.
  `moveLayer` emits nothing (count unchanged).
- QML binds `editorId.structureModel.layerCount >= 2` at the two sites
  (`structureModel` is a `CONSTANT` property, so the chain is stable). Drop the
  `()` call form. C++ callers (`editor->layerCount()`,
  `m_structureModel->layerCount()`) are unaffected — a `Q_PROPERTY` READ accessor
  is still a normal method. Sweep `tst_BrushEditor.qml` for any
  `editorId.layerCount()` call sites and convert them.

**Proof test:** drive `addLayer` and assert the *pre-existing* layer's grip
(`layerDragHandle_0`) becomes enabled/opaque **without** a `modelReset` — exactly
the regression Phase A would otherwise introduce (ties in with the `reuseItems`
verification below). Plus a `SignalSpy` on `layerCountChanged`: `addLayer` → 1,
`removeLayer` → 1, `moveLayer` → 0, a `loadBrushNamed` that changes the count → 1.

## Phase B — stage-based **category** encoding (optional, closes the latent gap)

A category *is* a contiguous stage block, and stages are unique within a layer,
so a category's stable identity is its **`stage` value** (Generate / Mutate /
Stamp / unknown-sentinel). Encode the category by its stage instead of its
positional index. That removes the last positional ancestry, makes the whole
`internalId` position-free, and fixes the latent mid-stack-rule-insert stranding
above. Map stage→position via `categoryBlocks()` in `parent()`/`data()`.

Sequence A first (focused, fully testable on its own), pause for review, then B.

---

## `reuseItems: true` — hypothesis + verification (fix soon)

`BrushEditorPanel.qml:216-221` sets `reuseItems: false`. Per commit `8cde5c69`:

> "The drag-reorder render glitch needed `reuseItems: false`: the view pooled a
> moved/removed delegate and stranded its recycled item live (**Metal RHI only;
> offscreen relayouts correctly**)."

**Hypothesis:** the `reuseItems: false` workaround and the positional-internalId
fragility share a root cause — stale persistent indexes after structural edits
on nested parents. If so, the position-free encoding lets us restore
`reuseItems: true` (delegate pooling back on) and delete the
`Qt.callLater(expandRecursively)` churn for non-load edits.

**Caveat — verify, do not assume.** The workaround was added for the *rule* drag
(`moveRule`), a *same-category* move that by the encoding analysis does **not**
strand a grandchild (the rule's `categoryIndex` is unchanged). The "Metal-only,
offscreen is fine" detail also points partly at a Qt scene-graph pooling quirk
rather than a pure model-index logic error. So treat this as a verification step:

1. Land Phase A (and probably B, since the rule path is where the glitch lived).
2. Flip `reuseItems: true`, run `tst_BrushEditor` under
   `QSG_RHI_BACKEND=metal` (the only backend that showed the glitch — offscreen
   never reproduced it, so this needs an on-screen / Metal run, not just the
   offscreen suite), and exercise rule drag + layer drag + add/remove by hand.
3. If the glitch is gone → remove `reuseItems: false` and the re-expand
   workaround for structural edits (keep the re-expand for `setBrush`
   load/discard). If it persists → it's a genuine Qt Metal pooling bug; keep the
   workaround and update the comment to say so (and that the encoding is no
   longer the reason).

---

## Tests (the real proof of the contract)

New cases in `testcases/test_cwBrushStructureModel.cpp` (or the editor test if
that's where structure-model coverage lives):

- Hold `QPersistentModelIndex` to a **category** and a **rule** under layer 2,
  then `moveLayer` / `insertLayer(0, …)` / `removeLayer(0)` and assert the
  persistent indexes **still resolve to the same rule** (`layerIndexOf` /
  `ruleIndexOf` track the move).
- `SignalSpy` on `modelReset` stays **0** across those ops, while
  `rowsMoved` / `rowsInserted` / `rowsRemoved` fire (count them).
- Existing `[cwBrushEditor]` and `tst_BrushEditor` stay green (encoding is
  internal; `layerIndexOf` / `ruleIndexOf` semantics unchanged).
- Phase B: repeat the persistent-index survival assertion across a mid-stack
  `insertRule` that creates a new category block before an existing one.

## Risks / edge cases

- `m_layerIds` desync with `decorations` — single invariant; centralize in the
  mutators + a `setBrush` reassign helper.
- `beginMoveRows` destination off-by-one — covered by the `to + 1` note and a
  move test in both directions.
- Empty inserted layer (no rules → no category rows) — `beginInsertRows` of one
  childless layer row; fine.
- Bit budget — stable layer id in 31 bits; monotonic counter, never reused
  within a session; `setBrush` may reset the counter on load.
