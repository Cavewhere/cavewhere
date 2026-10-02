# Fix-station datum plan

> **Canonical version: `FIX_STATION_DATUM_PLAN.html`** — same content plus the
> interactive UX prototype of the pick flow. Edit the plan there; this file is
> the prose mirror.

Let a fix station be placed by clicking the LiDAR terrain — coordinate, datum,
and elevation reference written in one atomic edit — with the datum defaulted
from context everywhere a coordinate is typed by hand. Branch: `fixed_1`.

## 1. The problem

Every fix station entered through the UI is interpreted as WGS84, and the UI
says so out loud:

- `CSPicker.qml:89` labels the mode "Lat/Lon (WGS84)" and picking it commits
  `EPSG:4326` (`CSPicker.qml:58`).
- UTM mode commits WGS84 UTM zones only (`utmZoneToEpsg` → `EPSG:326xx/327xx`).
- `parseCS` (`cwCoordinateTransform.cpp`, anonymous namespace) recognizes
  exactly those spellings; any other geographic or projected CRS reads as
  `Custom`.

Since the plate-fixed datum work (commit `88296c36`), the *derived frame*
adopts NAD83(2011)/ETRS89/etc. (`kPlateFixedRegions`,
`cwLocalProjection.cpp:80`). The input side still assumes WGS84. Whether that
is wrong depends on the coordinate's source: a phone GPS genuinely reports
WGS84; a published benchmark or USGS datasheet is NAD83(2011) (~1.5 m off in
CONUS when read as WGS84); LiDAR tiles carry their full compound WKT and land
correctly — so a NAD83 fix read as WGS84 sits 1.5 m off *relative to the
tiles*, several LiDAR pixels.

### 1.1 The driving use case

1. The user fixes the entrance from their phone (WGS84). The frame derives,
   plate-fixed.
2. They add a LAZ tile (NAD83(2011) + NAVD88). It lands correctly.
3. The cave sits ~3–5 m off the terrain — phone GPS error, which dwarfs the
   datum shift. They want to drag the entrance onto the sinkhole they can see
   in the hillshade.

Today that adjustment is a transcription loop: pick the point with
`CoordinatePicker`, copy "lat, lon, elevation" out of
`CoordinatePickerPopup`, paste into the fix. The loop happens to be
datum-consistent today *because everything is WGS84* — and the moment fixes
default to the context datum, pasting WGS84-picked numbers into a
NAD83-defaulted fix becomes a silent 1.5 m error. Copying a datum label
alongside the numbers still leaves a human matching two combo boxes. The real
fix is removing the transcription: a pick interaction that writes the fix
directly.

## 2. Decisions

Made 2026-08-13:

1. **Datum default is context-derived**: the loaded LiDAR layer's datum if one
   is loaded, else the frame's plate-fixed datum, else WGS84. Always
   overridable.
2. **Vertical is metadata only this round**: record ellipsoidal-vs-MSL on the
   fix and display it; `z` still passes through every transform untouched.
   Geoid-grid transforms are a follow-up plan with its own grid-distribution
   problem. This fits the existing invariant at `cwLocalProjection.cpp:122`
   (`horizontalCrs`): a compound CRS's vertical half must never reach a
   transform.
3. **The datum lives in each fix's `inputCS`** (e.g. `EPSG:6318` instead of
   `EPSG:4326`). No project-level datum field.

Made 2026-08-14:

4. **The centerpiece is a fix-station pick interaction, entered from the fix
   surfaces** (Option B): a "Pick from 3D view" button on the Fix Station
   page and the Fixed Station popup, modeled on the Map page's "Add Layer"
   flow (`MapLayers.qml:34` — activate a view-parented tool, then
   `gotoPageByName(null, "View")`).
5. **The datum combo is demoted**: still shipped (typed coordinates from
   datasheets need it) but last in the sequence and visually secondary.
6. **`CoordinatePickerPopup` names the datum of the numbers it shows, and the
   copied text stays numbers-only** — no datum suffix in the clipboard.

## 3. Design

### 3.1 The datum table (`cwCoordinateTransform.cpp`)

