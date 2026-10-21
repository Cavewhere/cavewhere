# Passage tube rendering — implementation plan for workflow agents

Companion to `plans/TUBE_RENDERING_RESEARCH.md` (the design; section numbers
below refer to it). This file is written for agents that start with no
context: every task says what to read, what to create, what "done" means,
and what to commit. Tasks run **in order**; each one leaves the build green
and ends in one commit on the `tube_render` branch.

The workflow that drives this is `.claude/workflows/tube-render.js`. Each
task goes through four agents: **implement → simplify → review (read-only)
→ fix and commit**.

## 0. Ground rules for every agent

- Work directly in the repository on branch `tube_render`. Never switch
  branches, never rebase, never push.
- Build directory: `build/Qt_6_11_2_for_macOS_Debug` (Debug has Address
  Sanitizer). If it is missing, run `ls build/` and use the newest directory
  that contains `CMakeCache.txt` and `cavewhere-test`.
- Build: `cmake --build build/Qt_6_11_2_for_macOS_Debug --target cavewhere-test --parallel`
  (add `cavewhere-qml-test` when QML changed, and `CaveWhere` for the app).
  Library sources (`cavewherelib/src/*.cpp|h`) and tests
  (`testcases/*.cpp`) are globbed with `CONFIGURE_DEPENDS`; new files need
  no CMake edits. Shaders must be registered (task T05).
- Test: `ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_2_for_macOS_Debug/cavewhere-test "[PassageTube]" -d yes 2>&1 | tee /Users/cave/.claude/jobs/ab1fa6ed/tmp/tube-test-<task>.log`.
  Every stage, including fix-and-commit, runs only this targeted
  `[PassageTube]` command; the full C++ and QML suites are run by hand
  before the branch is pushed, not by the workflow.
- Tag every new test `[PassageTube]` plus the class tag, e.g.
  `"[PassageTube][cwPassageTubeBuilder]"`.
- Tests and catalog cases are synthetic. Real projects (Blankenship
  Blowhole, or any `.cw`/`.cwproj` on this machine) only inspire a
  reproduction: rebuild the offending shape from synthetic stations in
  `cwPassageTubeCases` or inline `StationInput` surveys, and keep project
  paths, real station names and survey data out of source, tests and
  test data. Phake Cave is the one checked-in survey and stays so.
- Temp files go in `/Users/cave/.claude/jobs/ab1fa6ed/tmp/`, never `/tmp`.
- Follow `CLAUDE.md` exactly. The rules that bite most here: `k`-prefixed
  constants in an anonymous namespace instead of magic numbers; `.at(i)` /
  const refs on `QList`/`QVector` in worker code; no `shared_ptr<QList>`;
  `AsyncFuture::observe(...).context(this, ...)` in production; function
  pointer `connect`; `QStringLiteral`; comment sparingly; say things
  positively; American English.
- Do not touch `plans/`, `.claude/`, or unrelated files. Do not "improve"
  neighboring code outside the task.
- A task is **not done** until `cavewhere-test "[PassageTube]"` passes and
  the build has zero new warnings in the files you touched.

## 1. Agent contracts

### Implement (Opus, medium)
Read §0, the task section, and the research sections the task lists. Write
the code and the tests. Build and run the `[PassageTube]` tests until green.
Do not commit. Return: files created/modified, the exact test command and
its last 20 lines, anything the spec left ambiguous and what you chose.

### Simplify (Opus, medium)
Read `git diff HEAD` (the uncommitted work) and the task section. Make the
diff smaller and clearer without changing behavior: remove dead branches,
collapse duplicated helpers, replace comments with names, enforce the
CLAUDE.md conventions. Rebuild and rerun the `[PassageTube]` tests. Do not
commit. Return: what you simplified and the test output tail.

### Review (Fable, medium, read-only)
Read `git diff HEAD` and the task section. You may build and run tests; you
may not modify any file. Report findings as `file:line`, a one-sentence
defect, a concrete failure scenario, and a confidence 0–100. Check: the
"Done when" list of the task, memory and lifetime (worker-thread const
access, `AsyncFuture` contexts, `QObject` parenting), CLAUDE.md
conventions, test coverage of each "Done when" item, and whether the code
matches the research formulas it cites.

