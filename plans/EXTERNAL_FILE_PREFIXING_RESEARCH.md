# External File Prefixing Research

Background research for the umbrella issue [#511 — Use third-party survey files
directly](https://github.com/Cavewhere/cavewhere/issues/511) and its children
[#508 (Compass)](https://github.com/Cavewhere/cavewhere/issues/508),
[#509 (Walls)](https://github.com/Cavewhere/cavewhere/issues/509),
[#510 (Survex)](https://github.com/Cavewhere/cavewhere/issues/510).

The proposed direction is: external files (`.dat`/`.mak`, `.svx`, `.wpj`/`.srv`)
are user-owned, read-only inside CaveWhere, referenced from the `.cwproj` at
region / cave / trip level, and resolved by running `cavern` against a generated
driver `.svx` that `*include`s them. This document is the prefixing research
that should land before Phase 1 of that work begins.

Format docs consulted:

- Compass — <https://fountainware.com/compass/HTML_Help/Compass_Editor/surveyfileformat.htm>
- Survex — <https://survex.com/docs/manual/datafile.htm>, `compass.htm`, `walls.htm`
- Walls — `Walls_manual.pdf` §4 "Survey Data Format" (pp 64–74)

## 1. CaveWhere today

CaveWhere does not surface prefixes to the user. Internally:

- `cwStationValidator::validCharactersRegex()` accepts only
  `[a-zA-Z0-9_-]+`. Dots and colons are not allowed in a station name.
- Station names are unique per `cwCave` and stored unprefixed.

When CaveWhere runs cavern (via `cwSurvexExporterRegionTask` →
`cwSurvexExporterCaveTask` → `cwSurvexExporterTripTask` → `cwCavernTask`):

1. The region is wrapped in an anonymous `*begin` (`cwSurvexExporterRegionTask.cpp:50`).
2. Each cave is exported as `*begin <caveName>` where `caveName` has spaces
   stripped (`cwSurvexExporterCaveTask.cpp:38–40`).
3. Each trip is wrapped in an **anonymous** `*begin` with the trip name only
   in a comment + `*title` directive (`cwSurvexExporterTripTask.cpp:60–65`), so
   trips do not add to the prefix.

Critically, `cwLinePlotTask::buildInput` rewrites every cave's name to its
numeric index before export:

```cpp
// cwLinePlotTask.cpp:191 (approx, inside buildInput)
cave.name = QString("%1").arg(i);
```

So caves go to cavern as `*begin 0`, `*begin 1`, .... Cavern returns stations
labelled `0.A1`, `1.B2`, etc. (Survex's default separator is `.`.)
`cwLinePlotTask::splitLookupByCave` (`cwLinePlotTask.cpp:298–360`) strips the
prefix back with the regex:

```cpp
QRegularExpression regex(QString("(\\d+)\\.(%1)").arg(stationPattern));
// stationPattern = cwStationValidator::validCharactersRegex() = "(?:[a-zA-Z0-9]|-|_)+"
```

i.e. it expects exactly **one** numeric prefix segment followed by a dot and a
single station-name token. Any additional `*begin` block inside the cave would
produce `0.entrance.A1` and the regex would not match (the inner `entrance.A1`
contains a `.`, which `stationPattern` rejects). This is the load-bearing
assumption that breaks the moment an external file uses `*begin`/`*end`
internally — which essentially every real Survex file does.

`*case` is never emitted by the exporter, so cavern uses its default
(lowercase fold). That works today because CaveWhere station names go to cavern
in whatever case the user typed and come back lowercased — and CaveWhere's
lookups compare case-insensitively (`splitLookupByCave` uses
`Qt::CaseInsensitive`, and elsewhere `cwStation::canonicalKey` lowercases).

Cavern itself is invoked through the in-process `cavern_lib.h` C API
(`cwCavernTask.cpp:14, 80`), serialized through a static mutex because
`cavern_run` uses global state. The `.3d` is then parsed with `img.h` in
`cwSurvex3DFileReader.cpp`.

## 2. Survex prefix model (manual: `datafile.htm`)

- **`*begin <name>` / `*end <name>`.** Pushes `<name>` onto the survey prefix
  stack. Nestable to indefinite depth. Restores all settings on `*end`.
- **Separator** is `.` by default; configurable with `*set separator`.
- **`*prefix`** is deprecated (since 0.92); use `*begin`/`*end`.
- **`*include <file>`.** Settings (units, case, calibrations) carry into the
  included file, AND alterations carry back out — except the survey prefix,
  which is restored when the include returns. So the surrounding `*begin`
  scope is inherited at the include point and is the prefix any
  unwrapped stations in the included file pick up, but the included file's
  own `*begin`s are popped automatically on return.
- **`*equate s1 s2 ...`.** Fully-qualified station names; works across prefix
  boundaries. Required when bridging two namespaces.
- **`*export <station>`.** Required to make a station referable from the
  enclosing survey. Without it, an outer `*equate` to a name inside an
  inner block fails.
- **`*case preserve|toupper|tolower`.** Default is `tolower` (case-insensitive
  matching). External Survex files may or may not set `*case preserve`.
- **`*infer equates on`** can turn zero-length legs into implicit equates.
  Relevant if a user file relies on it.
- **`*truncate <n>`** limits significant chars in a name — unlikely to surface
  but worth knowing it exists.

## 3. Compass (`.dat` / `.mak`) via Survex (manual: `compass.htm`)

From the Compass docs (`surveyfileformat.htm`):

- A `.dat` file contains one or more **surveys**, separated by form-feed
  (`0x0C`). Each survey has a "SURVEY NAME" header — up to 12 chars — that is
  also the station prefix: a survey named `AB` produces stations `AB1`, `AB2`,
  etc. Compass treats those as opaque strings; the "prefix" is just a naming
  convention, not a directive.
- Station names are up to 12 printable-ASCII chars, **case-sensitive**.
- No directive-level prefix mechanism exists. The `.mak` file does not impose
  a prefix; it links `.dat` files and declares fixed stations and datum.

Survex's reader:

- Survex `*include`s `.dat` files directly: `*include compassfile.dat` works.
- Survex auto-enables `*case preserve` for `.dat` reads (per the manual: the
  `.dat` reader uses the equivalent of `*case preserve`). Compass names are
  preserved as-typed.
- The docs do **not** explicitly say each survey block becomes its own
  prefix. From the manual wording and observed behavior, the survey-name field
  is treated as a station-name prefix string, not as a `*begin`, so within one
  `.dat` two surveys with names `AB` and `CD` produce flat `AB1`, `CD1` rather
  than `AB.1`, `CD.1`. This needs empirical confirmation against the survex
  submodule's reader before Phase 1.
- The `.mak` file's link-station overrides are reported by the manual as
  "Link stations are ignored." Fixed stations from `.mak` participate but
  do not add a prefix.

**Net effect for CaveWhere:** `*include`ing a Compass `.dat` inside our
driver's `*begin <something>` block puts every Compass station under
`<something>.<compassname>` — no nested levels from the file itself, just
the prefix we wrap it in.

## 4. Walls (`.wpj` / `.srv`) — manual §4

- `.srv` is a free-form text format. Station names are case-sensitive by
  default (`#Units CASE=...` can override).
- Unprefixed station names are max 8 chars and may not contain `: ; , #`
  or embedded tabs/spaces (p. 66).
- **Prefix separator is `:`** (colon), not `.` — confirmed by the
  "must not contain any colons" rule for unprefixed names.
- **`#Prefix`** directive sets the active prefix for following data
  (example p. 65: `#Prefix HACK` makes the surrounding `H1` shorthand for
  `HACK:H1`). Prefixes can be set inline; the manual's example shows them
  also being inherited from the project tree.
- **`.wpj` Compile Options** (p. 70): "global assignments (name prefixes,
  declinations, etc.)" can be attached to project-tree branches. WPJ branches
  inject prefixes into descendant `.srv` files processed below them. So a
  `.wpj` book can act as a prefix scope without any directive in the `.srv`.
- "Station names, with their implied or explicit prefixes, must be globally
  unique within any subset of data files that are to be compiled together"
  (p. 66) — i.e. Walls treats the full prefixed name as the station identity.

Survex's Walls reader (`walls.htm`):

- Maps Walls's `#Prefix` and WPJ branch prefixes onto its own `*begin`
  hierarchy — exact mapping not documented in the manual, but Survex
  internally normalises the `:` separator into `.`.
- Quirks: Walls allows an explicit prefix with an empty name (`HACK:`) →
  Survex emits `HACK.empty name` (with a literal space) to avoid collisions.
  Walls's 8-char unprefixed limit is not enforced by Survex.

**Net effect for CaveWhere:** the prefix levels a `.wpj` or `.srv` produces
land in Survex's normal dotted hierarchy. Names may contain spaces (from the
empty-name quirk) and may exceed CaveWhere's current validator regex.

## 5. Implications for the external-file feature

### 5.1 The single-numeric-prefix assumption has to go

The current `splitLookupByCave` regex hard-codes "one numeric segment, dot,
station name". This won't survive:

- Survex files using `*begin foo` internally → stations come back as
  `<cave>.foo.A1`.
- Walls books contributing prefix segments through WPJ Compile Options →
  same shape, possibly multiple levels.
- Compass survey-name prefixes (`AB1`) — these are *not* a separate prefix
  level (they're baked into the name), so dot-splitting is fine here, but the
  `[a-zA-Z0-9_-]+` validator rejects 12-char survey names? No — 12 chars of
  alphanumerics are fine. But the validator does reject names with dots, so
  any leftover prefix segment from a Survex/Walls file mid-name will fail.

The fix is to treat the cave prefix as "everything up to and including the
final segment separator" and the station name as "the rest, kept verbatim".
The rest can contain dots — but then CaveWhere's notion of "station name"
needs to accept dots (and possibly colons-after-normalization-to-dots, and
spaces, per the Walls quirk).

Two reasonable shapes:

1. **Keep the inner prefix as part of the name.** A scrap pointing at the
   Survex station `entrance.A1` literally stores `entrance.A1` as its
   reference. The validator regex grows to allow `.` and probably space.
   Pro: round-trip is lossless and obvious.
   Con: existing CaveWhere references typed by users are always unprefixed,
   so two namespaces exist within one cave when it's mixed.
2. **Flatten on the way in.** Replace `.` with `_` or similar. Pro: stays
   in the existing validator. Con: ambiguous when source legitimately uses
   underscores, and loses the prefix information when users want to
   cross-reference back to their external tool.

Recommendation: option 1, broaden the validator. The data the user sees in
their external tool is what they should see in CaveWhere.

### 5.2 Don't rename caves to integers when externally backed

`cwLinePlotTask::buildInput`'s integer-rename is fine for fully-native caves
(synthetic name lets `splitLookupByCave` parse the prefix back), but for an
externally-backed cave we either:

- Don't wrap it in our own `*begin` at all (let the external file's own
  structure provide the prefix). Then we have no synthetic level to strip and
  station names come back with whatever prefix the file uses.
- Wrap in `*begin <stable-uuid>` where the uuid is a guaranteed-not-to-collide
  string chosen by CaveWhere, and store the uuid in the project. The prefix
  segment becomes the cave's identity rather than its display name.

The second is safer (mixed projects with native + external caves can use the
same machinery; collisions across externally-attached files are
auto-resolved) but only if we update the consumer to look up the cave by uuid
instead of by parsing an integer index.

For a **region-level** attach (user owns the whole centerline in an external
tool), there's no per-cave `*begin` to add — the file structure is the
structure. Caves in this mode are "virtual": `cwCave` records exist to hold
notes/scraps and reference a subtree of the resolved station network. Their
naming has to come from the external file's prefix.

### 5.3 Case sensitivity has three regimes in one cavern run

In the same generated driver we may have:

- CaveWhere-native shots: today implicitly case-insensitive via cavern's
  default `*case tolower`.
- Compass `*include`: cavern auto-applies `*case preserve`.
- Survex `*include`: whatever the file says; defaults to `*case tolower`.
- Walls `*include`: case-significant by default; configurable.

Mixed in one run, the global default flips per `*include`d file (the manual
confirms `*include` carries settings into the included file, and they carry
back out except for the prefix). The pragmatic choice for the driver wrapper
is to set `*case preserve` at the top, so CaveWhere never relies on cavern
folding case. Our internal `cwStation::canonicalKey` lowercases for matching
anyway. The risk: existing native-only projects whose users typed mixed-case
shot names that happen to currently collide-and-resolve via cavern's lowercase
default would change behavior. Worth checking that the existing test suite
covers this, and possibly only setting `*case preserve` inside the wrapper of
each externally-attached `*begin` block rather than at the file root.

### 5.4 Driver-file shape (proposed)

Per region:

```svx
*case preserve            ; safe default for externally-attached blocks
*cs out <region-cs>

; CaveWhere-native cave (existing path)
*begin cave_<uuid-A>
  ...native shots, *fix, etc...
*end cave_<uuid-A>

; Externally-attached cave (Survex source)
*begin cave_<uuid-B>
  *include "../external/system.svx"
*end cave_<uuid-B>

; Externally-attached cave (Compass source)
*begin cave_<uuid-C>
  *include "../external/system.dat"
*end cave_<uuid-C>

; Externally-attached cave (Walls project)
*begin cave_<uuid-D>
  *include "../external/system.wpj"
*end cave_<uuid-D>
```

Trip-level attach is the same pattern, with an extra `*begin trip_<uuid>`
inside the cave's block. Region-level attach skips the wrapping `*begin
cave_*` entirely and instead requires the user to add `cwCave` records that
declare which prefix subtree of the resolved network they cover (deferred to
Phase 4).

Equates land as `*equate <native-fully-qualified> <external-fully-qualified>`
either right after the relevant `*end` (so both names are resolvable) or in
a dedicated `equates.svx` at the bottom of the driver (Phase 5).

### 5.5 The validator and the cwStation type

`cwStationValidator::validCharactersRegex()` currently:

```cpp
QRegularExpression("(?:[a-zA-Z0-9]|-|_)+");
```

To accept names that arrive from Survex/Walls/Compass via cavern, this needs
to permit:

- `.` — Survex prefix separator survives in names from nested `*begin` blocks.
- Possibly ` ` (space) — Walls empty-name quirk produces literal spaces.
- Compass keeps within alphanumerics so no change needed for it.

The validator is also used by user-facing input controls. Broadening it
globally will let users type dotted/spaced station names everywhere, which
isn't desirable in native-only projects. Reasonable scope: a separate
"external station name" validator used only where references to
externally-resolved stations are stored (scrap/note station-name fields when
the owning cave/trip is external).

## 6. Open questions — answers

Verified empirically using
`build/Qt_6_11_1_for_macOS-Debug/survex/cavern` (Survex v1.4.17-50-g159f7a5b)
on inputs in `/tmp/cw-prefix-research/`. Each test wrapped the data in a
`*begin cave0` block to mimic the proposed driver shape.

### Q3 — Case mode (the load-bearing one)

**Answer: do NOT add `*case preserve` (or any `*case` directive) at the
driver root.** Keep cavern's default `*case tolower` for the wrapper, and
let each external `*include` bring its own case setting via its own
directives.

Empirical proof — two trips in one cave, one types `A1 A2`, the other types
`a2 a3`, intending `A2` and `a2` to be the same station:

| Driver | `*case` | Stations reported | Components | Connects? |
|--------|---------|-------------------|------------|-----------|
| `q3a_case_default.svx` | (default `tolower`) | `cave0.a1`, `cave0.a2`, `cave0.a3` (3) | 1 | yes — `A2`/`a2` merge |
| `q3b_case_preserve.svx` | `*case preserve` at root | `cave0.A1`, `cave0.A2`, `cave0.a2`, `cave0.a3` (4) | 2 | **no — loop breaks** |

CaveWhere itself is already case-insensitive on the consumer side
(`cwStation::canonicalKey` lowercases for every position lookup —
`cwStation.h:66`, `cwStationPositionLookup.h:51-67`), so it doesn't care what
case cavern returns. The breakage is entirely upstream: with `*case
preserve` cavern stops merging stations that the user expects to be the
same, splits the network into two components, and CaveWhere's lowercased
lookup ends up storing whichever of `A2`/`a2` cavern writes last —
silently losing positions.

Survex `*case` is a per-`*begin` scoped setting (saved on `*begin`,
restored on `*end`), so the right pattern is to leave the wrapper untouched
and rely on:

- Native CaveWhere blocks → inherit cavern default (`tolower`), unchanged
  from today.
- Compass `*include` → reader auto-applies `*case preserve` inside the
  include scope; restored on return.
- Survex `*include` → whatever the file declares; default `tolower` if it
  doesn't say.
- Walls `*include` → reader handles Walls's case rules.

Cross-case-mode equates (Phase 5) will need care because a `*equate` at
the wrapper level sees `tolower`-folded names from one block and
`preserve`-cased names from another — but that's a Phase 5 problem, and
the user-visible equate UI can normalize.

### Q1 — Compass `.dat` survey-name prefixes

**Answer: Compass survey names are NOT translated into nested Survex
prefix levels. They are flat name-prefix strings only.**

`q1_compass.dat` has two surveys (`AB` and `CD`) separated by a form-feed,
each defining three stations. `*include`d inside `*begin cave0`, cavern
reports:

```
cave0.AB1, cave0.AB2, cave0.AB3
cave0.CD1, cave0.CD2, cave0.CD3
```

Not `cave0.AB.1` / `cave0.AB.AB1`. The Compass survey name is just baked
into the station name as-typed — exactly what the `.dat` file already
shows. Confirms case is preserved as well (capitalised `AB`/`CD` come
through unchanged).

**Implication for CaveWhere:** when an external Compass `.dat` is
attached, no extra prefix levels appear — stations come back as
`<our-wrapper>.<verbatim-compass-name>`. The `validCharactersRegex`
broadening from §5.5 is not needed for Compass on its own (alphanumerics
fit), but it's still needed for Survex/Walls.

### Q5 — `*export` requirement for cross-block equates

**Answer: cavern accepts fully-qualified `*equate` from the wrapper
level into nested external blocks without `*export`.** Tested
`q5b_export_deep.svx`:

```svx
*begin cave0
  *begin inner
    *data normal from to tape compass clino
    A1 A2 10 0 0    ; A2 is NOT *exported
  *end inner
*end cave0

*begin cave1
  *data normal from to tape compass clino
  B1 B2 5 45 0
*end cave1

*equate cave0.inner.A2 cave1.B1   ; reaches into cave0.inner
```

Cavern: "Survey contains 4 survey stations, joined by 3 shots. There are
0 loops." One connected component. The equate succeeded without
modifying `cave0/inner` to `*export A2`. The manual's `*export`
requirement appears to apply only to specific commands (e.g. `*fix` or
when a non-equate command references a station from outside the block
that defines it); `*equate` with fully-qualified names from above works.

**Implication for CaveWhere:** the equates UI (Phase 5) can generate
top-level `*equate <native-fq> <external-fq>` directives without
requiring any modification of the external file. Big simplification —
we never have to ask the user to edit their `.svx` to add `*export`
lines.

### Q6 — `*infer equates` carried through `*include`

**Answer: `*infer equates on` set inside an included file is honored for
data in that file, and (per the manual) the setting persists back to the
parent after the include returns.** Tested `q6_infer_equates.svx` (parent
just `*include`s an inner file that turns on `*infer equates` and uses a
zero-length leg `A2 B1 0 0 0` as an implicit equate):

Result: A1/A2/B1/B2 at coordinates 0,0,0 / 0,10,0 / 0,10,0 / 10,10,0 —
A2 and B1 share a coordinate, confirming the implicit equate fired.

**Implication for CaveWhere:** if external files use `*infer equates`,
they Just Work through our `*include`. We should NOT emit `*infer equates
off` (or any other override) in the driver, and we should be aware that
an external file's `*infer` directive can affect any cavern-native data
we emit *after* the include in the same scope. Safest: order the driver
so external `*include`s sit inside their own `*begin cave_<uuid>` blocks
and any wrapper-level emissions sit outside, so settings don't leak.

### Q2 — WPJ branch prefixes

**Answer: deferred. Constructing a minimal WPJ that exercises Compile
Options branch prefixes requires Walls-specific WPJ syntax that the
docs don't fully expose, and the result depends on Survex's Walls reader
implementation.** From the documentation:

- Walls allows per-branch Compile Options in the `.wpj` file that
  prepend a prefix to descendant `.srv` files (manual p. 70).
- Survex's Walls reader maps Walls's `:`-separated prefixes onto its own
  `.`-separated `*begin` hierarchy (walls.htm).
- Walls's empty-prefixed-name quirk (`HACK:`) becomes `HACK.empty name`
  with a literal space in the Survex name.

**Implication for Phase 1:** assume Walls prefixes produce arbitrary
levels of dotted nesting under our wrapper, possibly with spaces in
names. Defer empirical verification to when we have access to a real
Walls project; flag it as a Phase 1 acceptance criterion ("attach a
real Walls project and verify all stations are addressable").

### Q4 — Cave UUID migration

**Answer: no persistence migration needed. The integer cave naming is
purely transient.** Investigation:

- `cwLinePlotTask::buildInput` (cwLinePlotTask.cpp:~191) constructs a
  `cwCavingRegionData` snapshot from the live `cwCavingRegion` and
  rewrites each `caveData.name` to its index in the snapshot. The
  real `cwCave::name()` on the model is untouched.
- The snapshot is what flows through `cwSurvexExporterRegionTask` →
  `*begin <i>` in the driver, and `splitLookupByCave` parses the same
  `<i>` back to recover the cave index.
- Switching to a UUID is therefore a two-point change with no persisted
  field: pick a stable id (per-cave UUID stored on `cwCave`, generated on
  add, persisted in the project), use it as the `*begin` label in
  `buildInput`, and update `splitLookupByCave`'s regex to match the UUID
  pattern. No on-disk schema migration required for the cave id itself
  — but external references (Phase 1's protobuf addition) will need
  their own stable ids, which is the same mechanism.

## 7. Net effect on the Phase 1 design

Updates from the empirical run:

- **Driver root contains no `*case` directive** (was previously
  "set `*case preserve`"). Existing native behavior is preserved.
- **Equates UI in Phase 5 generates plain `*equate` lines** at the
  driver root; no `*export` injection needed, and we never have to ask
  users to edit their external files.
- **`*infer equates` and other settings** from external files affect
  only inside their `*begin` block as long as we keep each external
  `*include` in its own block and don't emit cavern-native data after
  it in the same scope — already the proposed shape.
- **Compass attaches need no extra prefix scope handling**: survey
  names baked in as-typed.
- **Walls attaches** remain the unknown — Phase 1 acceptance criterion
  should include attaching a real Walls project.
- **No project-format migration**: the UUID change is purely runtime in
  the line-plot pipeline, plus a new persisted field on `cwCave` for
  the stable id (initialised lazily for existing caves at load time).

## 7. Phase 1 implications

Based on the above, Phase 1 (data model + driver-file generator + runner +
watcher) needs to deliver:

- New `cwExternalReference` value type at region/cave/trip level: external
  file path (relative to project), source format hint, optional prefix
  override (Phase 5 hook), stable internal id (the UUID used in `*begin`).
- `cwSurvexExporterRegionTask` extension that emits `*include` for external
  refs and switches cave-id strategy from integer-index to UUID.
- `cwLinePlotTask::splitLookupByCave` rewrite: prefix is "the UUID we
  generated"; station name is "everything after the first separator,
  verbatim". Reject only when the leading UUID doesn't match a known cave.
- `cwStationValidator` companion regex for externally-resolved station names
  that permits `.` and ` `.
- File watcher hooked into `cwLinePlotManager` so changes to referenced
  external files trigger a re-run via the existing `Restarter`.
- Driver writes `*case preserve` at the top, after a test pass confirms it
  doesn't regress native-only projects.

CavernOutputPage, badging, and orphan detection come in later phases per the
plan thread.
