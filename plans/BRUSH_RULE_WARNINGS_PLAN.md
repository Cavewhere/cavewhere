# Brush editor — inline per-row rule warnings (leanest start)

## Goal
Show inline warning/error icons on the brush editor's rule rows (and layer rows)
when a layer's rule stack is malformed, driven by `cwDecorationLayerValidator`.
Storage/load stays faithful (Option C — already landed): rules keep authored
order; render sorts by stage; the editor *shows* when authored order/structure is
off so the user can fix it by dragging.

## Design decisions (settled)
- **2b — roles, not per-row `cwErrorModel`.** The structure tree is a
  `QAbstractItemModel`; per-row data is surfaced via `data()`/roles. Rows reorder
  by drag, so a position-keyed per-row `cwErrorModel*` would be churny and
  lifecycle-fragile (cwSurveyChunk's per-cell models work only because cells never
  reorder). Use `cwError` as the error type throughout; **no `cwErrorModel`
  instance yet** — we add a brush-level `cwErrorModel` rollup only when we build an
  overview widget later.
- **Out-of-order check lives in the shared `cwDecorationLayerValidator`** (single
  source of truth), severity **Warning** (render tolerates it via stable-sort, and
  faithful load tolerates it). IO already routes warnings non-fatally, so it will
  also surface at load — fine; editor-authored files are always in order.
- **Inline indicator = icon + tooltip** (keeps the fixed row height). Icon shown
  only when severity > none (no per-row "good" icon — too noisy).

## Where errors attach
A validator error with `ruleIndex >= 0` → shows on that **rule row**; `ruleIndex
== -1` (layer-level) → shows on the **layer row**.

## Backend — validator

### `cwSymbologyErrorCodes.h`
- Add `RulesOutOfOrder = Monad::ResultBase::CustomError + 8` (1032).

### `cwDecorationLayerValidator.{h,cpp}`
- Change return type to `QList<cwDecorationLayerError>` with a new struct in the
  header:
  ```cpp
  struct cwDecorationLayerError {
      cwError error;
      int ruleIndex = -1;   // flat index into layer.rules; -1 = layer-level
  };
  ```