### Fix and commit (Opus, medium)
Apply every finding with confidence ≥ 80; apply lower ones when they are a
one-line change. Rebuild; run the targeted `[PassageTube]` tests. If anything fails, fix it; if it still fails, do **not** commit
and return the failure. Otherwise `git add` only the task's files and commit
with the task's subject line and a body listing what changed and the test
commands run; no attribution or Co-Authored-By line (project `CLAUDE.md`).
Return the commit SHA.

## 2. Tasks

Format: **Goal** · **Read** (research sections and existing code to copy
patterns from) · **Create / Modify** · **Spec** · **Done when** · **Commit
subject**.

This file holds the core tasks T01–T12. The follow-up tasks that came out
of looking at the built tubes (`T07` + a letter: T07b … T07ac) live in
`TUBE_RENDERING_FOLLOWUPS.md` (open ones, with a status table) and
`TUBE_RENDERING_DONE.md` (finished ones, verbatim). Read only the section
of the task you are given, whichever file it is in.

---

### T01 — Tube settings

**Goal.** Persisted settings for sample spacing and ring resolution.

**Read.** Research §4.3, §5 decision 5. Copy the pattern of
`cavewherelib/src/cwRenderingSettings.h/.cpp` (`kDefault*` constants,
QSettings keys, `isAtDefaults`, `resetToDefaults`), its registration in
`cavewherelib/src/cwSettings.h/.cpp`, its QML in
`cavewherelib/qml/RenderingSettingsItem.qml`, and its test
`testcases/test_cwRenderingSettings.cpp`.

**Create.** `cavewherelib/src/cwPassageTubeSettings.h/.cpp`,
`testcases/test_cwPassageTubeSettings.cpp`.
**Modify.** `cwSettings.h/.cpp` (expose `passageTubeSettings`),
`RenderingSettingsItem.qml` (a "Passage tubes" group with two spin boxes).

**Spec.**
- `class cwPassageTubeSettings : public QObject`, QML-uncreatable singleton
  like `cwRenderingSettings`.
- Properties with plain setters and `NOTIFY` (no bindable properties):
  `double sampleSpacingMeters` (default `kDefaultSampleSpacingMeters = 1.0`,
  clamp to `[0.1, 50.0]`), `int ringResolution` (default
  `kDefaultRingResolution = 16`, clamp to `[4, 64]`),
  `bool isAtDefaults`, `Q_INVOKABLE void resetToDefaults()`.
- Persist under QSettings keys `passageTube/sampleSpacingMeters` and
  `passageTube/ringResolution`, read at construction.
- QML: spin boxes bound to the settings, using `Theme` tokens and `QC.Label`.

**Done when.** Test covers defaults, clamping, persistence across two
instances, `isAtDefaults`, and signal emission (use `SignalSpy`). QML builds
and `cavewhere-qml-test` still passes.

**Commit subject.** `Add passage tube settings`

---

### T02 — Builder skeleton: mesh type, edges, chains

**Goal.** The pure builder entry point, its output type, and the survey →
chain step.

**Read.** Research §3.3.1 (chains), §3.4 (mesh type), §4.1 (builder
signature), §1.3 (positions and network). Existing code:
`cwCavingRegionData.h`, `cwTripData.h`, `cwSurveyChunkData.h`,
`cwStationPositionLookup.h` (positions and `cwSplayTipsByStation`),
`cwSurveyNetwork.h` (`neighbors()`), `cwStation.h` (`canonicalKey`),
`cwLinePlotTask.cpp:125-150` for how it walks chunks.

**Create.** `cavewherelib/src/cwPassageTubeMesh.h`,
`cavewherelib/src/cwPassageTubeBuilder.h/.cpp`,
`testcases/test_cwPassageTubeBuilder.cpp`.

**Spec.**
- `struct cwPassageTubeMesh` exactly as research §3.4, plus
  `struct VertexRange { int first; int count; }`,
  `struct Warning { enum Type { NoPassageData, ChainTorn, LrudOutOfRange, ScrapOpening, SourceChange }; Type type; QUuid tripId; QString station; QString message; }`,
  and `bool isEmpty() const`.