One static table, same philosophy as `kPlateFixedRegions` ("depends only on
the shipped binary, never on which proj.db a machine has"):

```cpp
struct GeographicDatum {
    const char* geographicCode;   // "EPSG:4326", "EPSG:6318", ...
    const char* displayName;      // "WGS84", "NAD83(2011)", ...
    int utmNorthBase;             // EPSG code minus zone for north zones, 0 = none
    int utmSouthBase;             //   same for south zones
    int utmZoneMin, utmZoneMax;   // zones the datum's UTM series covers
};
```

Rows: WGS84 first, then the eight datums `kPlateFixedRegions` can produce.
Candidate UTM series (each code **must be verified against proj.db by a test**
before shipping — these are from memory):

| Datum | Geographic | UTM north | UTM south |
|---|---|---|---|
| WGS84 | EPSG:4326 | 32600+zone (1–60) | 32700+zone (1–60) |
| NAD83(2011) | EPSG:6318 | 6329+zone (1–19) | — |
| NAD83(CSRS) | EPSG:4617 | non-linear — omit UTM in v1 | — |
| Mexico ITRF2008 | EPSG:6365 | verify (11–16) | — |
| ETRS89 | EPSG:4258 | 25800+zone (28–38) | — |
| JGD2011 | EPSG:6668 | verify (51–55) | — |
| GDA2020 | EPSG:7844 | — | verify MGA2020 (46–59) |
| NZGD2000 | EPSG:4167 | — | verify (58–60) |

A datum whose series can't be verified, or that has no linear series
(NAD83(CSRS)), carries `utmNorthBase = 0` — Lat/Lon only.

`cwCoordinateSystem` (the QML singleton) grows: `parseCS` datum awareness
(pure string/integer matching, cheap enough for binding paths),
`datumFor(cs)`, `datumDisplayName(datumCode)`, `datumList()` /
`utmDatumList(zone, north)`, a three-arg `utmZoneToEpsg(zone, north,
datumCode)` (the two-arg form stays and means WGS84), and
`latLonCS(datumCode)`.

### 3.2 Datum extraction from an arbitrary CS

`static QString cwCoordinateTransform::geographicDatumFor(const QString& cs)`:
`proj_create` → compound? take the horizontal half → `proj_crs_get_geodetic_crs`
→ `proj_identify` against EPSG → match against the table → the table's
geographic code, else `""`. Cached per-thread like `nameFor` — it runs from
QML bindings.

### 3.3 The context default

`Q_INVOKABLE QString cwCavingRegion::defaultFixDatum() const` (or on
`cwGeoReference`, whichever reads cleaner — the region owns both inputs):

1. First **enabled** `lazLayers()` row whose `sourceCS` yields a table datum
   via `geographicDatumFor` → that datum.
2. Else the frame: `geoReference()->localCoordinateSystem()` through the same
   extraction.
3. Else WGS84.

With a notify driven by the layer model's rows/enabled changes and the frame's
change signal. (If the notify wiring gets heavy, v1 may resolve on demand —
the value only matters at commit and pick time.)

### 3.4 The pick interaction (the centerpiece)

**Entry.** A "Pick from 3D view" button (crosshair icon) on the coordinate row
of both surfaces — `FixStationPage.qml` (wide and narrow delegates) and
`FixStationPopup.qml`. Clicking it records the target and jumps to the view,
exactly like `MapLayers.qml:34`:

```qml
onClicked: {
    fixStationPickRequest.begin(caveId, stationId)   // shared handle
    RootData.pageSelectionModel.gotoPageByName(null, "View")
}
```

**The cross-page routing** (revised 2026-08-16 — the original
`cwFixStationPickRequest`-on-`cwRootData` design was rejected: interaction
state doesn't belong on the C++ hub, and each new cross-page interaction
would grow it again). Three pieces instead:

1. **A generic "arm a view tool" seam** — a QML singleton (the
   `ActiveTools`/`NoteToolMode` pattern) where view tools self-register by
   name; `arm(name)` records the pending tool and navigates to the View page,
   and the singleton activates the tool once it's current and registered. A
   second `arm()` replaces the first. Per-tool arguments stay out of the seam
   (no `property var`); each tool keeps its own typed state. The Map page's
   Add Layer button migrates onto the same `arm()` call, so the seam ships
   with two real users.
2. **`FixStationPick.qml`**, a small typed QML singleton holding the target
   (cave, station) and `active`, with `begin()` (records the target, arms the
   pick tool) and `cancel()`.
3. **The atomic write as a `Q_INVOKABLE` on `cwFixStationModel`** — a model
   operation, Catch2-testable without QML or RootData.

The popup case needs no popup restore — the write goes through the model, and
the popup simply isn't reopened (v1).

**The tool.** A view-parented QML tool (working name
`FixStationPickTool.qml`) instantiated where the view's other tools live,
sitting at `view.zOverlay` (clickable chrome must claim that layer — the
LeadView/LinePlotLabelView input-eating rule). Active when the request is.
It reuses the existing `CoordinatePicker` C++ item (`cwCoordinatePicker` —
ray-cast plus frame→geographic transforms) rather than growing a new picker.
While active: a `HelpQuoteBox`-style banner — *"Click the terrain to place
&lt;station&gt;. Esc cancels."* — and Esc/away-navigation cancels the request.

**The write.** On pick, one atomic edit through `cwFixStationModel`:

- **coordinate**: the picked point expressed in the context datum
  (`defaultFixDatum()`), lat/lon at the popup's existing 8-decimal precision
  plus elevation. Numerically this is the same PROJ pipeline inverted, so the
  round trip is essentially exact regardless of datum — the datum choice here
  is about consistency with the tiles, not error.
- **inputCS**: the context datum's geographic code.
- **elevationReference**: `MeanSeaLevel` when the picked geometry's source
  declares a vertical datum (the LAZ compound WKT), else `Unknown`. The
  metadata field gets set *truthfully by the machine*, not by asking.

Then `RootData.pageSelectionModel.back()` returns the user to where they
started, and the row shows its new values.

**Not written in v1**: variances. A LiDAR pick (~0.5 m) deserves a tighter
variance than a phone fix (~5 m) and the solve would use it — noted as a
follow-up, not baked in.

### 3.5 `CoordinatePickerPopup` (the copy path)

The popup reports its lat/lon in the **context default datum** and names that
datum in the section header (today the header hardcodes "WGS84" and the
numbers are always WGS84 — after this, a paste into a context-defaulted fix
is consistent by construction). The copied string remains numbers-only —
`lat, lon, elevation+unit` — with no datum suffix. The elevation-only
fallback section is unchanged.

### 3.6 Elevation reference metadata

- Proto: new field on `FixStation` (next free tag — 4, 5, 6, 10 are reserved;
  verify the coordinate field's tag before picking):
  `optional ElevationReference elevationReference` with
  `ELEVATION_REFERENCE_UNKNOWN = 0 / ELLIPSOID / MEAN_SEA_LEVEL`.
- `cwFixStation`: enum + accessors; `cwFixStationModel` role; rows default to
  `Unknown` (existing projects load as `Unknown` for free).
- UI: a small combo beside the elevation field in both surfaces —
  "Elevation ref: — / Mean sea level / Ellipsoid (GPS)". Display and storage
  only; the tooltip says plainly that no conversion happens yet.
- Survex export ignores it (`*fix` has no vertical datum concept).

### 3.7 The datum combo (demoted)

`CSPicker.qml` still gets it — typed coordinates from datasheets need a way
to say NAD83(2011) — but last in the sequence and visually secondary:

- Mode label "Lat/Lon (WGS84)" → "Lat/Lon".
- A compact datum combo in Lat/Lon and UTM modes, preset from a new
  `property string defaultDatum` the host binds to `defaultFixDatum()`
  (CSPicker stays region-ignorant). UTM's list holds only datums whose series
  covers the chosen zone/hemisphere; switching zone out of the series falls
  back to WGS84.
- `CSComboBox.qml`'s resolved label names the datum when it isn't WGS84.
- A `Custom` value hides the combo (unchanged behavior).

## 4. Not in this phase

- Geoid/vertical transforms, grid shipping or PROJ network — follow-up plan.
- Variances written by the pick — follow-up.
- Restoring `FixStationPopup` after a popup-initiated pick returns.
- NAD83(CSRS) UTM zone codes (non-linear series) — Lat/Lon only.
- NATRF2022 or any datum beyond the `kPlateFixedRegions` set.
- A project-level datum setting (decided against — per-fix `inputCS` only).
- Reinterpreting existing fixes: a stored `EPSG:4326` row stays WGS84; no
  migration, no prompt.

## 5. Commit sequence

Each commit builds green and runs its gate before the next starts.

1. **C1 — Datum table and parsing.** §3.1 + §3.2. Tests: every table code
   resolves in proj.db to the expected name (the verification gate for the
   from-memory codes); `latLonCS`/`utmZoneToEpsg` → `parseCS` round trips;
   `geographicDatumFor` on a LAZ-style compound WKT, a derived frame's WKT2,
   and garbage. Gate: `cavewhere-test "[cwCoordinateSystem]"` plus
   `[cwSurvexExporterRegion_OutputCS]` (the exporter consumes inputCS).
2. **C2 — Context default.** §3.3. Tests: each rung of the ladder and the
   enabled-flag filter. Gate: the new tag + `[cwLocalProjection]`.
3. **C3 — Elevation reference metadata.** §3.6 model half: proto field,
   `cwFixStation` accessors, model role, serialization round trip. (Before
   the interaction, which writes it.) Gate: `[cwFixStation]` tags.
4. **C4 — The pick interaction.** Split in two on 2026-08-16: **C4a** the
   generic arm-a-view-tool seam + Add Layer migrated onto it; **C4b** §3.4's
   pick singleton, view tool, the model-invokable atomic write,
   back-navigation. Tests: C++ for the commit path
   (pick point → coordinate/inputCS/elevationReference written, in a region
   with and without a LAZ layer); QML for the button → request → tool-active
   → simulated pick → row updated flow, and Esc cancel. Screenshot for the
   commit body.
5. **C5 — Picker popup datum.** §3.5. QML tests: header names the context
   datum; copied text carries no datum. Gate: the popup's existing QML tests.
6. **C6 — The datum combo (demoted).** §3.7 + the elevation-reference combo
   UI from §3.6. QML tests: NAD83(2011) default commits `EPSG:6318`;
   overriding to WGS84 commits `EPSG:4326`; UTM datum list filters by zone;
   Custom hides the combo. Screenshot.

Final gate: both suites, sequentially (never concurrently — #638), with
`ASAN_OPTIONS=detect_container_overflow=0`.

## 6. Risks

- **The UTM EPSG bases are from memory.** C1's proj.db test is the gate; a
  wrong base fails loudly there and the fix is one integer.
- **`parseCS` is in binding paths.** Keep it table-driven; the PROJ-backed
  `geographicDatumFor` is the only new proj.db toucher and it is cached.
- **Cross-page tool plumbing.** The Add Layer flow proves the pattern
  (`SelectExportAreaTool` + `gotoPageByName`), but the request handle adds a
  cancel path Add Layer lacks — navigating away, Esc, and a second `begin()`
  while active all need to leave exactly one consistent state. C4's QML tests
  cover all three.
- **Picking the cave instead of the terrain.** The ray-cast hits whatever
  geometry is nearest — including the line plot. v1 accepts the nearest hit
  (the user zooms to the entrance; the terrain dominates the click target);
  if it proves annoying, a "terrain-only" filter on the intersector is the
  follow-up.
- **Exports.** `EPSG:6318` etc. are ordinary authority codes — the Shareable
  policy (`944cf986`) passes them through untouched, and the WorkingFrame
  driver never sees inputCS. C1's gate runs the exporter tags to prove it.
- **Diagnostics.** `cwFixStationDiagnostics::domainCheck` resolves the fix's
  effective CS — new geographic codes must behave like 4326 there; C6 adds
  one test.
