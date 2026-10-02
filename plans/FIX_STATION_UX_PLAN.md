# Fix Station page UX fixes

2026-08-19. Follow-up to `plans/FIX_STATION_DATUM_PLAN.md` after the user's
hands-on test of its C6. This plan **supersedes** that plan's §3.6 (the
elevation-reference feature is removed entirely, storage included) and
**revises** its §3.7 (the datum combo stays, but gated, filtered, and
region-labeled). Branch: `fixed_1`.

## 0. User decisions (verbatim intent)

1. The wide table gets cut off at lower widths — fix the breakpoint.
2. "—" / "Mean sea level" / "Ellipsoid (GPS)" won't be obvious. Decision:
   **remove elevation reference entirely** — CaveWhere's convention is that
   every elevation is a height above sea level. Consumer GPS already reports
   sea level, a picked elevation is the terrain's own number, and users
   reconcile against their DEM/LiDAR regardless. No hidden storage kept.
3. The crosshair (pick) button belongs **in the coordinate column**.
4. The datum combo stays, but: **bounds check + show only in-bounds datums**,
   **label datums by the region they're useful for** ("North America (USA)"
   for NAD83, "World (GPS)" for WGS84), and **disable the combo until the row
   has a valid coordinate** (default WGS84; the user types the coordinate
   first, then adjusts the datum from what's available). Disabled state gets
   a tooltip.
5. **Never silently swap a datum.** A coordinate edit never rewrites inputCS.
   When a coordinate leaves its datum's bounds, the existing domain warning
   is the messenger — switching a stored NAD83 row to ETRS89 is the user's
   act alone.
6. Consequence: the **C7 gap is retired by decision** — `newFixStation()`
   birthing rows on `EPSG:4326` is now correct, not missing.
   `cwCavingRegion::defaultFixDatum()` keeps exactly two consumers: the pick
   write (`setPickedPoint`) and the picker popup's readout (C5).
7. The enabled combo gets a **tooltip that explains what a datum is and
   recommends the plate-fixed datum** for the row's region.

## 1. Current state (verified 2026-08-19)

- `FixStationPage.qml` wide layout needs ~1030px (columns 130 + 400 + 300 +
  150 `elevationReferenceFieldWidth`, plus pick button and two warning
  icons), but `isNarrow` only trips below `Theme.breakpointPanelCollapse`
  (600). `TableStaticView.qml` is a vertical `ListView` with `clip: true` —
  no horizontal scroll exists. Between 600 and ~1030px the right side of
  every row is silently clipped.
- Elevation reference lives in: `cavewherelib/src/cavewhere.proto:124`
  (`optional ElevationReference elevationReference = 11;` on FixStation,
  whose reserved lines are 104–105), `cwFixStation.h:87–98` (enum) and
  `:192–195` (accessors) plus the QML namespace re-export around `:203`,
  `cwFixStationModel.h:58–61` (`ElevationReferenceRole`) and `:146`
  (`setPickedPoint` takes it as a parameter), `cwProtoUtils.cpp`
  (static_asserts pinning enum↔proto), `ElevationReferenceComboBox.qml`,
  `FixStationPage.qml` (component 227–249, column 293–297, wide cell
  463–467, narrow label+combo 610–624, `required property` at 375 and 512),
  `FixStationPopup.qml` (property 53, reload 153–164, combo 385–391),
  `Theme.qml:165` (`elevationReferenceFieldWidth`). The pick's tool/model
  path computes MeanSeaLevel from the LAZ WKT's vertical datum.
- The datum combo is `CSPicker.qml`'s `datumComboId` (lines 191–217), fed by
  `CoordinateSystem.datumList()` / `utmDatumList()`; `defaultDatum` is bound
  from `RootData.region.defaultFixDatum` at `FixStationPage.qml:221` and
  `:574` and in `FixStationPopup.qml`.
- The datum table is `kGeographicDatums` in `cwCoordinateTransform.cpp:76–85`
  (8 datums + WGS84). The per-datum region bounds already exist as
  `kPlateFixedRegions` in `cwLocalProjection.cpp:80–95` (lat/lon boxes →
  datum code). Note the **US and Canada boxes overlap** (lat 41.5–49.5 ×
  lon −125…−66.5): a border cave is legitimately in both.
- `cwFixStationDiagnostics::domainCheck` (cwFixStationDiagnostics.cpp:17)
  already answers per row through `cwCoordinateTransform::domainCheck`, which
  internally transforms the point to WGS84 via the per-thread
  `cachedTransform(key, Wgs84)` (cwCoordinateTransform.cpp:535). The row's
  lat/lon is therefore one small extraction away, in C++, cached.
- `cwFixStationDiagnosticsModel` is the `QIdentityProxyModel` the page binds;
  computed read-only roles start at `Qt::UserRole + 100`.

## 2. Commit sequence

Each commit builds green and runs its gate before the next starts. Work on
`fixed_1`. Never `git add` anything under `plans/`.

### U1 — Remove the elevation reference

The feature converts nothing (no geoid grids ship), consumer GPS already
reports sea level, and the pick's elevation is the terrain's by construction.
Remove it **entirely**, storage included — it only ever existed on unpushed
commits, so nothing released wrote proto tag 11.

- `cavewhere.proto`: delete the field and the `ElevationReference` enum; add
  `11` to FixStation's `reserved` numbers and `"elevationReference"` to its
  reserved names (follow the file's own convention at lines 104–105).
- Delete: the enum/Q_ENUM/accessors on `cwFixStation`, the static_asserts in
  the proto utils, the model role, the `setPickedPoint` parameter (and the
  QML call site in `FixStationPickTool.qml`), the LAZ-WKT vertical-datum
  detection **if** the pick path is its only consumer (grep first — if
  anything else reads it, leave that plumbing), `ElevationReferenceComboBox.qml`
  (and its `qt_add_qml_module` entry), every FixStationPage/FixStationPopup
  piece listed in §1, and `Theme.elevationReferenceFieldWidth`.
- **Keep `Theme.toolTipDelay`** — U4 uses it.
- If the QML namespace re-export at `cwFixStation.h:203` exists only for this
  enum, remove it too; if other enums ride it, keep it.
- State the convention once: add one sentence to the manual's fix-station /
  georeferencing page (find it under `docs/manual/`; follow
  `docs/manual/AUTHORING.md`; the page's word count must not grow — trade a
  sentence if needed): CaveWhere treats every elevation as a height above sea
  level. Nothing else in-app needs to say it.
- Tests: update/delete every test that names `elevationReference` — grep
  **all of `testcases/` and `test-qml/`**, not just the tags you plan to run
  (the C5–C6 round's lesson: stale expectations outside the narrow gates
  surfaced only at the full-suite gate). Expect hits in
  `test_cwFixStation*.cpp` (proto round trip, accessors, model role,
  `[cwFixStationPickedPoint]`), `tst_FixStationPage.qml`,
  `tst_MarkStationFixed.qml`, `tst_FixStationPickTool.qml`.
- Gate: `ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_1_for_macOS-Debug/cavewhere-test "[cwFixStation],[cwFixStationModel],[cwFixStationPickedPoint]"`,
  then (never concurrently) `cavewhere-qml-test --platform offscreen -input`
  on `tst_FixStationPage.qml`, `tst_MarkStationFixed.qml`,
  `tst_FixStationPickTool.qml`.

### U2 — Crosshair into the coordinate column, fit-based breakpoint

Two layout fixes, one commit — the second depends on the first's widths.

- **Pick button inside the coordinate column** (wide layout). Replace the
  coordinate `WideCell` usage with a cell that holds the `FixField` on the
  left and the `PickFromViewButton` anchored right, field's `anchors.right`
  to the button's left — so the "Coordinate" header visually owns the button.
  Grow `coordinateColumn.columnWidth` by the button's footprint
  (`Theme.iconSizeButton` + spacing) with a comment saying why, matching the
  file's existing width-comment style. Drop the free-floating
  `PickFromViewButton` from the row's trailing items. Keep its
  `objectName: "pickFromViewButton." + index` — tests click it by name. The
  narrow layout already places the button right after the coordinate field;
  leave it.
- **Fit-based `isNarrow`.** Replace the fixed comparison with:
  `readonly property real wideMinimumWidth` = `columnModelId.totalWidth` +
  the trailing chrome (two `Theme.iconSizeButton` warning icons +
  `Theme.tightSpacing` each) + `2 * Theme.pageMargin` + a vertical-scrollbar
  allowance; then
  `isNarrow: width < Math.max(Theme.breakpointPanelCollapse, wideMinimumWidth)`.
  The floor keeps genuinely narrow hosts on the narrow layout even if the
  column set shrinks further someday. `isNarrow` reads the page's own
  `width`, never the table's content width, so no binding loop.
- Tests (`tst_FixStationPage.qml`): a page width just below
  `wideMinimumWidth` shows the narrow delegate (probe an objectName only the
  narrow layout has, e.g. the inline "Elevation ref" label is gone after U1 —
  use the `·` separators' parent or add a stable objectName to the Flow);
  just above shows the wide delegate with the pick button visible **and
  fully inside the page** (`mapToItem` its right edge ≤ page width — this is
  the regression the whole issue is about). Update any existing tests that
  assumed the 600 breakpoint.
- Gate: `cavewhere-qml-test --platform offscreen -input test-qml/tst_FixStationPage.qml`,
  then `-input test-qml/tst_FixStationPickTool.qml` (it clicks the moved
  button).

### U3 — Region-labeled datum names

- `kGeographicDatums` gains a `const char* regionName` column:
  `"World (GPS)"` (WGS84), `"North America (USA)"` (NAD83(2011)),
  `"Canada"` (NAD83(CSRS)), `"Mexico"` (Mexico ITRF2008), `"Europe"`
  (ETRS89), `"Japan"` (JGD2011), `"Australia"` (GDA2020),
  `"New Zealand"` (NZGD2000).
- New invokable `cwCoordinateSystem::datumRegionName(datumCode)`; empty for
  an unknown code.
- `CSPicker.qml` datum combo: the **dropdown rows** show
  `"<region> · <name>"` (e.g. `North America (USA) · NAD83(2011)`); the
  **closed control keeps showing the short name** via `displayText`, so
  `Theme.csDatumFieldWidth` (130) still fits and the table gains no width.
  Trap: a `QC.ComboBox` popup defaults to the control's width — widen the
  popup (or the delegates' `implicitWidth`) so the long labels aren't
  elided; verify visually in the test via `truncated`/implicit widths, not
  by pixel counts.
- Tests: C++ — every table code returns the expected region name, unknown
  code returns empty. QML — the open popup's delegate text carries the
  region label; the closed combo still reads `NAD83(2011)`.
- Gate: `cavewhere-test "[cwCoordinateSystem]"`, then
  `cavewhere-qml-test --platform offscreen -input test-qml/tst_FixStationPage.qml`.

### U4 — Coordinate-gated, bounds-filtered datum combo

The datum reinterprets numbers that already exist, so it stays locked until
the row has numbers, and only offers datums plausible for where those
numbers land.

**C++:**

- `cwLocalProjection`: new
  `static QStringList plateFixedDatumsFor(double latitude, double longitude)`
  returning the datum codes of **every** `kPlateFixedRegions` box containing
  the point, deduped, in table order. (The existing single-answer
  `plateFixedDatumFor` stays first-match — it drives frame freezing and must
  not change behavior.) The US/Canada overlap makes the multi-answer form
  necessary: a border cave legitimately gets both.
- Row lat/lon: extract a small helper on the `domainCheck` pathway
  (`cwCoordinateTransform.cpp:535` already builds `cachedTransform(key,
  Wgs84)` and transforms the point) — e.g.
  `std::optional<cwGeoPoint> cwCoordinateTransform::toWgs84(cs, point)` —
  reusing the per-thread cache. **No PROJ call may run in a QML binding**;
  everything below happens in C++.
- `cwFixStationDiagnosticsModel`: new read-only
  `AvailableDatumsRole` (QStringList), next value in the `UserRole + 100`
  block, refreshed exactly when `DomainErrorRole` is. Contents:
  - Row not `Valid` (no CS, or unreadable coordinate): `[WGS84, current]`
    (current = `datumFor(inputCS)` when non-empty and ≠ WGS84).
  - Row `Valid`: `[WGS84] + plateFixedDatumsFor(lat, lon)`, then append the
    row's current datum if the bounds dropped it — the combo must always be
    able to display what the row stores (decision 5: no silent swaps; the
    warning, not the combo, reports an out-of-bounds pairing).

**QML:**

- `CSPicker.qml`: new `property list<string> availableDatums:
  CoordinateSystem.datumList()` (the default keeps any other host unchanged)
  and `property bool datumEnabled: true`. The datum combo's codes become
  `availableDatums`, intersected with `utmDatumList(zone, north)` in UTM
  mode. `enabled: rootId.datumEnabled` on the combo only — mode, zone, and
  hemisphere stay live.
- Disabled tooltip: **"Enter a coordinate to choose its datum"** with
  `Theme.toolTipDelay`. Trap: a disabled `QC.ComboBox` never reports
  `hovered` — put a `QQ.HoverHandler` on a wrapper `Item` around the combo
  (or on `rootId` scoped to the combo's geometry) and drive
  `QC.ToolTip.visible` from it only while disabled.
- **Enabled tooltip: explain the datum and recommend the plate-fixed one**
  (decision 7). The recommended datum is the first non-WGS84 entry of
  `availableDatums` (the bounds check already picked it for this row);
  derive it in CSPicker as a `readonly property string`. Text when a
  recommendation exists (substitute the region-labeled name):

  > The datum is the reference frame the numbers are measured in — the same
  > point lands a meter or two apart on different datums. Use the one your
  > source states; a GPS reading is WGS84. For a lasting fix, NAD83(2011)
  > holds better: it moves with the continent, so the coordinate stays put
  > while WGS84 drifts about 2 cm a year.

  When the list holds only WGS84 (out of every region, or row unreadable —
  though that case shows the disabled tooltip instead), drop the last
  sentence and keep the first two. One `QC.ToolTip` on the combo, text
  switched by whether a recommendation exists, `Theme.toolTipDelay`; the
  enabled `QC.ComboBox` reports `hovered` itself, so the HoverHandler wrapper
  is only for the disabled case.
- `CSComboBox.qml`: pass both properties through.
- `FixStationPage.qml` + `FixStationPopup.qml`: bind
  `availableDatums` to the new role and
  `datumEnabled` to "the row has a readable coordinate" —
  `coordinateText.trimmed() !== "" && coordinateError === ""` (reuse
  existing roles; add a bool role only if those two don't compose cleanly in
  the popup).
- **Remove `defaultDatum`** from `CSPicker.qml` and `CSComboBox.qml` and its
  `RootData.region.defaultFixDatum` bindings (`FixStationPage.qml:221`,
  `:574`, and the popup's) — decision 6: typed rows are born WGS84 and stay
  there until the user acts. `currentDatum`'s names-none fallback becomes
  plain `CoordinateSystem.wgs84()`. `defaultFixDatum()` itself stays (pick
  write + picker popup readout).

**Tests:**

- C++: `plateFixedDatumsFor` — a point only in the US box → `[EPSG:6318]`;
  a US/Canada-overlap point (e.g. 45.0, −90.0) → `[EPSG:6318, EPSG:4617]`;
  mid-ocean → `[]`. Diagnostics role: Valid Tennessee row →
  `[EPSG:4326, EPSG:6318]`; stored EPSG:6318 row whose coordinate is in
  Europe → list contains EPSG:4258 **and** EPSG:6318 (current appended), and
  `DomainErrorRole` is non-empty; unreadable row → `[EPSG:4326, current]`.
- QML: new row → combo disabled, shows WGS84, tooltip appears on hover;
  type a valid Tennessee lat/lon → combo enables with exactly the two
  labeled entries and the enabled tooltip names NAD83(2011) as the
  recommendation (an ocean coordinate's tooltip carries no recommendation); switch to NAD83(2011) → inputCS becomes EPSG:6318 and the
  displayed numbers don't move; edit the coordinate to Europe → the combo
  **still shows NAD83(2011)** (no silent swap) and the row's warning icon is
  visible; UTM mode intersects the list with the zone series; Custom still
  hides the combo. Mutation-verify the no-silent-swap test (temporarily make
  a coordinate edit rewrite inputCS; the test must fail) — and restore the
  tree completely afterward (`git status` + `git diff HEAD` clean).
- Gate: `cavewhere-test "[cwLocalProjection],[cwFixStationDiagnostics],[cwCoordinateSystem]"`,
  then `cavewhere-qml-test --platform offscreen -input test-qml/tst_FixStationPage.qml`,
  `-input test-qml/tst_MarkStationFixed.qml`,
  `-input test-qml/tst_FixStationPickTool.qml`.

### Final gate

Both full suites, **sequentially, never concurrently** (#638), with
`ASAN_OPTIONS=detect_container_overflow=0`; the C++ suite needs a 600000 ms
timeout. Classify any failure as new vs pre-existing before touching
anything. Known pre-existing: `tst_DeepLinkConfirmDialog`'s clone-error test
(network-dependent) and the possible #570 abort (if it hits, finish the
remaining QML files with individual `-input` runs).

## 3. Not in this phase

- Geoid/vertical transforms (a future plan may reintroduce an elevation
  reference **when it can convert**; it designs its own storage then).
- Variances written by the pick; popup restore after a pick; terrain-only
  picking (all carried from the parent plan).
- Datums beyond `kGeographicDatums`; NATRF2022.
- Any horizontal scrolling for the table — the fit-based breakpoint is the
  answer.
- Auto-selecting a datum from the typed coordinate. The list is filtered;
  the *choice* is always the user's.

## 4. Traps for implementers

- `plans/` is untracked on purpose — never `git add` it, never edit it.
- Tests must run with narrow scopes (Catch2 tags / single `-input` files);
  the two test binaries deadlock when run concurrently (#638).
- The tree is shared across pipeline stages: after any mutation-verify,
  restore worktree **and index** and prove it with `git status` +
  `git diff HEAD`.
- When deleting API (U1), grep **all** of `testcases/` and `test-qml/` for
  the identifiers, not just the files you expect — stale expectations
  outside the narrow gates are exactly what bit the previous round.
- Disabled Qt Quick Controls don't hover; ComboBox popups default to the
  control's width. Both handled explicitly in U3/U4 above.
- No PROJ calls in QML binding paths; per-row geodesy lives in the
  diagnostics model like `DomainErrorRole` already does.
- House rules per CLAUDE.md throughout: pointer handlers not MouseArea in
  new QML, Theme tokens, `QQ`/`QC` import aliases, k-constants, function
  pointer connects, `QStringLiteral`, American English, positive phrasing.