- `struct cwPassageTubeBuilder::Options { double sampleSpacingMeters = 1.0; int ringResolution = 16; double defaultRadiusMeters = 0.5; double blendLowDegrees = 45.0; double blendHighDegrees = 75.0; double subdivideBendDegrees = 60.0; }`.
- `static cwPassageTubeMesh build(const cwCavingRegionData& region, const QHash<QUuid, cwStationPositionLookup>& positionsByCave, const QHash<QUuid, cwSplayTipsByStation>& splaysByCave, const Options& options);`
  This task returns an empty mesh with the chains computed internally; the
  internal pipeline is a set of free functions in an anonymous namespace or
  a `detail` namespace so each step is testable:
  - `struct Leg { QUuid caveId; QUuid tripId; int chunkIndex; int shotIndex; QString from, to; QVector3D fromPosition, toPosition; cwStationLrud fromLrud, toLrud; }`
    where `cwStationLrud` holds four `std::optional<double>` in meters
    (convert with `cwUnits::convert(value, trip.calibrations.distanceUnit(), cwUnits::Meters)`;
    empty string → `nullopt`; see T03 for the zero rules, parse them here).
  - `extractLegs(region, positionsByCave)`: one `Leg` per shot whose both
    stations have a position; skip legs shorter than `kMinLegLength = 0.01`
    m (zero-length / equated stations) and record nothing for them.
  - `buildChains(legs)`: degree per station from the leg list; chains follow
    Loch's rule: start at degree-1 or degree ≥3 stations, continue through
    degree-2 stations, and at a junction continue along the incident leg
    with the largest dot product with the incoming direction (research
    §3.3.1), marking legs used. A chain is a `QVector<int>` of leg indices
    with a direction flag per leg (`reversed` when traversed to→from).
    Remaining unused legs (pure loops) start a chain at any station.
- Expose the step results for tests through a `cwPassageTubeBuilder::Debug`
  struct returned by `static Debug buildDebug(...)` with the same arguments
  (legs, chains). This is production API, so keep it a real, documented
  feature ("diagnostics"), small, and value-typed.

**Done when.** Tests build a `cwCavingRegionData` by hand (helper in the
test file) with: a straight 3-station line → one chain of two legs; a
T-junction → one through chain picked by straightest continuation and one
arm chain; a loop → every leg used exactly once; an equated (zero-length)
shot skipped. Every leg appears in exactly one chain.

**Commit subject.** `Add passage tube builder with chain extraction`

---

### T03 — Frames and LRUD rings

**Goal.** Blended cross-section frames along a chain and the LRUD-ellipse
ring fit, with the artifact guards.

**Read.** Research §3.1 (frame and blend), §3.2.1 (LRUD ellipse),
§3.2.4 (fallback), §3.2.5 (ring fit), §3.2.6 (units, L/R swap), §3.6 rows
"Twisted segment", "Pinched ring from `0 0 0 0` LRUD", §5 decision 1.
Code: `cwSurvexImporter.cpp:1438-1470` (L/R swap on reversed traversal).

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.**
- `struct Frame { QVector3D tangent, up, right; double inclinationWeight; }`.
  `frameAt(sample)` per research §3.1: tangent = normalized sum of incoming
  and outgoing leg directions at a station (leg direction at mid-shot
  samples), `incl = asin(|t.z|)`, `w = smoothstep(blendLow, blendHigh, incl)`,
  `up_h = n(Z − (Z·t)t)` (when `|up_h| < 1e-6` use the previous sample's
  up), `up_rmf` by the double-reflection method from the previous frame,
  `up = rotateAbout(t, up_h, w · angle)` where `angle` is the signed angle
  from `up_h` to `up_rmf` **unwrapped against the previous sample's angle**
  (pick `angle ± 2π` closest to the previous value), `right = t × up`.
  After a run of samples with `w > 0.5` ends, the residual twist between
  `up_rmf` and `up_h` is spread linearly over the next
  `kTwistDistributionSamples = 4` samples.
- `struct Ring { QVector3D center; QVector<double> radii; Frame frame; QVector<bool> measured; }`
  with `radii.size() == ringResolution`, bin `k` at `θ_k = 2πk/N` in
  `(right, up)`.
- LRUD samples: `ρ(θ) = |(a cosθ, b sinθ)|`, `a = cosθ ≥ 0 ? r : l`,
  `b = sinθ ≥ 0 ? u : d`; a bin whose `a` or `b` is missing gets no sample.
  Weight `1 − w`; in this task a weighted sample with weight below
  `kMinSampleWeight = 0.05` is dropped (so pit rings get no LRUD samples).
  Swap `l` and `r` when the chain traverses the leg reversed. Which LRUD:
  the station occurrence on the chunk that owns the leg.
