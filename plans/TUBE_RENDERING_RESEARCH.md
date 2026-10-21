# Passage Tube Rendering Research

Background research for rendering cave passages as a continuous 3D tube that
follows the survey centerline. The goal, as stated:

> Create a continuous tube rendering that follows centerline data. It should
> generate cross-sections, then cross-sections are joined at passage junctions.
> Investigate whether the cross-sections should be z-aligned, or have pitch
> (a vertical pit would be an x-y plane). The cross-section is modified by
> scrap geometry to get the extent: plan scraps add to the width, profile
> scraps define the height. Splays should also change the cross-section
> geometry. If no scraps or splays exist, use LRUD data.

This document records what CaveWhere already has, how other cave tools and the
graphics literature solve each sub-problem, and a recommended design. A phased
implementation plan is at the end. Line numbers are from the `tube_render`
branch at the time of writing (October 2026).

## 0. Summary

1. **CaveWhere has every input already, in the worker that solves the line
   plot.** LRUD lives per station occurrence on `cwStation`, splays per station
   occurrence as `cwShotMeasurement`, solved positions come back from cavern,
   and the region snapshot the worker receives (`cwCavingRegionData`) carries
   every scrap's outline, note stations, note transform and view matrix. The
   missing pieces are the tube builder itself, a lit mesh render object, and a
   place to keep splay tips (today they are dropped on the main thread).
2. **Cross-section frame: hybrid, with a smooth blend.** Walking passage uses
   a section plane perpendicular to the (bisected) shot direction with world-up
   as the in-plane "up" reference, which is how LRUD, plan scraps and profile
   scraps were all measured. As the inclination steepens, the reference blends
   into a rotation-minimizing (parallel-transport) frame, so a vertical pit
   gets a horizontal (x-y) ring with no singularity. Every shipping tool uses a
   hard threshold here (Therion 67.5°, Loch 60°, Compass 45–75°, CaveView 76°);
   the blend is the one real improvement on them.
3. **Treat every source as radial samples in the section plane**, then fit one
   star-shaped ring per cross-section: LRUD gives 4 samples (a quadrant
   ellipse), splays give scattered samples, a plan scrap gives samples in
   every ring direction that has a horizontal component (ray cast in page
   space), a profile scrap gives samples in every direction with a vertical
   component. This single rule produces the requested "plan adds width,
   profile defines height" behavior, gives a full footprint ring in a pit
   from the plan scrap alone, and makes the join and pit logic independent of
   where the shape came from. Survex's own roadmap is the same idea ("treat
   each LRUD cross section as 4 splays").
4. **Join along chains, overlap at junctions first, hull later.** Chain the
   network into traverses the way Loch does (straightest continuation through
   junctions), miter adjacent rings in the bisecting plane, subdivide sharp
   bends, and end side arms at the junction with a dome cap pushed into the
   through tube. That is what every shipping tool does and it looks continuous
   when drawn opaque. A manifold junction patch (convex hull of the incident
   rings, B-Mesh / Blender Skin) or a watertight SDF + marching cubes mode is
   a later phase and only matters for translucency and cutaways.
5. **Architecture:** a pure `cwPassageTubeBuilder` (testable without a GPU),
   driven by a `cwPassageTubeManager` (`cwUpdatable`, depends on
   `LinePlotManager`, also listens to scrap edits), feeding a new
   `cwRenderPassageTube` / `cwRHIPassageTube` pair with a new
   `PassageTube.vert/.frag` lit shader. Per-trip visibility through a
   `Type="Passage"` keyword item, mirroring `"Splays"`.

## 1. What CaveWhere has today

### 1.1 LRUD

- Stored **per station occurrence** on `cwStation`, as four
  `cwDistanceReading`s (`cavewherelib/src/cwStation.h:41-51`, storage at
  `:86-89`). A station name that appears in two chunks is two `cwStation`
  values and can carry two different LRUD sets. Identity (`operator==`,
  `qHash`) compares the canonical name only (`cwStation.h:97-112`).
- Values are raw strings in the trip's distance unit
  (`cwTripCalibration::distanceUnit()`, `cwTripCalibration.h:200`); convert
  with `cwReading::toDouble()` and `cwUnits::convert` (`cwUnits.h:76`).
- Left/right follow the chunk's station order. The Survex importer swaps L
  and R when a `*data passage` block runs against the chunk direction
  (`cwSurvexImporter.cpp:1438-1470`).
- All four importers fill LRUD: Compass (`cwCompassImporter.cpp:711-726`),
  Survex (`cwSurvexImporter.cpp:739-742`, `:1403`), Walls
  (`cwWallsImporter.cpp:400-424`), CSV (`cwCSVImporterTask.cpp:101-226`).
- **LRUD already goes to cavern.** `cwSurvexExporter::writeLRUDData`
  (`cwSurvexExporter.cpp:280-309`) writes one
  `*data passage station left right up down ignoreall` block per chunk, and
  the line-plot solve uses this exporter (`cwLinePlotTask.cpp:295-309`). The
  resulting `.3d` therefore contains `img_XSECT` / `img_XSECT_END` records
  (`survex/src/img.h:63-64`, `l, r, u, d` at `:145`) that
  `cwSurvex3DFileReader::readNetworkAndLookup` currently drops — it handles
  only `img_LABEL`, `img_MOVE`, `img_LINE` (`cwSurvex3DFileReader.cpp:81-139`).
  Reading them is optional for CaveWhere's own data (we have the same
  information in the snapshot) but would be the way to get tubes for
  externally included `.svx` files later.

### 1.2 Splays

- `QList<cwShotMeasurement> m_splays` per station occurrence
  (`cwStation.h:53-59`, `:91`; `cwShotMeasurement.h:8-49`). Chunk API at
  `cwSurveyChunk.h:110-122`, signal `stationSplaysChanged(int)` at `:151`.
- Exported to cavern as `*flags splay` legs to the anonymous station `..`
  (`cwSurvexExporter.cpp:193-276`). Solved tips come back from the `.3d`
  (`cwSurvex3DFileReader.cpp:133-139`) as
  `cwSplayTipsByStation = QHash<QString, QList<QVector3D>>`
  (`cwStationPositionLookup.h:29`), split per cave into
  `LinePlotCaveData::setSplayTips` (`cwLinePlotTask.cpp:511-540`).
- **Nothing on the main thread reads `splayTips()`**; they survive only as
  baked line-plot vertices. Tips are merged per cave-level station name, so
  per-occurrence ownership is lost (fine for tubes, which need positions).

### 1.3 Positions, network, topology

- Solver: in-process cavern via `cwCavernRunner` (`cwCavernRunner.h:40-54`).
  Worker steps in `cwLinePlotTask.cpp:151-242`; it runs under
  `cwConcurrent::run` with a `QPromise` and cancels between phases
  (`:722-739`). Coordinates are meters, local projection, Z up.
- `cwStationPositionLookup` is `QMap<QString, QVector3D>` by canonical name
  (`cwStationPositionLookup.h:35-49`); one per `cwCave` (`cwCave.h:128-135`).
- `cwSurveyNetwork` (`cwSurveyNetwork.h:18-69`) has `neighbors(name)`,
  `stations()`, `position()`. A junction is `neighbors(name).size() > 2`, a
  dead end is `== 1`. It has **no edge identity** (which chunk/shot a leg came
  from), so the tube builder must build its own edge list from
  `cwSurveyChunkData` (`{ QUuid id; QList<cwStation> stations; QList<cwShot> shots; }`,
  `cwSurveyChunkData.h:11-15`). That is also where per-leg LRUD attribution
  comes from: the from- and to-station occurrences of that chunk.
- `cwLinePlotGeometry::Result` (`cwLinePlotGeometry.h:81-87`) exposes per-trip
  vertex ranges (`tripVertexRanges`, `tripSplayVertexRanges`, `tripUuids`);
  the tube mesh should expose the same kind of per-trip index ranges.

### 1.4 Scraps

- The worker snapshot already includes them: `cwTripData::noteModel` →
  `cwNoteData { rotate; cwImageResolution::Data imageResolution; QList<cwScrapData> scraps; cwImage image; }`
  → `cwScrapData { QPolygonF outlinePoints; QList<cwNoteStation> stations; QList<cwLead> leads; cwNoteTransformationData noteTransformation; unique_ptr<cwAbstractScrapViewMatrix::Data> viewMatrix; }`
  (`cwTripData.h:17-29`, `cwNoteData.h`, `cwScrapData.h:66-72`).
- Outline points and note-station positions are in **normalized note-image
  coordinates** in [0,1] (`cwScrap.h:322-323`, `cwNoteStationBase.h:13-46`).
  Normalized → pixels via the image size, pixels → paper meters via
  `cwImageResolution` (dots per meter), paper meters → cave meters via
  `cwNoteTranformation::matrix(scale, northUp) = rotate(northUp, z) * scale(1/scale)`
  (`cwNoteTranformation.cpp:167-178`). `cwTriangulateTask` composes exactly
  this as `toWorldCoords = toNoteMatrix(noteTransform) * metersPerDot * cropPixels * toLocal`.
- View matrices (`cwAbstractScrapViewMatrix.h:30-37`, `Type` at `:23-26`):
  Plan = identity (`cwPlanScrapViewMatrix.cpp:14-17`); ProjectedProfile =
  `fromAxisAndAngle(1,0,0,-90) * fromAxisAndAngle(0,0,1, absoluteAzimuth())`
  (`cwProjectedProfileScrapViewMatrix.cpp:85-100`), one vertical plane for the
  whole scrap; RunningProfile = `Data(from, to)` per shot
  (`cwRunningProfileScrapViewMatrix.h:13-36`), a vertical plane that follows
  each shot.
- Morphing (`cwTriangulateTask::morphPoints`, `cwTriangulateTask.cpp:790-1104`)
  anchors each note station to `viewMatrix.map(stationWorld)`, offsets the
  note→world mapping of a point by each nearby station, and blends with
  inverse-distance-squared weights in note space (running profile: the two
  bracketing stations). Between two stations of one shot the morph is close
  to linear, which is what §3.2 relies on.
- The CPU copy of the triangulated scrap mesh is **released after upload**
  (`cwRenderTexturedItems::Item::storeGeometry` defaults to false,
  `cwRenderTexturedItems.h:36`; `updateGeometry` clears it, `.cpp:286-290`).
  The tube builder should therefore work from the outline polygon, which is
  simpler and more precise anyway.
- There is no cross-section, wall, or outline→3D helper anywhere in `src`.

### 1.5 Rendering

