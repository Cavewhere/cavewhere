# Commit B — Interpret placement-rule params via QVariant (decode at load)

Self-contained implementation plan. All design questions are **settled** (see
"Decisions" below). Implement the file changes in order, then run the tests.
Do **not** `git commit` unless explicitly asked. Commit messages must not mention
Claude / Co-Authored-By. Branch: `fall2026`. Build dir:
`build/Qt_6_11_1_for_macOS-Debug`.

## Goal

Stop discarding placement-rule params. Today the layout looks up `name → rule`
and runs every rule from a hard-coded constant (`kDefaultStampSpacingMm = 2.0`).
Make each param-bearing rule read its own typed parameters. First (and only)
consumer this commit: **Uniform spacing / `spacingMm`**.

This is Commit B, following Commit A (`d9c8389d`, "Share FileVersion/Stroke
across schemas and gate palettes on format version"), which added the
save/load format-version gate (`cwRegionIOTask::isVersionSupported` /
`newerVersionWarning`, `protoVersion() == 9`).

## Decisions (all settled with the user — do not re-litigate)

1. **No proto in the runtime core.** Proto is for disk serialization only.
   Rules read a plain C++ struct via `QVariant`, never a proto message.
2. **Architecture B — QVariant in the data model, decode at LOAD.**
   `cwPlacementRuleData` carries a `QVariant parameters` (not bytes). The codec
   runs inside `cwSymbologyPaletteIO` (decode on load, encode on save). Proto is
   touched **only** in IO + the codec. Chosen over "bytes in data, decode at
   layout" for efficiency (params parse once at load, not per re-layout) and for
   the literal "proto is disk-only / IO boundary" property.
3. **No `protoVersion()` bump now**, but adopt + document the **bump-rule**:
   editing any `*Params` message's fields (add/remove) MUST bump
   `cwRegionIOTask::protoVersion()`. Rationale: the codec decodes proto into a
   plain struct that does **not** carry proto unknown-fields, so an un-bumped
   newer field would be silently dropped on re-save (data loss). Bumping makes
   the Commit-A version gate open such palettes read-only on older builds
   instead. Introducing a brand-new param message (like `UniformSpacingParams`
   now) does NOT bump: pre-B builds preserve the whole `params` as opaque bytes,
   so there is no loss and the gate stays untriggered (file stays version 9).
4. **First consumer:** `Uniform spacing` / `spacingMm` (fallback 2.0 mm).

## How the version gate (Commit A) interacts — keep in mind

- Pre-B builds have `QByteArray params` and copy it verbatim through IO, so a
  B-saved palette with `spacingMm=3` round-trips losslessly on an old build
  (renders at the old 2 mm default, preserves the bytes on re-save). No bump,
  no read-only. Reopening in a B build restores 3 mm.
- The danger is only *future* field additions to an existing `*Params`; the
  bump-rule + gate handle that. This commit must DOCUMENT the rule and TEST the
  lossless round-trip, but changes no gate code.

## Codec behavior matrix (the crux — get this exactly right)

`decode(const QString &name, const QByteArray &bytes) -> QVariant`:

| Input | Result QVariant |
|---|---|
| empty bytes | **null** `QVariant()` |
| known name, valid bytes | `QVariant::fromValue(cwUniformSpacingParams{…})` |
| known name, **malformed** bytes | `QVariant::fromValue(bytes)` — preserve raw `QByteArray` |
| **unknown** name, non-empty | `QVariant::fromValue(bytes)` — preserve raw `QByteArray` |

`encode(const QString &name, const QVariant &v) -> QByteArray` — dispatch on
**what `v` holds**, not just the name:

| `v` holds | Result bytes |
|---|---|
| invalid/null | empty `QByteArray()` |
| `QByteArray` | that byte array (passthrough: unknown + malformed) |
| known struct (e.g. `cwUniformSpacingParams`) for a known `name` | re-serialized proto bytes |
| (defensive: anything else) | empty |

Net effects:
- empty ⇄ null (byte-stable; floor-step's empty params stay empty).
- unknown/future rules and malformed blobs round-trip verbatim (no loss).
- The rule always calls `value<cwUniformSpacingParams>()`; a null variant or a
  raw-bytes variant both fall to the struct's `2.0` default — no presence check.

## File-by-file changes

### 1. `cavewherelib/src/cavewhere_symbology.proto`
Add near `message PlacementRule` (which stays `{ string name = 1; bytes params = 2; }`):

```proto
// Per-rule parameter messages. Serialized into PlacementRule.params (opaque
// bytes on the wire). Decoded ONLY by cwPlacementRuleParamsCodec.
//
// BUMP-RULE: adding or removing a field in ANY *Params message below MUST bump
// cwRegionIOTask::protoVersion(). The codec decodes these into plain C++ structs
// that do NOT carry proto unknown-fields, so an un-bumped newer field would be
// dropped on re-save. Bumping makes the version gate open such palettes
// read-only on older builds instead of silently losing the field. Introducing a
// brand-new *Params message (no older build interprets it) does NOT bump.
message UniformSpacingParams {
    optional double spacingMm = 1;   // unset -> 2.0 mm (cwUniformSpacingParams default)
}
```

### 2. NEW `cavewherelib/src/cwPlacementRuleParams.h`
Plain value struct(s), no proto. Default member = the documented fallback.

```cpp
#ifndef CWPLACEMENTRULEPARAMS_H
#define CWPLACEMENTRULEPARAMS_H

#include <QMetaType>

// Typed, proto-free view of a placement rule's parameters. The default member
// values ARE the documented fallbacks, so a rule reading value<T>() off a null
// or wrong-typed QVariant gets the right defaults with no presence check.
struct cwUniformSpacingParams {
    double spacingMm = 2.0;   // was cwUniformSpacingRule's kDefaultStampSpacingMm
    bool operator==(const cwUniformSpacingParams &other) const = default;
};

Q_DECLARE_METATYPE(cwUniformSpacingParams)

#endif // CWPLACEMENTRULEPARAMS_H
```

### 3. NEW `cavewherelib/src/cwPlacementRuleParamsCodec.{h,cpp}`
The only module that includes `cavewhere_symbology.pb.h`. Free functions in a
namespace `cwPlacementRuleParamsCodec`.

Header:
```cpp
#ifndef CWPLACEMENTRULEPARAMSCODEC_H
#define CWPLACEMENTRULEPARAMSCODEC_H

#include "CaveWhereLibExport.h"
#include <QByteArray>
#include <QString>
#include <QVariant>

// Bidirectional bridge between persisted PlacementRule.params bytes and the
// typed, proto-free param structs the rules read. The ONLY proto-aware module
// in the rule-params path; see the codec behavior matrix in
// plans/COMMIT_B_RULE_PARAMS_PLAN.md.
namespace cwPlacementRuleParamsCodec {
    CAVEWHERE_LIB_EXPORT QVariant   decode(const QString &name, const QByteArray &params);
    CAVEWHERE_LIB_EXPORT QByteArray encode(const QString &name, const QVariant &params);
}

#endif // CWPLACEMENTRULEPARAMSCODEC_H
```

`.cpp` — implement the matrix. Use the rule's `displayName()` string as the key
(`QStringLiteral("Uniform spacing")`). For decode: `isEmpty()` → null; look up
name; if known, `UniformSpacingParams p; if (p.ParseFromArray(...)) -> struct
else -> fromValue(bytes)`; unknown → `fromValue(bytes)`. For encode: `!v.isValid()`
→ `{}`; `v.typeId() == QMetaType::QByteArray` → `v.toByteArray()`; if name known
and `v` holds the struct (`v.canConvert<cwUniformSpacingParams>()` /
`v.typeId() == qMetaTypeId<cwUniformSpacingParams>()`) → serialize; else `{}`.
Mirror the `toStd`/byte helpers already used in cwSymbologyPaletteIO.cpp if
convenient, but the codec is self-contained.

### 4. `cavewherelib/src/cwPlacementRuleData.h`
Change `QByteArray params` → `QVariant parameters`. Swap `#include <QByteArray>`
for `#include <QVariant>`. Keep `#include <QMetaType>` and `#include <QString>`.
`bool operator==(...) const = default;` now compares `QVariant` (verified by
test — Qt 6 compares registered custom types that have `operator==`). Update the
struct's doc comment ("raw (name, params)" → "(name, typed parameters)").

### 5. `cavewherelib/src/cwSymbologyPaletteIO.cpp`
Add `#include "cwPlacementRuleParamsCodec.h"`.

In `layerToProto` (currently ~line 95-99 region; the `add_rules()` loop):
```cpp
auto *protoRule = proto->add_rules();
protoRule->set_name(toStd(rule.name));
const QByteArray bytes = cwPlacementRuleParamsCodec::encode(rule.name, rule.parameters);
protoRule->set_params(bytes.constData(), static_cast<size_t>(bytes.size()));
```

In `layerFromProto` (currently ~line 119-126 region; the `proto.rules()` loop):
```cpp
cwPlacementRuleData rule;
rule.name = fromStd(protoRule.name());
const std::string &params = protoRule.params();
rule.parameters = cwPlacementRuleParamsCodec::decode(
    rule.name, QByteArray(params.data(), static_cast<int>(params.size())));
layer.rules.append(rule);
```

### 6. `cavewherelib/src/cwPlacementRule.h`
Add to `struct cwPlacementContext`, **before** `scene`:
```cpp
    // This rule invocation's decoded params (proto-free; see
    // cwPlacementRuleParams.h). Null/wrong-typed -> the rule's value<T>()
    // default. The layout rebuilds the context per rule so each gets its own.
    QVariant ruleParameters;
```
Add `#include <QVariant>`. Field order becomes: strokePath, layer,
worldPerPaperMm, **ruleParameters**, scene.

### 7. `cavewherelib/src/cwSketchDecorationLayout.cpp`
`resolveRuleStack` must keep `(rule, QVariant params)` paired through the stage
sort (it currently returns `QVector<const cwPlacementRule*>`). Introduce a small
struct in the anon namespace:
```cpp
struct ResolvedRule { const cwPlacementRule *rule; QVariant parameters; };
```
- `resolveRuleStack` returns `QVector<ResolvedRule>`; push
  `{rule, ruleData.parameters}`; keep the unknown-name drop + thread_local
  warned-set; stable_sort by `a.rule->stage() < b.rule->stage()`.
- `terminalOf` takes/returns `ResolvedRule` (find the one whose
  `rule->stage() == Terminal`); return by pointer or a found-flag.
- In `layout()`: build the per-rule context inside the apply loop:
  ```cpp
  for (const ResolvedRule &step : stack) {
      const cwPlacementContext context{strokePath, layer, worldPerPaperMm,
                                       step.parameters, nullptr};
      step.rule->apply(positions, context);
  }
  ```
  For the terminal materialization (stampPath / tracePolylines), build a context
  with the **terminal's** parameters:
  ```cpp
  const cwPlacementContext terminalCtx{strokePath, layer, worldPerPaperMm,
                                       terminal.parameters, nullptr};
  ```
  Use `terminalCtx` for `terminal.rule->outputKind()`,
  `tracePolylines(strokeWorld, terminalCtx)`, and
  `stampPath(position, glyphPath, terminalCtx)`.

### 8. `cavewherelib/src/cwPlacementRules/cwUniformSpacingRule.cpp`
- `#include "cwPlacementRuleParams.h"`.
- Delete the anon-namespace `kDefaultStampSpacingMm` (the default now lives in
  the struct).
- In `apply`:
  ```cpp
  const auto params = context.ruleParameters.value<cwUniformSpacingParams>();
  const double spacingWorld = params.spacingMm * context.worldPerPaperMm;
  if (spacingWorld <= 0.0) { return; }   // guard stays
  ```

## Tests

### NEW `testcases/test_cwPlacementRuleParams.cpp` (tag `[cwPlacementRuleParams]`)
- **Codec matrix:** encode(struct spacingMm=3)→non-empty bytes→decode→struct ==
  {3.0}. empty bytes → null QVariant; encode(null) → empty. unknown name +
  non-empty bytes → decode holds QByteArray; encode → same bytes (passthrough).
  malformed bytes for known name → decode holds QByteArray; encode → same bytes.
- **cwPlacementRuleData equality:** two equal `{name, QVariant{struct}}` compare
  equal; differing `spacingMm` compare unequal. (Proves Qt 6 custom-type QVariant
  comparison works; if it does NOT, fall back to a hand-written `operator==`
  that compares `name` + decoded structs, and note it.)
- **Rule honors params:** drive `cwUniformSpacingRule::apply` with a context
  whose `ruleParameters = QVariant::fromValue(cwUniformSpacingParams{3.0})`
  (worldPerPaperMm = 1.0) over a known-length straight `cwStrokePath`; assert
  tick count matches 3 mm spacing. null ruleParameters → 2 mm count.
  `QVariant::fromValue(QString("x"))` (wrong type) → 2 mm count.
- **IO round-trip (the B path):** build a `cwLineBrush` with a decoration layer
  whose `Uniform spacing` rule has `parameters =
  fromValue(cwUniformSpacingParams{3.0})`; save the palette via
  cwSymbologyPaletteIO, load it back, assert the loaded rule's `parameters`
  decodes to `{3.0}` (survives the JSON/base64 `bytes` round-trip).
- **Opaque-passthrough (old-build guarantee):** a rule with an UNKNOWN name +
  arbitrary param bytes survives save→load with bytes intact.

Put round-trip tests in this dedicated file (per the repo convention: feature
round-trip tests get their own file, not test_cwSaveLoad/test_cwProject).
Use `QCoreApplication::applicationPid()` / `QTemporaryDir` for any temp paths
(tests run as concurrent processes).

### Update `testcases/test_cwPlacementRules.cpp`
The ~19 `cwPlacementContext{strokePath, layer, X, nullptr}` aggregate inits now
have an extra field. Drop the trailing positional `nullptr` →
`cwPlacementContext{strokePath, layer, X}` (both `ruleParameters` and `scene`
fall to defaults). Verify none of these tests depend on `scene` being set
positionally (none do — all pass nullptr).

### Seed
`cwSymbologyPaletteSeed.cpp` unchanged: `cwPlacementRuleData{name, {}}` now
brace-inits a null `QVariant` (was empty `QByteArray`) → still means "no params
→ default". Confirm it still compiles; no edit expected.

## CMake
New `src/*.{h,cpp}` are auto-globbed (`CONFIGURE_DEPENDS`), as is
`testcases/*.cpp`. No CMakeLists edit expected — re-run cmake configure if the
glob doesn't pick them up.

## Verification

```bash
cd /Users/cave/Documents/projects/cavewhere_4
cmake --build build/Qt_6_11_1_for_macOS-Debug --target cavewhere-test 2>&1 | tee /tmp/cw-build.log
./build/Qt_6_11_1_for_macOS-Debug/cavewhere-test \
  "[cwPlacementRuleParams],[cwPlacementRules],[cwPlacementRuleRegistry],[cwSketchDecorationLayout],[cwSymbologyPaletteIO]" \
  -d yes 2>&1 | tee /tmp/cw-paramtest.log
```
Targeted single-tag runs only (don't run the full suite without asking). Pause
for review when built + green; do not commit unless asked.

## Out of scope (later commits)
- `Offset stroke` rule (signed `offsetMm`) and the ceiling-channel demo — next.
- Folding `offsetCurveOffsetMm` into a trace rule's params.
- Any editor/authoring UI (authoring stays C++ in seed/tests).
- Bumping `protoVersion()` (only happens when an existing `*Params` field changes).