- LRUD parsing rules: all four values zero or empty → absent; a single zero
  → `kMinRadiusMeters = 0.05`; any value above `kMaxRadiusMeters = 100.0`
  → dropped with a `LrudOutOfRange` warning.
- Ring fit: per bin max over samples; empty bins interpolated linearly
  (angularly) between the nearest filled bins; fewer than
  `kMinFilledBins = 4` filled → fallback: copy the radii of the nearest
  ring along the chain that had ≥ 4 filled bins, else `defaultRadiusMeters`
  everywhere with a `NoPassageData` warning.
- Pit dead end: the cap offset uses `u` (chain ending going up) or `d`
  (going down) of the end station; store it on the chain end as
  `capOffsetMeters` for T04.

**Done when.** Tests: horizontal leg with LRUD 1/2/3/4 → ring bins at 0°,
90°, 180°, 270° equal r, u, l, d; a vertical shot between two horizontal
legs → `up` continuous (`right_i · right_{i−1} > 0` for every consecutive
pair), the pit ring has `inclinationWeight ≈ 1` and no measured bins; a
chain that enters a pit on bearing 0° and leaves on 90° still satisfies
`right_i · right_{i−1} > 0` everywhere (twist test); reversed traversal
swaps l/r; `0 0 0 0` → fallback to the neighbor ring; a station with LRUD
`999 1 1 1` drops the 999 with a warning; a LRUD-free station between two
measured ones borrows the nearest ring.

**Commit subject.** `Add blended frames and LRUD rings to the tube builder`

---

### T04 — Mesh generation

**Goal.** Rings along chains to triangles: sampling, bends, junction arms,
caps, trip ranges.

**Read.** Research §3.3.2 (along a chain), §3.3.3 (junction arms),
§3.4 (mesh), §3.6 rows "Inner-bend fold", "Open tube ends when a trip is
hidden", "Degenerate quads", "Zig-zag facet pattern", "Junction stubs".

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.**
- Sample points along each chain: every station, plus mid-shot samples every
  `sampleSpacingMeters` (evenly spaced so the last interval is not shorter
  than half the spacing). Merge samples closer than
  `kMinRingSpacingMeters = 0.05`.
- Station rings lie in the bisector plane (the frame of T03 already does
  this). When the turn angle exceeds `subdivideBendDegrees`, inset both
  legs by `ε = min(kCornerInsetMeters = 0.5, ¼ leg length)` and add two
  rings framed by RMF with radii lerped from the two station rings.
- Fold-back fix: for ring `i+1` vertex `k`, if
  `(v − c_i) · t_i < 0`, move `v` to the intersection of its own ring ray
  with ring `i`'s plane (clamp to `c_i + kMinRadiusMeters · dir` if that
  would pass the center).
- Side quads between consecutive rings → two triangles split along the
  **shorter diagonal**, outward winding (counter-clockwise seen from
  outside).
- Caps: fan from the ring center at every chain end. At a dead end in the
  pit regime (`inclinationWeight > 0.5`) the fan center is offset along the
  tangent by `capOffsetMeters` from T03. At a junction arm (chain end at a
  degree ≥ 3 station that is interior to another chain): extend the arm by
  `kArmInsetFactor = 0.5 ×` the arm's last ring mean radius along its
  tangent, then close with a dome of `kDomeRings = 3` shrinking rings and a
  fan. Dome vertices belong to the arm's trip.
- Trip boundaries: when consecutive legs of a chain belong to different
  trips, emit a fan cap owned by each trip at the shared ring (two caps,
  opposite winding).
- `tripIndexRanges` / `tripUuids`: index ranges are contiguous per trip; sort
  emitted triangles by trip before writing indices so each trip is one
  range (a trip may own several chains; concatenate them).
- Warnings: `ChainTorn` when a chain had to break because a frame could
  not be built.

**Done when.** Tests: straight leg → `2 × N` vertices and `2 × N`
side triangles plus two fans; a 90° bend with narrow rings → every triangle
has outward winding (the signed volume of the closed mesh is positive) and
no vertex behind the previous ring plane; a 120° bend → subdivision rings
exist and the mesh is still closed; a T-junction → the arm's last ring
center is inset into the through tube and the dome is present; a chain
spanning two trips → two `tripIndexRanges` whose caps close each half
(each half's index range forms a closed surface: every edge shared by
exactly two triangles); a 0.3 m shot with 1 m spacing → exactly two rings.
Write an `isClosedSurface(indices)` helper in the test file for the
edge-count checks.