- Render objects come in pairs: `cwRenderObject` (GUI thread, `cwRenderObject.h:76` factory
  `createRHIObject()`) and `cwRHIObject` (render thread, interface at
  `cwRHIObject.h:235-331`: `initialize`, `synchronize`, `updateResources`,
  `gather`, optional `worldBounds`). `cwScene` queues Add/Update/Delete by
  `cwRenderObjectId` (`cwScene.h:151`), `cwRhiScene::synchroize`
  (`cwRhiScene.cpp:50-128`) drains the queue at the sync barrier, and
  `cwRhiFrameRenderer::renderLiveFrame` (`cwRhiFrameRenderer.cpp:185-311`)
  runs `initialize` / `updateResources` / `gatherScene` / `drawScene`. Draw
  order is registration order (`cwRegionSceneManager.cpp:21-47`).
- The closest templates: `cwRenderPointCloud`/`cwRHIPointCloud` (one
  `cwGeometry`, immutable buffers, `worldBounds`, geometry/render-state split
  so a color tweak skips a re-upload, `cwRHIPointCloud.cpp:62-71`, `:101-166`,
  `:298-310`) and `cwRenderLinePlot`/`cwRHILinePlot` (survey-driven geometry,
  per-vertex visibility mask, `cwRenderLinePlot.cpp:116-164`,
  `cwRHILinePlot.cpp:141-176`). Pipelines are acquired in `gather()` with a
  `buildPipelineKey(renderPassDescriptor, sampleCount)` using
  `QStringLiteral` shader paths (`cwRHILinePlot.cpp:218-348`); the camera is
  `GlobalUniform { mat4 viewProjection; mat4 view; mat4 projection; float dpr; pad; vec2 viewportSize; }`
  (`cwRhiFrameRenderer.h:121-128`) bound with `globalCameraBinding = 0`.
- **No shader in the codebase does normal-based lighting.** `scrap.*`,
  `unlit.*` (GLTF, LiDAR) and `LinePlot.*` are texture-only or flat color.
  `cwGeometry` already has a `Normal` semantic (`cwGeometry.h:74`). Shaders
  must be listed in `qt_add_shaders` (`cavewherelib/CMakeLists.txt:418-450`);
  the `"150,300es"` block is required for `flat`, `uint`, or `dFdx`.
- `cwRenderTexturedItems` is a poor fit: fixed Position+TexCoord0 layout
  (`cwRenderTexturedItems.cpp:101-107`), texture always bound, no normals.
- EDL applies only to point clouds; an Opaque-pass tube occludes the cloud
  correctly and gets its silhouette for free (`cwRhiFrameRenderer.cpp:793-825`).
  Transparent pass has no depth sorting across or within objects
  (`cwRhiFrameRenderer.cpp:652-658`); `cwRHIGridPlane` is the transparency
  pattern (`gather` `:123-157`).
- Picking: `registerPickable(subId, cwGeometry, modelMatrix)`
  (`cwRenderObject.cpp:119-162`). A pickable tube would win front-most hits
  and break station snapping (`cwScenePick.cpp:45` only snaps on a
  `cwRenderLinePlot` hit), hide leads behind `cwPickQuery::Solid`
  (`cwLeadView.cpp:618`), and change reset-view framing and the turntable
  pivot. Recommendation: do not register, at least initially.
- Visibility: `cwSceneVisibility` store (object, sub-item, per-vertex mask).
  Per-trip line-plot filtering runs
  `cwLinePlotManager::reconcileTripKeywordItems` (`cwLinePlotManager.cpp:280-376`)
  → `cwLinePlotTripVisibility` → `cwRenderLinePlot::setRangeVisible`. Keyword
  `Type` values today: `"Line Plot"`, `"Splays"`, scrap types, `"Lead"`,
  `"LiDAR"`. `cwRenderingSettings` has no content toggles (only sample count,
  budgets, HUD).
- Offscreen/export rendering reuses `gatherScene`/`drawScene` and skips
  `updateResources` (`cwRhiOffscreenRenderer.cpp:487-498`); keying pipelines on
  `context.renderData` is all that's required. Map export hides objects via
  `cwRegionSceneManager::captureHiddenObjectIds()` (`cwRegionSceneManager.cpp:89-99`).
- Memory ledger: `cwLedgeredBytes` + a `Category` (`cwRenderMemoryLedger.h:28-34`,
  labels in `cwRenderingStatsModel.cpp:31-37`).
- Threading pattern: `cwUpdatable` + `AsyncFuture::Restarter` +
  `observe(...).context(this, …)` (`cwLinePlotManager.cpp:518-609`,
  `cwScrapManager.cpp:1250-1278`); `cwRootData.cpp:202-207` registers
  dependencies with the update coordinator.

## 2. How other tools do it

| Tool | Section frame | Pit handling | Bends | Junctions | Sources |
|---|---|---|---|---|---|
| **Survex aven** (`survex/src/gfxcore.cc:3903-4106` `SkinPassage`) | Vertical plane; `right = n(hperp(in)) + n(hperp(out))`, `up = (0,0,1)` always | Both rings of a vertical leg lie in one vertical plane → the pitch is a flat sheet | 4 quads between rectangles, no miter; a corner-reindex fix for torsion after a vertical leg | None; each `*data passage` block is its own tube with flat caps at both ends; tubes overlap | LRUD only; missing values drawn as 1 unit (`fabs(-1)`) |
| **Therion core** (`thdb1d.cxx` `get_3d_walls`) | Per leg; X = horizontal ⟂ bearing; Y = world Z below `vthreshold` 67.5°, else ⟂ to leg in its vertical plane | Above threshold U/D rotate with the leg; exactly vertical: U/D read as N/S | None (independent prism per leg, both ends capped) | Overlap | LRUD, shapes rectangle/diamond/octagon/triangle/tunnel |
| **Therion Loch** (`loch/lxLRUD.cxx`) | As core, 60° default; exactly vertical: `rDir=+X`, `uDir=±Y` | As core | **Miter** in the bisecting plane `n(s_i+s_{i+1})`, fold-back test demotes to capped ends | Chains traverses by straightest continuation (max dot product); through-chain mitered, side arms end with a **dome cap** inside the through tube | LRUD, octagon default; first line of `Calculate()` is `// TODO - more sophisticated LRUD modelling :)` |
| **Therion scraps** (`thscrap.cxx` `process_3d`, `thscrapis.cxx`) | n/a — extrudes the plan outline | n/a | n/a | n/a | Plan outline gives footprint; height from `passage-height`/`dimensions` symbols and centerline U/D, inverse-square interpolated to every outline vertex; vertical wall bands between raised/lowered outlines. Elevation scraps are **not** used for shape |
| **CaveView.js** (`buildCrossSections.js`) | Horizontal ⟂ to bisector of in/out legs; U/D vertical | `|leg·up| > 0.97` → reuse previous `cross`, put U/D along `cross × up` → horizontal box | Fixed-topology quads | `fixup_xsects`: side tube **extended one station into the junction** using the junction station's LRUD, then overlap | LRUD from `.3d` XSECTs; diamond/square/oval(8) |
| **Compass viewer** (`pasmodel.doc`, 3D Modeling page) | Horizontal ⟂ shot, vertical U/D | Above a 45–75° threshold LRUD reinterpreted as W/E/N/S prism | **Corner rounding**: truncate both legs near the corner, insert 1 or 3 artificial shots with linearly interpolated LRUD | "Merge Passages" joins end-to-start only; otherwise overlap | LRUD; "Expand Passages" borrows the nearest station's LRUD for missing values |
| **TopoDroid / Cave3D** | Hull: per leg, vertical plane at the bisector bearing (`normal.z = 0` forced); Tube: plane from smallest eigenvector of the splay scatter matrix | Hull: none; Bubble/Powercrust: degree-agnostic | Per-leg hulls | Overlap; splays assigned to legs by bisector sectors (`HullAngle`); Bubble clips neighboring station hulls against each other | **Splays**: convex hull, "convex-concave" hull, power crust, per-station bubbles |

Takeaways:

- Every LRUD tool keeps L/R horizontal and U/D vertical for walking passage,
  because that is how the numbers were measured. All of them switch frame at
  a hard inclination threshold; none blends.
- Nobody joins junctions properly. Loch's chain + miter + dome is the best
  looking of the shipping approaches and is cheap. CaveView's "extend the
  side tube into the junction with the junction's section" is a one-line
  improvement that hides the seam.
- Only Therion derives 3D from scraps, and only from plan outlines plus
  height symbols. Combining plan width with profile height from drawn
  outlines, as requested here, has no precedent to copy; §3.2 designs it.
- Splay-based tools either build per-leg hulls (TopoDroid) or per-station
  envelopes (Therion `get_3d_outline`, TopoDroid Bubble). Survex's stated plan
  is to render walls from splays and then "treat each LRUD cross section as 4
  splays" — the unified-sample model in §3.2.

## 3. Design

### 3.1 Cross-section frame: z-aligned or pitched?

**Both, blended by inclination.** The reasons:

- LRUD, plan-scrap widths and profile-scrap heights are all measured in a
  vertical section through the station. For passage under ~45°, a section
  plane perpendicular to the path with world-up as its in-plane up vector
  reproduces that measurement.