- `ruleIndex` is the **flat authored index** into `layer.rules` (matches the
  structure model's flat rule index, including disabled rules in the count). Track
  the real loop index `i`, not a filtered position.
- **New: out-of-order detection.** Walk enabled+known rules in authored order,
  tracking `maxStageSeen`. A rule whose `stage() < maxStageSeen` → `RulesOutOfOrder`
  Warning at its flat index ("runs before the rules above it — out of pipeline
  order"). Then `maxStageSeen = max(maxStageSeen, stage)`. Disabled rules and
  unknown rules are skipped (unknown = kUnknownStage sorts last anyway).
- **Attribute existing per-rule errors to `ruleIndex`:**
  - `UnknownRule` → that rule's flat index (already in the loop; just record `i`).
  - `TwoTerminals` / `TwoTransformStrokes` → split from one layer-level error into
    **one error per *extra* terminal/transform** (the 2nd+), each at its flat
    index, message reworded per-rule ("this layer already has a terminal/stroke
    rule above; this one won't run"). Severity stays **Fatal**. (This is warning
    #2 — the dead-terminal case — shown on the offending row.)
  - Leave layer-level (`ruleIndex = -1`): `NoTerminalForRules`,
    `StampsWithoutGenerate`, `StampsWithoutGlyph`, `MissingGlyph`,
    `DeadRulesUnderTrace`.

### `cwSymbologyPaletteIO.cpp` (one call site, ~line 769)
- Iterate `cwDecorationLayerError`; use `.error` (and optionally `.ruleIndex` to
  enrich the located message). Fatal/warning routing unchanged.

### `test_cwDecorationLayerValidator.cpp`
- Update existing cases to read `.error`.
- Add: out-of-order brush → `RulesOutOfOrder` at the right flat index; two-terminal
  brush → one Fatal per extra terminal at its index; a clean brush → empty.

## Backend — structure model surfaces the per-row errors

### `cwBrushStructureModel.{h,cpp}`
- New roles: `RuleErrorSeverityRole` (int — worst `cwError::ErrorType` for the row,
  or a "none" sentinel) and `RuleErrorMessagesRole` (`QStringList`). Add to
  `roleNames()` as `"ruleErrorSeverity"` / `"ruleErrorMessages"`. Apply to both
  rule rows (errors with matching flat index) and layer rows (errors with -1).
- Hold `QSet<QString> m_availableGlyphNames` (setter; the validator needs it) and a
  cache `QVector<QList<cwDecorationLayerError>> m_layerErrors` (member, explicit
  lifetime — not a static).
- Private `revalidate()`: recompute `m_layerErrors` for all layers via
  `cwDecorationLayerValidator::validate(layer, registry, m_availableGlyphNames)`,
  then emit `dataChanged` on the two error roles across the affected rows. Call it
  at the end of `setBrush`, `insertRule`, `removeRule`, `moveRule`,
  `setRuleEnabled`. (The model already couples to `cwPlacementRuleRegistry` via
  `stageOf`, so the validator dependency is consistent.)
- `data()`:
  - rule row → worst severity / messages of `m_layerErrors[layer]` entries whose
    `ruleIndex == flatRuleIndex`.
  - layer row → same for entries whose `ruleIndex == -1`.

### `cwBrushEditor.cpp`
- In `loadBrushNamed` (and when the palette changes), set the model's available
  glyph names from the palette's glyphs:
  `m_structureModel->setAvailableGlyphNames(<names from m_palette->data().glyphs>)`
  before/at `setBrush`. Empty set when no palette.
- No other editor changes — the model revalidates itself on each mutation the
  editor already routes through it.

## Frontend — `BrushEditorPanel.qml`
- Add required delegate props for the new roles (`ruleErrorSeverity`,
  `ruleErrorMessages`).
- On the rule row's `RowLayout` (and the layer row), add a 16×16 icon shown only
  when `ruleErrorSeverity` is Fatal/Warning:
  - Fatal → `qrc:icons/svg/stopSignError.svg`; Warning → `qrc:icons/svg/warning.svg`.
  - Wrap with a `QC.ToolTip` (or HoverHandler + ToolTip) showing
    `ruleErrorMessages.join("\n")`.
  - Keep the fixed row height (icon fits within `kRowButtonSize`).
- Import `CwError` value-type enum (`import cavewherelib`) to compare severity, or
  expose severity as a plain int and compare numerically.

## Tests
- **C++ validator** (above).
- **C++ model/editor** (`test_cwBrushEditor.cpp`, structure-model section): load an
  out-of-order brush → offending rule row's `RuleErrorSeverityRole == Warning`,
  `RuleErrorMessagesRole` non-empty; clean brush → none; two-terminal brush →
  Fatal on the dead terminal row; after a corrective `moveRule`, the warning
  clears (revalidate fired).
- **QML** (`tst_BrushEditor.qml`): inline icon visible for a malformed brush and
  gone after the fix. Offscreen is fine — this is model-driven visibility, not the
  live-render-glitch class.

## Out of scope (later)
- Brush-level/ layer-level `cwErrorModel` rollup + an `ErrorIconBar` overview at
  the top of the editor (this is where the hierarchy earns its keep).
- Suppression of warnings; fix-it actions keyed off `errorTypeId`.
- Non-canonical-on-load auto-canonicalize (we stay faithful; warn instead).

## Build/test
```
cmake --build build/Qt_6_11_1_for_macOS-Debug --target cavewhere-test cavewhere-qml-test
./build/Qt_6_11_1_for_macOS-Debug/cavewhere-test "[cwDecorationLayerValidator][cwBrushEditor]"
QSG_RHI_BACKEND=metal ./build/Qt_6_11_1_for_macOS-Debug/cavewhere-qml-test --platform offscreen -input test-qml/tst_BrushEditor.qml
```