**Commit subject.** `Generate the passage tube mesh from rings`

---

### T05 — Render object and RHI object

**Goal.** Draw a `cwPassageTubeMesh` in the 3D scene, flat shaded,
two-sided, with a wireframe overlay for debugging.

**Read.** Research §1.5 (render pipeline), §4.2 (render object, shader),
§3.6 rows "Inside-out lighting", "Zig-zag facet pattern". Code to copy:
`cwRenderLinePlot.h/.cpp` and `cwRHILinePlot.h/.cpp` (the pair, `Data`
exchange, `setRangeVisible`, `cwLedgeredBytes`), `cwRenderPointCloud` /
`cwRHIPointCloud` (upload of an index + vertex buffer), `shaders/LinePlot.vert/.frag`
(`GlobalBlock` at binding 0), `cwRhiFrameRenderer` (`buildPipelineKey`),
`cavewherelib/CMakeLists.txt:418-460` (`qt_add_shaders`, the `"150,300es"`
block), `testcases/test_cwRenderPointCloud.cpp` and
`test_cwRenderLinePlotPickVisibility.cpp`.

**Create.** `cavewherelib/src/cwRenderPassageTube.h/.cpp`,
`cavewherelib/src/cwRHIPassageTube.h/.cpp`,
`cavewherelib/shaders/PassageTube.vert`, `cavewherelib/shaders/PassageTube.frag`,
`testcases/test_cwRenderPassageTube.cpp`.
**Modify.** `cavewherelib/CMakeLists.txt` (register the two shaders in the
`"150,300es"` `qt_add_shaders` block, since the fragment shader uses
`dFdx`/`dFdy`). The render object creates its RHI partner by overriding
`cwRenderObject::createRHIObject()` (see `cwRenderLinePlot.cpp:188`); no
registry edit is needed.

**Spec.**
- `cwRenderPassageTube : public cwRenderObject`: `setMesh(cwPassageTubeMesh)`
  (positions + indices + trip ranges), `clear()`,
  `setRangeVisible(int firstIndex, int count, bool visible)` mirroring the
  line plot's range visibility (compact the index buffer on the render
  thread the way `cwRHILinePlot` does, or keep a per-range draw list — pick
  the simpler one that matches the line plot), `bool wireframe` property
  (default false), and `cwLedgeredBytes` for the CPU copies.
- `cwRHIPassageTube : public cwRHIObject`: position-only vertex layout
  (`vec3`), `quint32` indices, opaque pass, depth test and write on,
  `QRhiGraphicsPipeline::CullMode::None`. Pipeline key via
  `QStringLiteral`. A second pipeline with `PolygonMode::Line` for the
  wireframe, created only when
  `rhi->isFeatureSupported(QRhi::NonFillPolygonMode)`; when unsupported the
  property is ignored.
- `PassageTube.vert`: `GlobalBlock` at binding 0 exactly as `LinePlot.vert`;
  outputs view-space position. `PassageTube.frag`: tube uniform block at
  binding 1 with `vec4 color` (default the Theme's passage color if one
  exists, otherwise `kDefaultTubeColor = (0.62, 0.60, 0.55, 1)`) and
  `float ambient = 0.35`; normal =
  `normalize(cross(dFdx(viewPos), dFdy(viewPos)))`, flipped when
  `!gl_FrontFacing`, lit by a view-space headlight `(0,0,1)`:
  `color.rgb * (ambient + (1 − ambient) · max(dot(n, L), 0))`.
- Do **not** register with `registerPickable` (research §1.5: it would
  steal station snapping).

**Done when.** `test_cwRenderPassageTube.cpp` mirrors
`test_cwRenderPointCloud.cpp`: starts empty; `setMesh` populates data and
the ledger; `clear` resets; `setRangeVisible` hides a range; the registry
sync test through `CwRhiSceneTestAccess` creates a `cwRHIPassageTube` for a
`cwRenderPassageTube`. The app builds (`--target CaveWhere`) and shaders
compile (`qsb` runs in the build).

**Commit subject.** `Add the passage tube render object and shader`

---

### T06 — Manager, root data, keyword visibility, View menu

**Goal.** Build the tube on a worker after every line-plot solve, show it in
the scene, filter it by keywords like the line plot.