- In a vertical pit the vertical section is meaningless (aven's flat sheet).
  The useful section is horizontal (x-y), and the useful data are the plan
  footprint (plan scrap rays in all directions, splays, or LRUD read as four
  horizontal directions).
- A hard threshold produces a visible kink where a passage steepens past
  it. A rotation-minimizing frame transported along the chain has no
  singularity at vertical, and blending toward it only when steep keeps the
  horizontal case exact.

Per station `i` on a chain (positions `P`, world up `Z = (0,0,1)`):

```
t_in  = n(P_i − P_{i−1}),  t_out = n(P_{i+1} − P_i)        // one of them at chain ends
t_i   = n(t_in + t_out)                                     // bisector tangent; the ring lies in the plane ⟂ t_i
incl  = asin(|t_i.z|)

up_h   = n(Z − (Z·t_i) t_i)                                 // world up projected into the section plane (vanishes when vertical)
up_rmf = doubleReflection(up_{i−1}, t_{i−1}, P_{i−1}, P_i, t_i)   // Wang et al. 2008, Table 1
w      = smoothstep(θ_lo, θ_hi, incl)                       // e.g. θ_lo = 45°, θ_hi = 75°
up_i   = rotateAbout(t_i, up_h, w · signedAngle(up_h → up_rmf, about t_i))   // pure RMF above θ_hi
right_i = t_i × up_i                                        // east for a northbound horizontal leg
```

Double reflection step (`r` = up reference, `t` = tangent):

```
v1 = P_i − P_{i−1};  c1 = v1·v1
rL = r_{i−1} − (2/c1)(v1·r_{i−1}) v1;   tL = t_{i−1} − (2/c1)(v1·t_{i−1}) v1
v2 = t_i − tL;       c2 = v2·v2
r_i = rL − (2/c2)(v2·rL) v2
```

Notes:

- Seed the chain's RMF from `up_h` at the first station whose `incl < θ_hi`;
  if the chain starts vertical, seed at the first non-vertical station and
  transport backwards. A chain that is vertical throughout seeds from world
  +Y (Loch's convention).
- Sample angles are always measured in the local `(right, up)` basis: L at
  180°, R at 0°, U at 90°, D at 270°. For horizontal passage that is the
  measured meaning. **In a pit, LRUD gives no ring samples** (decision in
  §5): L/R have no usable direction on a vertical shot, and U/D are distances
  along the pit axis, so the ring comes from splays, a plan scrap, or the
  fallback chain (§3.2.4). U/D are still used at a pit **dead end**: the cap
  is placed `D` below the bottom station (or `U` above a top station)
  instead of flush with it. LRUD samples fade out with the same weight
  `w` that rolls the frame, so there is no second threshold.
- Raw `u`/`d` are used as in-plane distances even on inclined passage (what
  every tool does). Projecting by `cos(incl)` assumes ceiling parallel to
  floor and is only ~13% different at 30°; skip it.
- Zero-length legs (coincident stations) and unresolved stations break a
  chain; skip them rather than inventing a frame.

### 3.2 Cross-section sources: one radial-sample model

Every source contributes **radial samples** `(angle θ, radius ρ, weight)` in
the section plane at a sample point `S` with frame `(t, right, up)`. A ring is
then fitted from the samples (§3.2.5). This is what makes "plan adds width,
profile defines height, splays modify, LRUD fallback" a single mechanism.

#### 3.2.1 LRUD → quadrant ellipse

For bin angle `θ`: `a = cosθ ≥ 0 ? r : l`, `b = sinθ ≥ 0 ? u : d`,
`ρ(θ) = a·b / |(b cosθ, a sinθ)|` (the polar radius of the quadrant ellipse at
angle `θ`; the parameter form `|(a cosθ, b sinθ)|` is the distance to the
point at parameter `θ`, which lies at a different angle and bulges the
diagonal bins when `a ≠ b`). Eight points of this are CaveView's "oval" and
Loch's octagon; evaluating it at every bin angle works for any ring
resolution. Missing dimensions produce **no sample** for that quadrant pair
(§3.2.5 handles fill), rather than aven's `fabs(-1) = 1`. Which LRUD
occurrence to use: the one on the station occurrence in the chunk that owns
the leg being skinned (a junction station has one per arm). The ellipse
samples carry weight `1 − w` (§3.1), so they vanish in a pit where L/R are
meaningless and U/D run along the axis.

**Facing (added after Phake Cave 3000).** A station occurrence's left and
right were measured facing one way: along its chunk, as aven draws a
`*data passage` block, i.e. perpendicular to the horizontal chord between
the station's chunk neighbors (the single shot at a chunk end; a vertical
shot borrows the nearest facing in the chunk, else north). The record's
right direction `R` is that facing turned clockwise, and a ring takes each
wall's component across itself: with `c = R · right` of the ring frame, the
ring's right axis reading is `r·c` and its left `l·c`, swapping when `c <
0`. A leg walked backwards therefore needs no left/right swap, and a leg
that leaves a junction station at an angle to the chunk sees the wall
foreshortened instead of drawn full-size across itself (at a6, chunk 1's
5 m left wall was a 5 m fin on the arm a4→a6 and a second 5 m fin, 75°
away, on the through chain a7→a6).

**Shaft readings (added after Phake Cave 3000).** Surveyors record a pit's
depth as the down, or an aven's height as the up, of the stations next to
it — including a dead end with no leg into the pit (a12: L 0, R 2, U 8,
D 35) and the station one leg before the pit (a4: D 23). An up/down reading
more than `kShaftRatio = 3` times the larger of the station's width
(`l + r`) and the median of the same reading at its neighboring stations is
a shaft, not a wall: the ring uses the neighbor median (or half the width),
the raw value still places a steep dead-end cap, and a `ShaftReading`
warning names the station. An up/down reading that runs along a leg leaving
the station (the lip and bottom of a surveyed pit) is dropped from the ring
for the same reason and, on a gentle leg, refilled from the leg's other end.

#### 3.2.2 Splays → projected tips

For every solved splay tip `T` attached to the sample's station (and to the
other end station of the shot when sampling mid-shot):

```
v = T − S;  along = v·t;  q = v − along·t
keep if |along| ≤ max(½ · min(adjacent leg lengths), kMinAlongTolerance)
     and |q| > kSplayParallelFactor · |along|       // drop splays shot down the passage (TopoDroid rule)
θ = atan2(q·up, q·right);  ρ = |q|;  weight = 1
```

Tips come from `LinePlotCaveData::splayTips()`; the manager must start
keeping them (§4.1). Mid-shot samples take tips from both end stations whose
`along` falls near the sample.

#### 3.2.3 Scraps → page-space ray casts

For each scrap `K` whose note stations include the sample's station (for
mid-shot samples: both stations of the shot), work in **page space**, the
paper-meter frame in which the outline polygon is a plain 2D polygon and the
morph is locally linear:

```
pageOf(noteNormalized) = metersPerDot · pixels(noteNormalized)              // paper meters
toCave  = noteTransform.matrix(scale, northUp)                              // paper m → cave m (rotation · uniform scale)
V       = scrap view matrix (Plan: I; ProjectedProfile: fixed; RunningProfile: Data(from, to) of this shot)
S_page  = pageOf(noteStation(S))                                             // or lerp of the two note stations at parameter s along the shot

for each ring direction d_k (world unit vector in the section plane):
    d_view = (V · d_k).xy                                                    // how much of d_k the page can see
    c = |d_view|;  skip if c < kMinPageComponent (e.g. 0.5 ≈ within 60° of the page plane)
    d_page = n(toCave⁻¹ rotation · d_view)
    hit    = nearest intersection of ray (S_page, d_page) with the outline polygon edges
    if hit:  ρ_k = distance(S_page, hit) · scaleToCave / c;  weight = c
```

What this yields:

- **Plan scrap, horizontal passage:** `d_view` is large for the L/R
  directions and zero for U/D, so the plan contributes width and nothing else.
  Diagonal ring directions get partial samples scaled back by `1/c`, so the
  ring's shoulders follow the drawn wall rather than a rectangle.
- **Profile scrap, horizontal passage:** `V` maps world Z to page Y; U/D
  directions project fully, L/R not at all. The profile contributes height
  and the floor/ceiling shoulders.
- **Plan scrap, vertical pit:** the section plane is horizontal, every `d_k`
  projects fully onto the plan page, and the ray fan returns the full pit
  footprint. Nothing special-cased.
- **Mid-shot samples**: `S_page = lerp(page(A), page(B), s)` with
  `S_world = lerp(P_A, P_B, s)` matches how `morphPoints` densifies shots
  with interpolated pseudo-stations. Sampling every `kSampleSpacing`
  (the 1 m default setting) is what makes the tube follow
  curved drawn walls instead of being piecewise linear between stations. Only
  sample along a shot whose both stations are in the scrap and whose
  `S_page` lies inside the outline; otherwise the scrap is silent there.
- Several scraps covering one station: keep each sample; the ring fit takes
  the max per bin (a plan scrap and its profile don't compete because they
  fill different bins). Conflicting plan scraps are a data problem the
  outline editor already exposes.

Projected-profile caveat: the profile shows the passage projected along the
profile azimuth, so a station's up/down ray in page space reads the ceiling
and floor at the station's projected location — fine where the passage runs
roughly along the profile plane, increasingly approximate as it turns toward
the viewer. Running profiles have no such issue.

#### 3.2.4 Priority and fallback

Per sample point, build the sample set from everything available, then fit.
If fewer than `kMinSamplesForRing` bins (say 4) are filled after scrap and
splay samples, add the LRUD ellipse samples. If still too sparse (no LRUD,
or a pit where LRUD contributes nothing): borrow the nearest ring along the
chain that had real samples (Compass "Expand Passages"), and if the chain
has none, use `kDefaultRadius` and flag it so the UI can show "no passage
data" differently (optional). This is the requested "if no scraps or splays
exist, use LRUD" with one more rung so chains never tear. A LRUD-only pit
therefore renders at its lip ring's width, which is the honest answer to
"nobody measured it."

#### 3.2.5 Ring fit

- `N` bins (16 default; 8 is enough for LRUD-only, 24–32 for dense splays),
  bin centers at `θ_k = 2πk/N` in `(right, up)`.
- Per bin: `ρ_k = max(ρ)` over samples in the bin, optionally a trimmed max
  when ≥3 splays land in one bin (outlier guard).
- Empty bins: interpolate radius angularly from the nearest filled bins
  (linear, or Catmull-Rom for ≥4 filled bins). Star-shaped by construction.
- Optional concavity limit (TopoDroid's `TODO allow a bit of concavity`):
  clamp a bin to at least `kMinConcavity ×` the mean of its neighbors to
  avoid single-splay spikes inward. The outward mirror, `kOpeningRatio`, is
  how scrap openings are caught (§3.5.3).
- Emit `N` ring vertices `S + ρ_k (cosθ_k right + sinθ_k up)`. Same `N` and
  same `θ_0` everywhere means consecutive rings zip with fixed topology.

#### 3.2.6 Units and conventions

- LRUD strings → `cwUnits::convert(toDouble(), trip.calibrations.distanceUnit(), Meters)`.
- Splay tips and station positions are already meters, local projection.
- Scrap distances: paper meters × note-transform scale, as the morph does.
- L/R follow chunk order; the chain may traverse a chunk backwards, in which
  case swap L and R for that leg (the Survex importer already does this on
  input, `cwSurvexImporter.cpp:1438-1470`).

### 3.3 Joining rings into a tube

#### 3.3.1 Chains

Build an edge list from every chunk's shots (edge = from, to, chunkId, shot
index; positions from the cave's lookup). Then chain like Loch:

1. Compute degree per station. Seed chains from odd-degree stations first
   (dead ends, then 3-way junctions), then even-degree.
2. From the current edge end, continue along the unused edge with the
   **largest dot product** with the current direction. Stop at a dead end
   (`Flat` end), or when the next node has no unused edges but other edges
   exist (`Round` end at a junction). Remaining edges at a junction seed new
   chains that start there.
3. Loops: a chain whose last node is its first closes on itself (no caps).

Result: exactly one chain passes straight through every junction; every
other arm starts or ends there.

#### 3.3.2 Along a chain

- Rings at stations use the bisector frame (§3.1), which **is** the miter:
  the ring lies in the bisecting plane of the two legs, so the side quads
  between consecutive rings meet with no gap and no fold. No separate miter
  pass is needed as long as rings are only placed in bisector planes.
- Stretch at sharp bends: a ring in the bisecting plane is elongated by
  `1/cos(φ/2)` for turn angle `φ`. Above `kSubdivideBendAngle` (~60°), use
  Compass corner rounding instead: truncate both legs `ε` before the corner
  (`ε = min(kCornerInset, ¼ leg)`), insert one or two interpolated sample
  points with lerped rings, and frame them by RMF. This also removes Loch's
  fold-back failure mode (ring edges that reverse direction when the miter
  plane is too oblique).
- Mid-shot samples (§3.2.3) are just extra rings with `t = leg direction`
  and RMF-transported `up`.
- Side quads `(ring_i[k], ring_i[k+1], ring_{i+1}[k+1], ring_{i+1}[k])` → two
  triangles, outward winding. **Phase 0 shades flat** from screen-space
  derivatives in the fragment shader, so the vertex layout is position only
  and every generated quad is visible as built (decision in §5). A `Normal`
  attribute with averaged vertex normals is a later "smooth" option; the
  builder owns the geometry, so computing them is a small addition.

#### 3.3.3 Junctions — the clean merge

Phase 0 and 1 closed each arm with a dome inside the through tube and let
the depth test hide the overlap. That failed two requirements — translucent
or cut-away rendering shows the interior faces, and a fly-through sees the
dome as a bump on the wall — and on Phake Cave 3000 the dome itself read
wrong (an arm whose last ring is borrowed from a neighbor pushed a deep box
across the through passage). The junction is now a separate **stitch pass**
between ring fitting and meshing, working on ring columns only, so LRUD,
splay and scrap rings all go through the same code. It was reviewed in 2D
on 18 cases in `plans/TUBE_JUNCTION_GALLERY.html` (local file).

For a junction `J` with through chain `T` and arms `A_a` (each arm's end
ring sits at `J`, no push):

1. **Fill.** The arm's section is carried from the through's near wall to
   its far wall: `fill_a = armTube_a ∩ extrude(T, n_a)`, where `armTube_a`
   is the arm's own rings read as a solid (last ring continued past `J`)
   and `extrude(T, n_a)` is the through tube with each ring's polygon
   swept along `±n_a`, `n_a = t_T × t_a`. For a side passage `n_a` is
   vertical, so the fill is the arm's section limited to the through's
   plan width; for a pit dropping into a passage `n_a` is horizontal across
   the passage, so the fill is the pit's section between ceiling and
   floor; for a passage entering a shaft it is the passage's section
   between the shaft's near and far walls. Nearly parallel arms (`|n_a| <
   sin 10°`) use the plain through tube.
2. **Grow.** Every through ring near `J` extends each column as far as its
   ray stays inside a fill (ray march + bisection). A trench floor dips
   across the full width to the far wall; a tall arm makes a chamber
   across the junction; an arm smaller than the through changes nothing.
   Growth tapers with the arm's outline, so there is no blend constant.
3. **Trim.** Each arm column is cut where it first enters the union of the
   through tube and the fills; the cut points form the **mouth loop**, on
   the through surface by construction (on the near-wall plane for columns
   wider than the through). The arm keeps its rings up to the last one
   entirely outside, then one strip to the mouth.
4. **Cut and zip.** The through quads whose centers fall inside the mouth
   loop in (ring, column) parameter space are removed; the window's
   boundary is zipped to the mouth loop by a greedy two-loop
   triangulation. The result is one closed, consistently wound surface
   with no faces inside either tube.

Known limits, in the order they are worth fixing: two arms whose windows
share through quads fall back to a fan-capped overlap with a warning (merge
the windows later); a shaft's far wall is flattened across an entering
passage's width (the extrusion carries the section to the wall's plane,
not its curve; small); grazing joins give a long mouth; the through needs
extra samples near a junction to resolve the mouth, which the pass inserts
itself. A fillet or smoothing of the crease at the mouth is a possible
later step and is cosmetic.

Dead ends still get a fan cap to the ring centroid.

#### 3.3.4 Junctions — phase 2 options

- **Hull patch (B-Mesh / Blender Skin / SQM):** at each junction collect the
  nearest ring of every incident chain, compute the 3D convex hull of their
  vertices (a QuickHull over `N · degree` points), delete hull faces that lie
  inside an incident ring's disc, and stitch the openings to the tubes. Gives
  a manifold junction with explicit "cross-sections joined at junctions"
  geometry. Costs a convex-hull routine (~300 lines, no new dependency) and
  concave-junction cleanup; Blender's implementation notes that tight
  branches still produce overlapping topology without a smoothing pass.
- **Signed-distance union + marching cubes / dual contouring:** per leg a
  distance to the swept ring (or elliptical capsule), `min` across legs
  (optionally a small smooth-min at nodes), evaluated on a sparse voxel grid
  clipped to each leg's AABB, then isosurface and decimate. Watertight and
  degree-agnostic; loses the ring correspondence, produces uniformly dense
  meshes, and needs decimation/LOD. The ring pipeline above is reused as the
  shape definition, so this is an additive mode, not a rewrite. Prefer this
  if cutaway/translucent rendering or export to a solid model becomes a
  requirement.

### 3.4 Mesh output

```cpp
struct cwPassageTubeMesh {
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;          // omit if flat shading via derivatives
    QVector<quint32>   indices;          // triangle list
    QVector<VertexRange> tripIndexRanges; // running trip id -> [first index, count], for keyword visibility
    QVector<QUuid>     tripUuids;
    QVector<Warning>   warnings;         // e.g. stations with no passage data, chains that tore
};
```

Size estimate: 16-vertex rings, one ring per station plus mid-shot samples
≈ 3 rings per shot → ~48 vertices and ~96 triangles per shot. A 10,000-shot
cave is ~0.5 M vertices / 1 M triangles / ~12 MB with normals — fine for one
immutable buffer pair, and well under the budgets the memory ledger tracks.
Rebuilding is a per-solve cost in a worker, so it never blocks the UI.

### 3.5 Concave outlines and un-surveyed side passages

Every real scrap outline is concave: walls wiggle, passages neck down, and
sketchers draw alcoves, side leads and whole side passages that have no
survey station in them. The question is what the ring model does at those
concavities, and in particular at a *split*: a lobe of the outline that is
really a passage with no centerline.

#### 3.5.1 What the scrap pipeline does today

- The outline is one `QPolygonF` (`cwScrapData::outlinePoints`), and
  `cwScrap` enforces no convexity or simplicity on it. Concave is the normal
  case; a self-intersecting outline is also accepted.
- The mesh handles concavity completely. `cwTriangulateTask::createQuads`
  classifies grid quads with `containsPoint(Qt::OddEvenFill)` at the four
  corners (full quads) or an outline-edge crossing (partial quads,
  `PointGrid::intersects`, `cwTriangulateTask.cpp:1379`). Partial quads go
  through `QPolygonF::intersected`, `addPointsOnOverlapingEdges`,
  `createSimplePolygons` and `cwTriangulate::Process`, a classic ear-clipping
  triangulator that takes any simple polygon, concave included. A failed clip
  is ignored (`Q_UNUSED(couldTrianglate)`, `:565-567`), and a
  self-intersecting outline fills odd-even, so figure-eight lobes alternate
  filled/empty.
- The morph treats a lobe as texture, not as a passage. Each grid point is
  positioned by an inverse-square-distance blend (`morphPoint`, `:1262`) of
  the `maxClosestStations` (10, `cwTriangulateWarpingData.h:15`) nearest
  pseudo-stations, where every shot is densified every 0.5 m
  (`buildStationsWithInterpolatedShots`, `cwTriangulateTask.h:75`). The
  outline-visibility test `stationsVisibleToPoint` exists but its call is
  commented out (`:959-963`), so a lobe with no station inside is simply
  pulled by whichever main-passage pseudo-stations are nearest. A side passage
  drawn in a plan scrap therefore renders as a flat sheet hanging off the main
  passage at the interpolated Z of its neighbors (then `smoothZ` with a 2 m
  radius); in a running profile a lobe lies in the bracketing shot's profile
  plane. Nothing in the pipeline knows the lobe is a passage.

So "concave scraps" are a solved problem for the sheet and an unsolved one
for the tube: the tube needs to decide where the passage *is*, which the sheet
never had to.

#### 3.5.2 What the ray-cast ring does at a concavity

The ring is star-shaped around the sample point `S_page` (nearest hit per
direction, §3.2.3), so the cases are:

1. **Neck or pinch.** The nearest hit reads the neck. Correct.
2. **Pillar** drawn by looping the outline inward. Rays hit the pillar's
   wall, the ring narrows on that side. Acceptable.
3. **Sample outside the outline** (the sketched wall misses the station, or
   a mid-shot point crosses a drawn pillar). `containsPoint` is false, the
   scrap is silent there and splays/LRUD take over (§3.2.3 already requires
   this). Keep it, and record a warning so the user can see which scraps
   miss their own stations.
4. **Opening.** A lobe, alcove or side passage whose mouth faces the
   sample. The ray through the mouth hits the lobe's far wall instead of the
   main wall. In a plan scrap every ring direction with a right component
   projects onto that same page ray (§3.2.3: `d_page` is the page-side
   direction for L, both shoulders and everything between, each scaled by
   `1/c`), so the whole side of the ring is pushed out together, and the
   angular neighbors cannot catch it. Sweeping the mid-shot samples past the
   mouth gives a run of such rings: a slab-shaped fin protruding from the
   tube where the side passage should be (Figure 9a in the HTML version).
   This is the failure case of the model and has to be handled.
5. **Profile lobes** are vertical openings: a dome in the ceiling or a pit
   in the floor drawn as a lobe in a running or projected profile. Same spike,
   in the up or down bins.

#### 3.5.3 Options

**A. Clamp the opening (phase 2, day one).** A scrap contributes at most
two page rays per sample (plan: ±right; profile: ±up), so compare each
ray's hit distance with the median of the *same* ray over the
`±kOpeningWindow` (3) neighboring samples along the shot. A hit above
`kOpeningRatio` (≈1.8) times that median "sees through an opening": replace
it by the interpolation between the nearest unflagged samples on either
side, and let the ring bins that share the ray follow. This is the
along-the-chain mirror of the angular concavity clamp in §3.2.5. The lobe
vanishes from the tube and still shows in the scrap layer. Record each clamp
as `Warning{ScrapOpening, scrapId, station, s, direction}`; the warnings are
the detector for option B.

**B. Walk the lobe: a synthetic chain (recommended, phase 2.5).** This is
the "make an intersection and generate separate cross-sections even though
there is no centerline" idea. Treat an opening as a junction whose third arm
is a chain with no survey shots, and let the ring machinery sample it.

- *Detect.* Along each shot, for each page-space side direction (plan:
  ±right; profile: ±up), look at `ρ(s)` over the mid-shot samples. A mouth is
  an interval `[s₀, s₁]` where ρ jumps by at least `kOpeningRatio` and comes
  back, whose page width `(s₁ − s₀)·L` is at least `kMinMouthWidth`
  (≈ max(2 × sample spacing, 0.75 m)), and whose depth (the hit distance
  through the mouth) is at least `kMinLobeDepth × mouthWidth` (1.0 — a bay
  shallower than its mouth is a wide spot; the clamp handles it). Mouth
  center `M_page` is the midpoint on the wall line, the start direction `n`
  is the side direction.
- *Walk.* From `M_page` step `kSampleSpacing` along `n`. At each step cast
  the two rays perpendicular to the current direction, move the center to
  the midpoint of the two hits (the local medial point), turn the direction
  toward that drift with the turn clamped to `kMaxTurnPerStep` (≈30°), and
  record `(center_page, halfwidth_page)`. Stop when the forward ray hits
  within one step (dead end → cap), when the center leaves the outline, when
  the walk comes within `kRejoinDistance` of a surveyed shot of the same
  scrap (the lobe is a bypass loop: finish at that second mouth and join
  there too), when the width exceeds `kMaxLobeWidthRatio ×` the mouth (a
  chamber, not a passage: stop and cap), or after `kMaxLobeSteps`. Openings
  seen while walking recurse once (nesting limit 2).
- This is the medial axis of the outline computed on demand along one
  branch, with the ray caster the design already has. A general medial axis
  or straight skeleton (CGAL `Segment_Delaunay_graph_2`,
  `Straight_skeleton_2`, Boost.Polygon Voronoi) would give the same branches
  plus a pruning problem, and none of those libraries are in
  `conanfile.py`. A raster distance transform via the existing
  `cwSketchScrapRasterizer` is the fallback if the walk ever proves
  insufficient.
- *Place in world.* Map page points through the same morph the scrap uses
  (`cwTriangulateTask::morphPoint` with `buildStationsWithInterpolatedShots`),
  so the synthetic centerline lands exactly where the scrap texture lands.
  Width comes from the two page hits. A plan lobe has no height, so inherit
  the junction sample's up/down bins (from its profile, splays or LRUD) and
  hold them constant along the lobe, optionally tapering toward a dead end.
  Profile lobes mirror this: height from the hits, width inherited from the
  mouth ring. Where a plan lobe and a projected-profile lobe describe the
  same side passage, both appear as openings on their own sample rows; match
  them by `s` along the shot within `kMouthMatchTolerance` and combine width
  with height.
- *Join.* The synthetic chain is an ordinary junction arm in §3.3.3 terms:
  first ring at the mouth, pushed ½ρ into the through tube and dome-capped
  inside it; the phase-4 hull or SDF applies unchanged. Mesh output is the
  same `cwPassageTubeMesh`; tag the vertex range `synthetic = true` so the
  shader can tint it and the UI can toggle it, since its height is inherited
  rather than measured.
- *Splays.* A splay that points into the lobe already lands in the spiking
  bin; when it falls inside the mouth sector it becomes a sample for the
  synthetic chain's first rings, otherwise it stays a ring sample at its
  station as before.
- *Tests.* Rectangular outline with a rectangular side bay → one synthetic
  chain with the expected ring count and inherited height; bay shallower than
  its mouth → clamp only, one warning; U-shaped bypass → chain ends at the
  second mouth; self-intersecting outline → no crash, warnings only.

**C. Implicit footprint extrusion (phase 4 alternative).** The plan
footprint times the height profile is a 2.5D solid: rasterize the outline's
signed distance in page space, extrude between floor and ceiling heights
taken from the rings, union with the tube SDF, marching cubes. Lobes come for
free with no detection and no synthetic centerline, at the cost of ring
correspondence and a dense mesh. It gives the phase-4 SDF option a second
justification but is heavier than B for the common case.

Decided (§5): A in phase 2 (one guard in the ring fit plus a warning), B
as phase 2.5 behind a `followScrapLobes` tube setting, C stays optional.

### 3.6 Artifact checklist

Things that produce visible glitches in tube renderers and where this
design catches each one. Items marked *added* were missing from the sections
above and are now part of the phase they name.

| Artifact | Cause | Handling | Phase |
|---|---|---|---|
| **Twisted segment** (bow-tie quads, ring rotates almost a full turn between two samples) | The blend `up_i = rotate(up_h → up_rmf, w)` uses a signed angle; when that angle crosses ±180° between samples, or the RMF has accumulated a large twist down a pit and the blend hands back to `up_h` on the way out, consecutive rings are zipped index-to-index across a big rotation | *Added.* Unwrap the blend angle along the chain (take the branch closest to the previous sample's angle) and, after a pit, distribute the residual twist between `up_rmf` and `up_h` over the transition samples instead of letting `w` absorb it; assert `right_i · right_{i-1} > 0` in the builder tests | 0 |
| **Inner-bend fold** (quads flip inside out on the inside of a bend, dark facets or holes) | Ring radius on the inside of the bend exceeds the bend's radius of curvature, so vertex `k` of ring `i+1` lands behind ring `i`'s plane | *Added.* Per vertex, if `(v_{i+1,k} − c_i) · t_i < 0`, pull `v_{i+1,k}` onto ring `i`'s plane along its own ring direction (Loch's fold-back test, applied per vertex instead of per ring); bend subdivision (§3.3.2) reduces how often it triggers | 0 |
| **Open tube ends when a trip is hidden** | `tripIndexRanges` split the mesh by trip; hiding a trip removes its quads and leaves the neighboring trip's last ring open, showing the interior | *Added.* Emit a fan cap at every trip boundary, owned by the trip it closes; it sits inside the neighboring tube and is hidden by depth while both are visible. The same caps serve keyword/date filtering, which is per trip | 0 |
| **Pinched ring from `0 0 0 0` LRUD** | Many surveys write zeros for "unmeasured"; a radius-0 ring makes degenerate triangles and a visible pinch | *Added.* All four dimensions zero → treat as absent (fallback chain, §3.2.4); a single zero (station against a wall) is a sample at `kMinRadius` (0.05 m). Values above `kMaxRadius` (say 100 m, a unit mix-up) are dropped with a warning | 0 |
| **Degenerate quads between coincident rings** | A sample spacing of 1 m on a 0.3 m shot, or bend subdivision near a very short leg, can place two rings within ε of each other | *Added.* Merge rings closer than `kMinRingSpacing` (0.05 m) along a chain; never emit two rings at one position | 0 |
| **Inside-out lighting / see-through walls from inside the cave** | Backface culling discards the far wall when the camera is inside the tube; with one-sided lighting the inner wall is unlit | *Added.* No culling; the flat shader flips the derivative normal with `gl_FrontFacing` so both sides light correctly. Viewing from inside is a normal use (fly-through) | 0 |
| **Junction stubs visible from inside** | A dome-capped arm overlapping the through tube is hidden by depth from outside; from inside, under translucency or in a cutaway it is a bump or a wall through a wall | *Replaced.* The stitch pass (§3.3.3) trims the arm at the mouth, cuts the through surface and zips, so the junction has no interior faces; `cwPassageTubeMeshCheck` (self-intersection and nested-shell detectors) guards it in every builder test and the junction sweep | 1 |
| **Zig-zag facet pattern along the tube** | Side quads are non-planar whenever consecutive rings differ; their two triangles get different flat normals, so the fixed diagonal reads as a herringbone | *Added.* Split each quad along its shorter diagonal (or alternate by `k`); cosmetic under flat shading, gone with smooth normals | 0 |
| **Pinch at a scrap's end** | A scrap outline closes across the passage at its ends; rays from samples near that end hit the closing edge instead of the wall, and the ring collapses toward the centerline | *Added.* Ignore hits on outline edges that run across the passage: in page space keep a hit only if the edge direction satisfies `|ê · t_page| > cos(kMaxWallEdgeAngle)` (60°) or the hit is farther than `kScrapEndMargin` from the sample's projection on the closing edge; samples with no valid wall hit are silent and fall back | 2 |
| **Step where sources change** | A scrap ends mid-passage, or splays exist at one station and not the next; the ring jumps from one width to another at a single sample | *Added.* Cross-fade radii over `kSourceBlendDistance` (2 samples) when the dominant source changes along a chain; record a `SourceChange` warning so the user can see where their data ends | 2 |
| **Spike from one splay or one lobe** | Covered: inward splay spikes by the concavity clamp (§3.2.5), outward lobe spikes by the opening clamp (§3.5.3) | — | 1, 2 |
| **Outer-bend stretch** | Covered: miter in bisector planes, subdivision above `kSubdivideBendAngle` (§3.3.2) | — | 0 |
| **Zero-length legs, unresolved stations, splays shot along the passage** | Covered: §3.1 last bullet, §3.2.2 | — | 0, 1 |
| **Interpenetrating tubes** (parallel passages, loops, overgenerous LRUD) | Data: the tubes intersect and the seam is visible as a wall passing through a wall | *Planned (T07x).* Cut each crossing chain where it enters the other and zip the openings as at a junction; join parallel overlaps along the shared wall; cases 27–29 first. Accept; opaque drawing hides most of it. The checker reports a crossing between two chains as `PassageOverlap` (a warning, since the union is what the viewer sees) and keeps `SelfIntersection` for crossings within one chain or involving a zip band (T07l) | 3 |
| **Sloped walls off the ring** (`WallMissed` up/down on every sloped passage) | Up and down are plumb readings, but the ring square to a 30° leg used them unforeshortened, so the ceiling sat `1.5 (1 − cos 30°)` above the wall; a dead end's ring square to the leg leaves the station's vertical wall points past the end plane | *Added (T07h).* Project up/down onto `frame.up` as the side walls are projected onto `frame.right`; gentle dead ends get a plumb end ring (vertical plane through the station, raw readings), with the sharp-bend inset rules applied to the reflected leg when a tall section would fold through the ring before it | 0 |
| **Pit at the default radius, cone cap** | The ring fit skipped every steep ring, so a pit chain without a gentle ring borrowed nothing and drew at `defaultRadiusMeters`; the pit cap extended by `up`/`down` through a fan to a point | *Added (T07h).* A steep ring with left/right fits a round section of radius `(L + R)/2`; a steep end emits a ring at the cap offset with the same radii and a flat fan | 0 |
| **Thin-section ridge** (150–160° creases along the ceiling of a 0.8 × 7 m section, most random-seed creases) | Bins sampled the quadrant ellipse at uniform angles: for `a = 0.8, b = 7` the bins at 67.5° and 90° land at (0.77, 1.86) and (0, 7), one vertex for 5 m of wall | *Added (T07h).* Sample bins by eccentric anomaly (`a cos t_k, b sin t_k`) so vertices spread by arc length; each ring stores its bin angles and every consumer (`binDirection`, `polarRadius`, growth, zip) reads them from the ring | 0 |
| **Sparse mouth loop** (150–180° creases, slivers and crossings in zip bands and mouth strips; all remaining random-seed issues after T07l) | The mouth has one vertex per arm column (16) while a mouth as big as the through, or oblique to it, spans 10–50 through rings: random seed 1's mouth is 10 m long with 4.5 m edges over a 0.5 m through spacing. Such an edge is a chord through the through's surface, so the zip triangles on it are slivers folded against the arm's strip, one mouth vertex fans to a dozen boundary vertices, and the strip quad between two columns whose mouths sit 0.5 m and 5 m away twists | *Added (T07m).* Trace sub-columns between adjacent arm columns (bisecting until adjacent mouth points are within 1.5 · the through spacing, at most 16 per column), mesh the strip as a fan per column, and zip over the denser loop | 0 |
| **Overlapping windows** (a passage ending in a cone inside another at every 2- and 3-arm junction of equal passages; 1001 of 1836 sweep junctions) | Each mouth of an arm the through's size wraps more than half the through, so two windows at one junction share roof and floor quads and the second arm falls back to the hidden cone with `JunctionOverlap` | *Added (T07n).* Merge windows whose quads intersect while their mouth polygons stay disjoint, bridge the boundary and mouth loops into one inner loop and zip it with the two-loop DP; arms whose mouths intersect keep the fallback | 0 |
| **Full-wrap arm** (an arm ending in a flat cap with the through running into it, like a pipe into a tin can; 32 sweep junctions, all `ratio 2.5` or two small arms in a 90° bend) | An arm larger than its through has a mouth that loops the whole circumference, so `mouthPolygon` has no closing edge and the arm is capped with `JunctionUnzipped`. The big-passage rule of T07j only re-chains the big arm as the through when one leg continues it within `kMaxChainTurnDegrees` and no other leg ties its size; and when it does, the through's own leg on that side is left grazing the big wall at 20–30° and its columns miss | *Added (T07o).* The through pair at a junction is the two largest legs with the smallest turn, ties allowed; a biggest leg with no partner ends its chain past J and takes every other leg as an arm; a mouth that still wraps is cut as a whole-circumference window zipped between its front and back loops; a grazing leg takes its parameters from the nearest wall point or is reported as grazing from the built chains | 0 |
| **Centerline outside between elbow rings** (Phake Cave a9: the leg leaving the foot of the shaft runs up to 0.084 m outside the tube for 1.15 m) | The corner reads `down = 0`, so its floor is the 0.05 m minimum radius; T07r holds the station inside the middle elbow ring, but the skin from that ring to the next is a straight chord across the bend and sags past the floor; the checker hid it by counting points within `kWallTolerance` (0.15 m) of the wall as inside | *Planned (T07s).* Centerline rule with its own 0.01 m tolerance; keep a ring at the station inside the elbow so the skin follows the legs | 1 |
| **Knob at a sharp bend** (gallery 23; Phake Cave a1 and a3: the tube bulges around the corner station) | T07r fits the middle elbow ring to the union of the readings around the ring center and around the station, so the ring reaches the center's offset past the readings on the bend's inside; no checker rule reports a wall wider than the readings | *Planned (T07v).* `WallBeyondReading` rule first; then fit the bend to the station's ellipse seen from the ring center, relying on T07s's station ring for the chord | 1 |
| **Unread corner creases** (catalog case 24) | The elbow inset at a station with no readings uses the default radius while the rings borrow wider walls | *Planned (T07t).* Resolve borrowed sections before the elbow inset | 0 |
| **Bare hairpin dome** (a dome the other chain's tube leaves uncovered) | `CapExposed` tags every cap but domes; a 150°+ hairpin or a wide-into-narrow hairpin leaves the wide dome's apex outside | *Planned (T07u).* Tag domes; case 26; extend the narrower end's offset to the wider apex | 0 |
| **Rings invisible** (every elbow and frame problem diagnosed from printed numbers) | The gallery shows the skin and the markers but not the rings and frames the builder made | *Fixed (T07y).* A second line plot draws every ring loop and its frame cross; elbow and grown rings are told apart; the report lists each elbow's offset and frame roll | 0 |
| **Section fit invisible** (a bulging ring at Phake Cave a2/a3 cannot be traced to its readings) | `fitLrudRing` rotates, widens, fills and unions the readings and keeps only the final radii; `stationRadii` is dropped before the ring is built | *Planned (T07z).* Rings keep their fitted section; the gallery draws the survey readings as spokes, each ring's station ellipse, and whiskers where the ring overshoots it; the report lists the overshoot per ring | 0 |
| **Overlays in two colors** (rings, frames, readings, sections and whiskers all red or pink) | `cwRenderLinePlot` has two segment types, centerline and splay, and the gallery's seven overlay kinds share them | *Landed (T07aa, `04ef8135`).* Segment type becomes a palette index (7 types, one color table in C++ mirrored in `LinePlot.frag` under a test); each gallery layer draws in its own color with a swatch by its checkbox | 0 |
| **Gallery crawls with rings on** (a large project stutters while orbiting and takes seconds per toggle once Rings or Sections are shown) | The ring plot is registered with the intersecter like the centerline (a sub-BVH over a million segments per build), `ringAt` scans every ring per pointer move, `setRangeVisible` copies the whole mask five times per toggle | *Planned (T07ab).* Measure first; keep the ring plot out of the intersecter (`setPickable(false)`), one `ringAt` per frame over a center grid, one mask publish per toggle | 0 |
| **Neck past a corner station** (Phake Cave a2 → a3: the left wall goes 2.13 → 2.61 → 1.82 m around a2, which reads 2.5; every sharp corner in the catalog does the same) | Along a leg `lrudAcross` projects each side reading onto the ring's right axis by the cosine of the facing's angle to the leg (35° at a2, ~45° at a 90° corner), so the leg interpolates between shrunk values, while the elbow's middle ring keeps the reading stretched by the miter | *Landed (T07ac, `ae3de2e6`, `70c7a4f0`).* Side readings at face value along the leg and on the corner ring (the facing only sets the side); up/down keep T07h's slope foreshortening; case 30 (an S-bend small → large → medium) and a test that every wall runs between its two stations' readings | 0 |
| **Corner rings off the legs' walls** (case 30 at s1: the outside wall bulges 0.3 m at the first elbow ring then cuts the corner; the inside wall has a 0.1 m shelf; Phake a2's inside wall dips to 2.2 m where the legs head for 2.5) | The elbow's middle ring puts the reading at face value along the bisector, `L cos(halfTurn)` from either leg's centerline, and `blendElbowRings` lerps radii between rings whose centers sit 0.4–1.7 m apart | *Landed (T07ad, `1eb19b22`).* The two leg tubes, each at its interpolated width and run on past the station as far as it stays inside the other's, are the corner's envelope (a miter outside, a square corner inside); every elbow ring reaches from its own center to it | 0 |
| **Question-mark pit and inflated corners** (Phake a6 → a9 in profile from 54°: a 90° swept elbow of up to 5.8 m radius hooks over the lip, then a round shaft of radius `mean(L, R)` centered on the shot sits 1.5 m off and 1.6 m narrow where a6 reads `L1 R5`; every corner in the catalog carries the same swept surface) | An elbow ring is a station section swept around the bend from a center off the station, and every corner fit (T07ad, T07af, T07r, T07s, T07t, T07v) exists to pull that surface back onto the legs; steep rings discard the facing | *Landed in part (T07ah1 `d0d33219`, T07ah2 `8001628b`: lofts and plumb sections); the station cap (T07ah3) is blocked on the lofts interpenetrating at two-section stations, see FOLLOWUPS. A leg is the loft between its two station sections (quad-ellipses through the reading points, pit and aven readings at face value; a plumb leg's section is horizontal with front/back at `mean(L, R)`); rings are point-wise lerps; lofts that share a section meet in a crease at the reading points, and a station whose sections differ gets the convex hull of them as a cap; no elbows, no smoothing of any kind — the smooth transitions come from scrap interpolation (T09/T10) | 0 |
| **Leg wider than the line between its readings** (case 30 s1 → s2: the rings run 2.71 → 1.65 m each side where the line from s1's reading point to s2's passes 2.19 → 1.33 m from the centerline, and the inside wall steps 0.4 m at s2) | Both stations read facing the bisector of their legs, so their reading points lie `L cos 30°` from the centerline and behind the station; T07ac's face-value rule uses the reading as the perpendicular half-width, so the leg is `L (1 − cos 30°)` too wide at each end and only the station section ends on the readings | *Landed (T07ag, `fdfbbdfb`).* A side reading is a point on the wall (station + reading along the facing's left/right); a leg's wall is the line between its two stations' points, rings take their half-widths where that line crosses their plane; a corner's walls then meet at the reading points | 0 |
| **Fat, skinny, fat between a3 and a4** (Phake a3 → a4: the left wall runs 1.5 → 0.5 then climbs to 4.2 over the last 3.5 m) | a4 is read in two chunks (`L0` facing a5, `L5` facing a6); the leg interpolates toward the first while the station ring and the elbow inset take `widestReadings` over both | *Planned (T07ae).* A chain leg and its end ring use the record whose facing lies along the leg; the other record belongs to the a4 → a6 passage | 0 |
| **Rolled rings at a shaft foot** (case 25 at f2: the elbow rings are rolled 120°/92°/60° about their tangents, the middle ring reaches 1.26 m under a station that reads `down = 0`, and the exit's right wall crosses the elbow) | `chainFrames` transports the lead-in frame down the round shaft and unwinds the 120° roll to the exit's horizontal frame over the four samples after the shaft, i.e. across the elbow and the first exit rings | *Landed (T07af, `d6dc315b`).* Elbow frames at a steep–gentle corner lie in the plane of the legs; the roll is spread along the shaft, whose rings are round | 1 |
| **Real projects cannot be inspected with markers** | The checker and its markers live in tests and the gallery; the gallery builds only synthetic surveys | *Planned (T07w).* The gallery opens a `.cw`/`.cwproj` from a temporary copy as a row per cave, with `--station` framing and station names on markers; CaveWhere stays marker-free | 0 |
| **Caps where the survey continues** (flat and fan discs at tie-in stations; 150 chain ends at non-dead-end stations in a real project) | Hairpin breaks (legs turning 100°–170°, three of them cross-trip tie-ins) end both chains with fans whose tubes leave each other uncovered; a junction no walk reaches first seeds itself and starts every arm there, so all three ends are fans; a re-shot tie-in gives two coincident legs and a false degree-5 junction; the two coincident boundary discs emitted at every trip change stand across a junction window or poke out of elbow and grown rings | *Added (T07p).* Checker rule `CapExposed`; merge legs with the same station pair (`RepeatedShot` warning); walk backward through a seeded junction; move each hairpin end ring past the station along its leg by the station's half-width and close it with a sphere-profile dome (a half ring at `r/2` and depth `r·√3/2`, apex at `r`), so the two domes overlap around the station and the centerline stays inside both; emit boundary discs into their own index ranges, clipped to kept columns, drawn only while the neighbor trip is hidden | 0 |
| **Arm cut far from the junction** (stations and shots outside the tube beside a big room; four stations of a real project) | `trimArm` keeps rings only up to the first column that touches the grown through anywhere along the arm, so a 3–5 m passage running 15 m beside a 16 m room through two bends, or a default-radius passage bending inside a 6 m through's reach, is cut many rings early and one straight strip to the mouth spans the bends | *Added (T07q).* Checker rule `CenterlineOutside` (stations and 0.25 m centerline samples inside the mesh); keep rings up to the last center outside the through, project columns already inside onto the wall, search entries from there | 0 |
| **Corner station outside its elbow ring** (two stations of a real project, 7–10 cm out) | The middle elbow ring sits `inset · tan(φ/2)` off the station toward the bend's inside but fits the readings as radii from the ring center, so a zero reading on the bend's outside (`kMinRadiusMeters`), measured or borrowed from a neighbor trip, falls short of the station | *Added (T07r).* Translate the readings by the ring's offset before the fit; borrow fits from the same passage before a neighbor trip's | 0 |
| **Mouth off the through mesh** (~150° creases and slivers at zipped mouths) | The ring-tube inside test interpolates the radius linearly between bins while the mesh is the chord, which lies `r (1 − cos π/N)` inside it — 2 cm at `r = 1.2`, the size of `kJunctionTolerance` — so mouth vertices float above the wall; where the arm's ceiling coincides with the through's, mouth and boundary vertices sit centimeters apart | *Added (T07i).* Trim each arm column against the grown through's triangles (Möller–Trumbore), take the surface parameter from the hit, weld mouth vertices within `kWeldMeters` of a boundary vertex or of each other, re-grow the through without arms that were dropped, and hide a fallback fan on a cone inside the through | 0 |
| **Line plot and stations hidden inside the opaque tube** | The centerline lies on the tube axis, so an opaque tube covers it from outside | Expected under decision 2 (opaque) and decision 3 (independent layers): the user toggles the tube off, or views from inside. A depth-biased "centerline on top" mode is a later option, listed under phase 4 | 4 |
| **Z-fighting with scraps** | The plan sheet is horizontal and the tube wall is vertical where they meet, so they cross along a line rather than share a plane | None expected; verify on the first scrap-driven build | 2 |

## 4. Architecture

### 4.1 Builder and manager

- **`cwPassageTubeBuilder`** (new, pure, `cavewherelib/src`): static
  `build(const cwCavingRegionData&, const QHash<QUuid, cwStationPositionLookup>&, const QHash<QUuid, cwSplayTipsByStation>&, const Options&) -> cwPassageTubeMesh`.
  Internally: edge list → chains → per-sample frames → samples → rings → mesh.
  Every step is a free function over value types, callable from any thread
  and unit-testable with hand-built data. Follows the CLAUDE.md worker rules
  (`.at()`, value snapshots, no `shared_ptr<QList>`).
- **`cwPassageTubeManager`** (new, `cwUpdatable`), registered with
  `UpdateCoordinator->add(PassageTubeManager, {LinePlotManager})`
  (`cwRootData.cpp:202-207`). Inputs: the region snapshot (reuse
  `cwLinePlotTask::buildInput`-style capture), each cave's
  `stationPositionLookup()`, and splay tips. Runs the builder through an
  `AsyncFuture::Restarter` under `cwConcurrent::run`, delivers via
  `observe(...).context(this, …)` into `cwRenderPassageTube::setMesh`.
  Triggers: `cwLinePlotManager::stationPositionInCavesChanged` (positions,
  LRUD, splays all route through a solve) **and** the scrap signals
  `cwScrapManager` already listens to (`cwScrapManager.cpp:618-645`:
  outline point edits, note station edits, view-matrix and note-transform
  changes), debounced, so dragging an outline point updates the tube without
  re-running cavern.
- **Splay tips:** add `cwCave::setSplayTips` / `splayTips()` next to the
  position lookup (`cwCave.h:128-135`) and populate it in
  `cwLinePlotManager::updateLinePlot` from `LinePlotCaveData::splayTips()`.
  Small, and it also unblocks future splay-based morphing.

Why a separate manager rather than building inside `cwLinePlotTask`: the
line-plot worker already has everything the builder needs, but it only runs
on survey edits, and re-running cavern on every scrap outline drag would be
wasteful. The scrap manager has the same relationship to the line plot and is
the model to copy.

### 4.2 Render object

- **`cwRenderPassageTube`** (`cwRenderObject` subclass, `QML_NAMED_ELEMENT`):
  `setMesh(cwPassageTubeMesh)` into `cwTracked<GeometryState>`; color/opacity
  in `cwTracked<RenderState>`; `setRangeVisible(start, count, visible)` over
  index ranges with a published mask (copy `cwRenderLinePlot::setRangeVisible`),
  `updateVisibility()` override to republish. No `registerPickable` (§1.5),
  so no pick gate. Created in `cwRegionSceneManager` right after the line
  plot so it draws before scraps.
- **`cwRHIPassageTube`** (`cwRHIObject` subclass): `initialize` with
  `buildRhiInputLayout` (Position + Normal), `synchronize` copies the
  trackers, `updateResources` uploads immutable vertex/index buffers, a small
  std140 uniform block (color, opacity, light direction), the visibility
  mask gated on `entryVersion`, and `cwLedgeredBytes` under a new
  `Category::PassageTube`. `gather` in the Opaque pass (Transparent with
  `depthWrite=false` + Alpha blend once a phase-2 junction mode exists),
  `ensurePipeline` keyed on `context.renderData`, `globalCameraBinding = 0`,
  `worldBounds()` = mesh AABB. Destructor deletes buffers and SRB.
- **`PassageTube.vert/.frag`**: `GlobalBlock` at binding 0 copied from
  `LinePlot.vert:25-31`, tube block at binding 1, view-space headlight
  (`normalize((view * vec4(normal,0)).xyz) · (0,0,1)` with an ambient floor,
  like aven's `0.7 + 0.3·dot`). Phase 0 computes the normal in the fragment
  shader as `normalize(cross(dFdx(viewPos), dFdy(viewPos)))` (flat shading,
  needs the `"150,300es"` `qt_add_shaders` block), flipped by
  `gl_FrontFacing` so the inside of the tube lights correctly, and the
  pipeline sets `CullMode::None` because the camera is routinely inside the
  cave (§3.6); a Normal attribute path is added with the smooth option.
  Register both in
  `cavewherelib/CMakeLists.txt:418-450`. For inspecting the generated mesh,
  a second pipeline with `QRhiGraphicsPipeline::PolygonMode::Line` gives a
  wireframe overlay on the desktop backends (unsupported on GLES, so gate it
  on `QRhi::isFeatureSupported(QRhi::NonFillPolygonMode)`).

### 4.3 Visibility and settings

- Per-trip keyword item `Type="Passage"` created in the tube manager the way
  `cwLinePlotManager::reconcileTripKeywordItems` creates `"Splays"`
  (`cwLinePlotManager.cpp:378-397`, `cwTrip.cpp:540-548`), with a
  `cwVisibilityProxy` subclass that calls `setRangeVisible` on the tube.
  This makes tubes filterable by cave/trip/date alongside the line plot, and
  it is how tubes and scraps coexist: they are independent layers the user
  toggles (decision in §5), with no special-casing in the builder.
- A whole-object toggle ("Passage Tubes" in the View menu next to "Scraps
  Visible", `qml/FileMenu.qml:157-165`) bound to `cwRenderObject::visible`.
- **Tube settings** (decision in §5): a persisted settings object under
  `cwSettings` (`cwSettings.h:27-33`), either new properties on
  `cwRenderingSettings` or a sibling `cwPassageTubeSettings` with the same
  `kDefault*` + QSettings pattern (`cwRenderingSettings.cpp:22-30`), exposed
  on `qml/RenderingSettingsItem.qml`. Properties: sample spacing along shots
  (meters), ring resolution (bins), and later the blend angles, default
  radius and subdivide-bend angle. A change triggers a tube rebuild only,
  never a cavern run.
- Export (decision in §5): the tube is an ordinary keyword layer, so map
  export shows exactly what the keyword visibility shows. The user hides the
  tube from an export by hiding it in the layering, the same as any trip or
  scrap; `captureHiddenObjectIds()` stays untouched.

### 4.4 Tests

- `test_cwPassageTubeBuilder.cpp`: straight horizontal leg with LRUD → ring
  vertices at `P ± r·right ± u·up`; 90° bend → bisector frame and
  subdivision kicks in above threshold; vertical shot between two horizontal
  legs → `up` continuous, pit ring horizontal, no NaN; chain assignment at a
  3-way and 4-way junction (one through chain, correct arm count); L/R swap
  when a chain traverses a chunk backwards; plan-scrap ray cast on a
  rectangle outline returns the drawn half-widths; profile-scrap ray cast
  returns the drawn height; splay binning with an outlier; missing LRUD falls
  back to neighbor ring then default radius. From the artifact checklist
  (§3.6): a pit entered and left on different bearings keeps
  `right_i · right_{i−1} > 0` at every sample (no twist); a tight bend with a
  wide ring produces no vertex behind the previous ring's plane (no fold);
  hiding one trip of a two-trip chain leaves a capped end; `0 0 0 0` LRUD
  falls back and a single zero gives `kMinRadius`; a 0.3 m shot with 1 m
  spacing emits two distinct rings; a scrap that closes across the passage
  gives no pinched ring at its end; a scrap ending mid-chain produces a
  cross-faded transition and a `SourceChange` warning.
- Junctions (`test_cwPassageTubeJunction.cpp`): a symmetric T, a trench
  floor, a tall arm, a pit onto a floor, a passage into a shaft, a four-way,
  a loop junction, overlapping mouths (fan-cap fallback) and an arm inside
  the through each give a closed, consistently wound mesh with the mouth on
  the through surface and the union volume within 3 %.
- Algorithmic artifact detection (`cwPassageTubeMeshCheck`): closed edges,
  winding, non-manifold vertices, signed volume, degenerate and sliver
  triangles, triangle–triangle self-intersection on a grid hash, nested
  shells, creases, radius jumps, frame twist, fold-back, and LRUD wall
  points on the surface. Every geometric builder test asserts an empty
  report. `cwPassageTubeCases` generates the 18 gallery cases, the Phake
  survey, a parameter sweep (plan angle × profile angle × size ratio × arm
  count × through bend × section change, ~2,500 cases, `[Sweep]`, nightly)
  and seeded random trees; the `cavewhere-tube-gallery` executable renders any case with the
  real builder, shows the report and grabs contact sheets.
- `test_cwPassageTubeManager.cpp`: load a small region, wait for the line
  plot and tube; edit a scrap outline point → tube rebuilds without a new
  cavern run (observe `cwLinePlotManager` state); keyword visibility hides a
  trip's index range.
- Front-end render tests like `test_cwRenderPointCloud.cpp` (trackers,
  `createRHIObject` type) and the registry sync test through
  `CwRhiSceneTestAccess`; a QML screenshot in `tst_ManualScreenshots.qml`
  once the look is settled.

## 5. Decisions and open questions

Decided (October 2026):

1. **Pit LRUD: U/D only.** On a near-vertical shot L/R have no usable
   direction and U/D are distances along the pit axis. LRUD therefore gives
   no ring samples in the pit regime (weight `1 − w`); the ring comes from
   splays, a plan scrap, or the fallback chain, and U/D set the cap offset at
   a pit dead end (§3.1, §3.2.1, §3.2.4).
2. **Opaque.** CaveWhere draws nothing translucent, and the tube keeps to
   that. Translucency is dropped from phase 4; junction hulls remain only as
   a cutaway/export quality option.
3. **Scraps and tubes are independent layers.** A plan scrap is a sheet at
   station height inside the passage, so an opaque tube hides it from
   outside. Rather than skipping tubes where scraps exist (Therion's
   `walls auto`), both are keyword-filtered layers the user toggles (§4.3).
   Skipping can be added later as an option.
4. **Faceted first.** Phase 0 flat-shades from derivatives with a
   position-only vertex layout so the generated geometry is visible as
   built; a wireframe overlay is the debugging aid; smooth normals come later
   as an option (§3.3.2, §4.2).
5. **Sampling is a setting.** Sample spacing along shots and ring resolution
   live in persisted CaveWhere settings on the rendering settings page; a
   change rebuilds the tube without a solve (§4.3). Defaults:
   `kDefaultSampleSpacing = 1.0` m and `kDefaultRingResolution = 16`.
6. **Map export follows the keywords.** Tubes carry keywords (per-trip
   `Type="Passage"` items, §4.3) and are shown or hidden through the
   layering like everything else; export renders whatever the keyword
   visibility shows, with no tube-specific entry in
   `captureHiddenObjectIds()`.
7. **Scrap lobes: clamp, then walk.** Option A (clamp + `ScrapOpening`
   warning) lands with scraps in phase 2; option B (walk each opening into a
   synthetic chain joined at a synthetic junction) follows in phase 2.5
   (§3.5.3).

Open:

- **Synthetic chain appearance.** Whether a walked lobe's inherited height
  is tinted or drawn like measured passage, and the `kMinMouthWidth` /
  `kMinLobeDepth` thresholds that separate "side passage" from "wide spot".
  Pick in phase 2.5 and tune by eye.

## 6. Phases

- **Phase 0 — LRUD tubes, aven-parity and better.** Builder (edges, chains,
  blended frames, LRUD ellipse rings, bisector joins, bend subdivision, dome
  caps at junction arms, fan caps at dead ends with U/D offsets in pits and
  at trip boundaries, angle unwrap and twist distribution, per-vertex
  fold-back fix, ring merge below `kMinRingSpacing`, `0 0 0 0` LRUD as
  absent with `kMinRadius`/`kMaxRadius` guards, shorter-diagonal quad
  split), manager, render pair, flat-shaded two-sided shader without
  culling, wireframe overlay, View-menu toggle, tube settings with sample
  spacing and ring resolution. Builder unit tests. Visible deliverable on
  every existing project with LRUD.
- **Phase 1 — Splays.** Keep splay tips on `cwCave`; projected-tip samples;
  binning and outlier guard; tests with synthetic splays.
- **Phase 2 — Scraps.** Page-space ray casts for plan, running-profile and
  projected-profile scraps; mid-shot sampling; opening clamp with
  `ScrapOpening` warnings (§3.5.3 A); closing-edge rejection and source
  cross-fade (§3.6); rebuild on scrap edits without a cavern run; tests with
  rectangular outlines.
- **Phase 2.5 — Scrap lobes.** Walk detected openings into synthetic chains
  (§3.5.3 B) behind a `followScrapLobes` setting; inherited height, tinted
  as synthetic; bay / bypass / self-intersection tests.
- **Phase 3 — Integration polish.** Per-trip `"Passage"` keyword visibility,
  memory ledger category, culling bounds, export check (keyword-hidden tubes
  stay out of exports), warnings surfaced
  (stations with no passage data), manual screenshot.
- **Phase 4 — Junction geometry (optional).** Convex-hull junction patches or
  SDF + marching cubes watertight mode, for cutaways and solid export; smooth
  normals as a shading option.
- **Later:** read `img_XSECT` from the `.3d` so externally included `.svx`
  passage data renders too; splay-based scrap morphing can reuse the same
  per-station samples.

## Sources

Local:

- Survex aven tube skinning: `survex/src/gfxcore.cc:3903-4106` (`SkinPassage`),
  `survex/src/model.cc:244-305, 465-650`, `survex/src/img.h:63-91, 145`,
  `survex/doc/datafile.rst:912-965`.

Cave software:

- Therion core walls `thdb1d::get_3d_walls`: <https://github.com/therion/therion/blob/master/src/therion-core/thdb1d.cxx>;
  `vthreshold` default: <https://github.com/therion/therion/blob/master/src/therion-core/thdata.cxx>;
  Therion Book on LRUD orientation and `walls auto`: <https://github.com/therion/therion/blob/master/thbook/ch02.tex>;
  3D page: <https://therion.speleo.sk/3D.php>
- Loch tubes: <https://github.com/therion/therion/blob/master/loch/lxLRUD.cxx>,
  <https://github.com/therion/therion/blob/master/loch/lxLRUD.h>
- Therion 3D from scraps: <https://github.com/therion/therion/blob/master/src/therion-core/thscrap.cxx>,
  <https://github.com/therion/therion/blob/master/src/therion-core/thscrapis.cxx>
- Survex manual `*data passage`: <https://survex.com/docs/manual/datafile.htm>;
  mailing list on tubes and splays: <https://lists.survex.com/pipermail/survex/2022q1/002353.html>,
  <https://lists.survex.com/pipermail/survex/2025q1/002480.html>
- CaveView.js cross-sections: <https://github.com/aardgoose/CaveView.js/blob/dev/src/js/viewer/walls/buildCrossSections.js>
- Compass passage modeling: <https://www.fountainware.com/compass/Documents/pasmodel.doc>,
  <https://fountainware.com/compass/HTML_Help/Compass_Viewer/3dmodelingpage.htm>,
  <https://fountainware.com/compass/HTML_Help/Compass_Viewer/modelpitfalls.htm>,
  <https://fountainware.com/compass/HTML_Help/Compass_Viewer/modelexpand.htm>
- TopoDroid / Cave3D walls: <https://github.com/marcocorvi/topodroid/blob/master/assets/man/page_3d_walls.htm>,
  <https://github.com/marcocorvi/topodroid/blob/master/assets/man/page_walls.htm>
- CaveConverter LRUD from splays: <https://manpages.debian.org/jessie/caveconverter/caveconverter.1.en.html>

Geometry:

- Wang, Jüttler, Zheng, Liu, "Computation of rotation minimizing frames,"
  ACM TOG 27(1), 2008: <https://dl.acm.org/doi/10.1145/1330511.1330513>
  (preprint: <https://i.cs.hku.hk/GraphicsGroup/publications/pdf/Computation%20of%20rotation%20minimizing%20frames.pdf>)
- Hanson & Ma, "Parallel Transport Approach to Curve Framing," 1995:
  <https://scholarworks.iu.edu/dspace/items/39fbe931-c2c8-43a1-9c9d-a207b92d551e/full>
- Blender Skin modifier (frame propagation, branch hulls):
  <https://github.com/blender/blender/blob/main/source/blender/modifiers/intern/MOD_skin.cc>,
  <https://docs.blender.org/manual/en/2.93/modeling/modifiers/generate/skin.html>
- Ji, Liu, Wang, "B-Mesh: A Modeling System for Base Meshes of 3D Articulated
  Shapes," CGF 29(7), 2010: <https://diglib.eg.org/handle/10.2312/CGF.v29i7pp2169-2177>
- Bærentzen, Misztal, Welnicka, "Converting skeletal structures to quad
  dominant meshes" (SQM), 2012: <https://people.compute.dtu.dk/janba/Research_in_Graphics/SQM.html>
- Bloomenthal & Shoemake, "Convolution surfaces," SIGGRAPH 1991; Bloomenthal,
  "Bulge elimination in convolution surfaces," CGF 1997:
  <https://unchainedgeometry.com/JBloom/pdf/bulge-elim.pdf>
- Zanni et al., "SCALIS: scale-invariant integral surfaces," CGF 32(8), 2013:
  <https://hal.archives-ouvertes.fr/hal-00863523>
- Vessel reconstruction via SDF union + marching cubes:
  <https://arxiv.org/abs/2402.12797>