**Read.** Research §4.1 (manager), §4.3 (visibility), §5 decisions 3 and 6.
Code: `cwScrapManager.h/.cpp` (`cwUpdatableBase`, `AsyncFuture::Restarter`,
`doUpdateState`, teardown in the destructor, how it reads
`cwCavingRegionData` snapshots), `cwLinePlotManager.cpp:280-400`
(`reconcileTripKeywordItems`, keyword items `Type="Splays"`),
`cwVisibilityProxy.h` (`applyVisible`), `cwRootData.cpp:195-215`
(`UpdateCoordinator->add(...)`), `cwRootData.h` (manager properties),
`cavewherelib/qml/FileMenu.qml:150-170` ("Scraps Visible"),
`cwRenderingSettings` (how settings reach C++ consumers),
`testcases/test_cwLinePlotManager.cpp`.

**Create.** `cavewherelib/src/cwPassageTubeManager.h/.cpp`,
`testcases/test_cwPassageTubeManager.cpp`.
**Modify.** `cwRootData.h/.cpp` (own the manager, expose
`passageTubeManager`, `UpdateCoordinator->add(PassageTubeManager, {LinePlotManager})`),
`cwRegionSceneManager.cpp` (line 28 creates `m_linePlot`; create and add the
tube render object next to it), `FileMenu.qml` ("Passage
Tubes" check item next to "Scraps Visible"), `cwLinePlotManager` (expose
the per-cave `cwSplayTipsByStation` it already computes:
`cwCave::setSplayTips/splayTips()` populated in `updateLinePlot`; research
§4.1), `cwCave.h/.cpp`.

**Spec.**
- `cwPassageTubeManager : public QObject, public cwUpdatableBase`. Inputs:
  `cwCavingRegion*`, the `cwRenderPassageTube*`, `cwPassageTubeSettings*`
  (T01). Trigger: `cwLinePlotManager::stationPositionInCavesChanged(QList<cwCave*>)`
  (`cwLinePlotManager.h:129`) and both settings' change signals, debounced
  by `kRebuildDebounceMs = 150`.
- Run: snapshot `region->data()` (value), the per-cave position lookups and
  splay tips (values), `Options` from settings; run
  `cwPassageTubeBuilder::build` through `AsyncFuture::Restarter` on
  `cwConcurrent::run`; deliver with `observe(...).context(this, ...)`;
  `setMesh` on the render object; emit `meshChanged()`; store
  `warnings()` for the UI.
- Keyword items: per trip, `Type="Passage"` items created the way the line
  plot creates `"Splays"`, with a `cwVisibilityProxy` subclass whose
  `applyVisible` calls `setRangeVisible` for that trip's index range.
  Reconcile after every build (ranges change).
- A `bool visible` on the manager bound to the View menu item; persisted
  under `view/passageTubesVisible` (default true).

**Done when.** `test_cwPassageTubeManager.cpp`: load a small existing test
project (`fileToProject(testcasesDatasetPath("network.cw"))` as in
`test_cwLinePlotManager.cpp:62`), wait for the line
plot (`waitToFinish` helpers are test-only and allowed), wait for the tube
manager (`waitToFinish`), assert the render object has a non-empty mesh
and one keyword item per trip; hiding a trip's keyword item hides its
range; changing `ringResolution` rebuilds without a new line-plot run
(observe the line-plot manager stays idle with `SignalSpy`); destroying
the manager mid-run is safe (ASan clean). App builds and runs:
`./build/Qt_6_11_2_for_macOS_Debug/CaveWhere` opens.

**Commit subject.** `Add the passage tube manager with keyword visibility`

---

### T07 — Phase 0 polish

**Goal.** Make phase 0 presentable: settings UI wired, warnings visible,
memory ledger category, a manual screenshot.

**Read.** Research §4.3, §4.4, §6 Phase 0 and Phase 3. Code:
`cwRenderMemoryLedger.h` and `cwLedgeredBytes` categories, the
manual-screenshot QML test `test-qml/tst_ManualScreenshots.qml`, the
render-stats HUD for where warnings could go.

**Modify.** Settings UI (T01's spin boxes: labels, tooltips, reset),
`cwPassageTubeManager` (`Q_PROPERTY(QStringList warnings)`), the
screenshot test, ledger category "Passage tubes".

**Done when.** `cavewhere-qml-test` passes; a screenshot of a tube-rendered
project is produced by the manual screenshot test; the memory HUD lists the
tube.

**Commit subject.** `Polish passage tube settings, warnings and screenshot`

---

### T08 — Splay samples

**Goal.** Phase 1: splay tips shape the rings.

**Read.** Research §3.2.2 (projected tips), §3.2.5 (binning, trimmed max,
concavity clamp), §6 Phase 1, §3.6 "Spike from one splay".

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.** For each sample, tips of the station (and of both end stations
for mid-shot samples): `v = T − S`, `along = v·t`, `q = v − along·t`; keep
when `|along| ≤ max(½ · min(adjacent leg lengths), kMinAlongToleranceMeters = 0.5)`
and `|q| > kSplayParallelFactor (0.5) · |along|`; bin by
`atan2(q·up, q·right)`, `ρ = |q|`, weight 1. Per bin: max, or the trimmed
max (drop the single largest) when ≥ 3 splays share the bin. Concavity
clamp: a bin below `kMinConcavity = 0.4 ×` the mean of its two neighbors is
raised to that value. Splay samples take priority: LRUD samples are added
only when fewer than `kMinFilledBins` bins are filled by splays.

**Done when.** Tests: a station with 8 symmetric splays at radius 2 → ring
radii ≈ 2 in those bins, LRUD ignored; a splay shot down the passage is
dropped; one wild splay among three in a bin is trimmed; a single short
splay among long ones is clamped up.

**Commit subject.** `Shape passage tube rings with splays`

---

### T09 — Plan scrap ray casts

**Goal.** Phase 2 part 1: plan scraps set the tube width through page-space
ray casts, with the opening clamp and the closing-edge guard.

**Read.** Research §3.2.3 (page-space ray casts), §3.2.4, §3.5.2, §3.5.3
option A, §3.6 rows "Pinch at a scrap's end", "Step where sources change".
Code: `cwTriangulateTask.cpp:1058-1069` (the `toWorldCoords` chain:
pixels → paper meters → cave meters), `cwNoteTransformationData.h`
(`matrix(scale, northUp)`), `cwNoteData.h` (`imageResolution`),
`cwScrapData.h`, `cwNoteStation`, `cwPlanScrapViewMatrix`.

**Modify.** `cwPassageTubeBuilder.h/.cpp` (the builder already receives
`cwCavingRegionData`, which contains notes and scraps),
`test_cwPassageTubeBuilder.cpp`.

**Spec.**
- For each sample whose station(s) are note stations of a plan scrap:
  `S_page` = page position of the station (or lerp of the two for mid-shot
  samples); skip the scrap when `containsPoint(S_page, Qt::OddEvenFill)` is
  false.
- For each ring direction `d_k`: `d_view = (V · d_k).xy` with `V` identity
  for plan; `c = |d_view|`; skip when `c < kMinPageComponent = 0.5`;
  `d_page = normalize(toCave⁻¹ rotation · d_view)`; nearest positive
  intersection of the ray `(S_page, d_page)` with the outline edges;
  **reject hits on edges whose direction satisfies
  `|ê · t_page| < cos(kMaxWallEdgeAngleDegrees = 60)`** (edges that close the
  scrap across the passage); `ρ_k = distance · scaleToCave / c`, weight
  `c`.
- Opening clamp (research §3.5.3 A): for the two page side rays of each
  sample, compare the hit distance with the median of the same ray over
  the `±kOpeningWindow = 3` neighboring samples along the chain; above
  `kOpeningRatio = 1.8 ×` the median → replace with the interpolation
  between the nearest unflagged neighbors, emit a `ScrapOpening` warning,
  and let the bins that share the ray follow.
- Source cross-fade: when the dominant source (scrap / splay / LRUD) of a
  ring differs from the previous ring's, blend radii over
  `kSourceBlendSamples = 2` rings and emit one `SourceChange` warning.
- Scrap samples take priority over splays and LRUD in the same way splays
  take priority over LRUD (T08).

**Done when.** Tests with a hand-built note + plan scrap whose outline is a
rectangle around a two-station shot: ring radii at 0° and 180° equal the
drawn half-widths in cave meters (scale applied); a rectangle with a
rectangular side bay → the side bins at samples facing the bay are clamped
to the wall and a `ScrapOpening` warning is emitted; a rectangle whose end
edge is slanted produces no ring narrower than the wall width near that
end; a scrap covering only the first half of a chain produces a
`SourceChange` warning and monotone radii across the transition.

**Commit subject.** `Add plan scrap ray casts to the passage tube`

---

### T10 — Profile scrap ray casts

**Goal.** Phase 2 part 2: running and projected profiles set the height.

**Read.** Research §3.2.3 (profile cases and the projected-profile caveat),
§1.4. Code: `cwRunningProfileScrapViewMatrix.h` (`Data(from, to)`),
`cwProjectedProfileScrapViewMatrix`, `cwTriangulateTask.cpp:975-1000`
(`calculateViewMatrix`).

**Modify.** `cwPassageTubeBuilder.h/.cpp`, `test_cwPassageTubeBuilder.cpp`.

**Spec.** Same ray cast as T09 with `V` = the running-profile matrix of the
sample's shot (`cwRunningProfileScrapViewMatrix::Data(from, to).matrix()`)
or the projected-profile matrix of the scrap. Up/down page rays project
fully; the opening clamp and closing-edge guard apply to the ±up rays.

**Done when.** Tests: a running-profile rectangle scrap around a shot → ring
radii at 90° and 270° equal the drawn half-heights; a plan scrap and a
profile scrap on the same shot fill different bins and both survive; a
projected profile on a shot running along the profile azimuth gives the same
answer as the running profile.

**Commit subject.** `Add profile scrap ray casts to the passage tube`

---

### T11 — Rebuild on scrap edits

**Goal.** The tube follows outline drags without a cavern run.

**Read.** Research §4.1. Code: `cwScrapManager.cpp:600-660` (the scrap
signals it connects to and how it debounces).

**Modify.** `cwPassageTubeManager.h/.cpp`, `test_cwPassageTubeManager.cpp`.

**Done when.** Test: move an outline point of a scrap in a loaded project →
the tube manager runs again and the line-plot manager does not.

**Commit subject.** `Rebuild passage tubes on scrap edits`

---

### T12 — Lobe walk (synthetic chains)

**Goal.** Phase 2.5: walk each `ScrapOpening` run into a synthetic chain
joined at a synthetic junction, behind a setting.

**Read.** Research §3.5.3 option B in full (detect, walk, place in world,
join, splays, tests), §5 decision 7. Code: T09's clamp (the warnings are
the detector), `cwTriangulateTask::morphPoint` and
`buildStationsWithInterpolatedShots` (`cwTriangulateTask.h:75`) for page →
world placement.

**Modify.** `cwPassageTubeSettings` (`bool followScrapLobes`, default
false, persisted), `cwPassageTubeBuilder.h/.cpp`, `cwPassageTubeMesh.h`
(`bool synthetic` on `VertexRange` or a separate `syntheticIndexRanges`),
shader uniform `vec4 syntheticColor` used for synthetic ranges (draw them
as a second range set with a different color),
`test_cwPassageTubeBuilder.cpp`.

**Spec.** Constants: `kMinMouthWidthMeters = max(2 × spacing, 0.75)`,
`kMinLobeDepthRatio = 1.0`, `kMaxTurnPerStepDegrees = 30`,
`kRejoinDistanceMeters = spacing`, `kMaxLobeWidthRatio = 3.0`,
`kMaxLobeSteps = 200`, nesting limit 2. Walk exactly as research §3.5.3 B.
Height of a plan lobe: the mouth sample's up/down radii held constant.
Join: the synthetic chain is a junction arm (T04 inset + dome).

**Done when.** Tests: rectangle with a rectangular side bay → one synthetic
chain with the expected ring count and inherited height; bay shallower
than its mouth → clamp only; U-shaped bypass → the chain ends at the second
mouth; self-intersecting outline → no crash, warnings only; the setting off
→ no synthetic chains.

**Commit subject.** `Walk scrap lobes into synthetic passage tube chains`

---

## 3. Order and dependencies

```
T01 ─┐
T02 → T03 → T04 → T05 → T06 → T07 → [follow-ups, see TUBE_RENDERING_FOLLOWUPS.md] → T08 → T09 → T10 → T11 → T12
```

T01 only feeds T06; everything else is strictly sequential. T01–T07 have
landed. The follow-ups run between T07 and T08 in the order given by the
status table in `TUBE_RENDERING_FOLLOWUPS.md`; their history (why each was
added, what each left behind) is in that table, in `TUBE_RENDERING_DONE.md`
and in the commit bodies. T08 builds on the ring fit and runs its rings
through the same junction pass, so the junction sweep tests guard it.

The workflow runs the tasks in the listed order and stops at the first task
whose fix stage cannot get the `[PassageTube]` tests green, so a human can
look before anything else is built on top.
