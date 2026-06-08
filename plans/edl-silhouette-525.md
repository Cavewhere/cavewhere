# Plan: EDL silhouettes against background and opaque scene (issue #525)

Issue: https://github.com/Cavewhere/cavewhere/issues/525 — "EDL renderer needs to
work against background and other objects." We lose the point-cloud silhouette
against the background, scraps, and LiDAR note geometry.

## Goal

The point cloud should show a dark EDL silhouette (a) against the empty
background and (b) against opaque scene geometry — grid, lines, scraps, and
LiDAR note geometry — with the silhouette strength modulated by the real depth
gap (Option B, shared depth). EDL still paints only point-cloud pixels.

## Root cause

EDL samples only the offscreen **cloud-only** color+depth texture, and its
single darkening term darkens the *far* side of a depth break
(`max(0, logZ0 - log2(linDepth(di)))` in `EDL.frag`). At the cloud's outer edge
the cloud is the *near* side, so nothing on the cloud gets darkened. In addition,
opaque geometry (the `Opaque` pass) renders *after* the EDL composite, so EDL is
structurally blind to it. Hence the lost silhouette.

How the frame is built today (`cwRhiScene::draw`):

1. **Offscreen PointCloud pass** -> cloud into its own color + depth textures
   (depth cleared to 1.0).
2. **Swap-chain pass**: Background (radial gradient, writes no depth) -> EDL
   composite (samples only the offscreen cloud color+depth, forwards
   `gl_FragDepth = cloud depth`) -> Opaque (grid, lines, scraps, LiDAR notes).

So at EDL time the only depth anywhere is the cloud's own depth.

## Key design decision: Option B (shared depth)

EDL only paints cloud pixels, so the silhouette must be drawn on the **near
(cloud) side** of the cloud<->scene boundary — unlike Potree/CloudCompare, which
run EDL over the whole framebuffer and darken the far/background side. The fix
adds a near-side silhouette term, gated by a **cloud mask** so the cloud's
internal shading is preserved and only the cloud<->non-cloud boundary gains the
new outline.

Per neighbor, using the combined depth and the `cloudColor.a` mask:

```
gap_i  = log2(linDepth(neighborDepth_i)) - log2(linDepth(centerDepth))
if (cloudNeighbor) response += max(0, -gap_i)   // far-side shape term (unchanged)
else               response += max(0,  gap_i)   // depth-modulated silhouette
```

Behavior:
- Cloud floating in front of distant geometry/background -> big gap -> strong outline.
- Cloud edge flush against same-depth geometry -> gap ~= 0 -> little/no outline.
- Internal cloud shading -> untouched (cloud-cloud pairs only use the old term).

---

## Phase 1 — Background silhouette (shader-only, ships first)

Works on today's architecture; gives an immediate outline against the background
and lays down the EDL math that Phase 2 reuses unchanged.

**File:** `cavewherelib/shaders/EDL.frag` (+ rebuild `.qsb`)

1. In the 8-neighbor loop, classify each neighbor with a **cloud mask** by
   sampling `uColor(neighbor).a`:
   - **cloud neighbor** (`a > kMaskThreshold`) -> existing far-side term
     `max(0, logZ0 - log2(linDepth(di)))` (internal shading, unchanged).
   - **non-cloud neighbor** (`a ~= 0`, i.e. background) -> **silhouette term**
     `max(0, log2(linDepth(di)) - logZ0)`, darkening the cloud *edge*.
2. Clamp the per-neighbor silhouette contribution (background sits at the far
   plane -> a huge raw gap) so the outline saturates cleanly instead of blowing
   to pure black. Add a `silhouetteStrength`/`silhouetteClamp` knob to `EdlBlock`.
3. Keep the center-pixel `discard` for pure-background centers (`d0 >= 0.9999`) —
   we still don't paint outside the cloud; the silhouette lands on cloud-edge
   pixels whose *neighbors* are empty.

**Supporting C++:** `cwEDLEffect` — extend `EdlUniform` with the silhouette
knob(s) and a tunable baseline (mirror the existing `m_strengthBaseline`
pattern). No pipeline/binding changes (color already bound at binding 2 carries
the alpha mask).

**Why this generalizes:** Phase 1 already reads the alpha mask to classify
neighbors. Phase 2 changes *only the depth texture source* (cloud-only ->
combined), so "background neighbor at far depth" becomes the special case of
"non-cloud neighbor at its real depth." The shader math is written once, here.

**Phase 1 verification**
- Extend/model a render test after `test_cwRenderPointCloud.cpp`: render a small
  point cloud offscreen, assert cloud-edge pixels adjacent to background are
  darker than interior pixels, and that interior shading is unchanged vs a
  captured baseline.
- Manual: run the app on a LAZ layer, confirm the outline against background
  matches the desired look in the issue screenshot.
- Build per the project build command; run both test suites offscreen, tee logs.

---

## Phase 2 — Silhouette against opaque scene (render restructure + Option B)

### Architecture change

Reorder so opaque geometry's depth exists before the cloud, and composite at the
end. **Engage this offscreen path only when a point cloud is visible** — when
there's no cloud, keep today's direct-to-swap-chain path so the common case keeps
its MSAA and stays untouched.

```
Cloud visible:
  Pass A  Scene offscreen   Background(gradient) + Opaque(grid, lines, scraps, LiDAR notes)
                            -> sceneColor (RGBA8)  +  sceneDepth (D32F)         [1x sample]
  Pass B  Cloud offscreen   point cloud into { cloudColor, sceneDepth(shared) }
                            target flag PreserveDepthStencilContents (load scene depth,
                            clear cloudColor) -> HW depth test = free occlusion;
                            depth buffer becomes combined cloud+scene depth
  Pass C  Composite->swapchain  EDL reads sceneColor + cloudColor + combinedDepth,
                                outputs final color (no blend, no depth)
```

### EDL math

Option B, masked (see "Key design decision" above). Uses the **combined** depth
and the `cloudColor.a` mask.

### Code changes

- **`cwRhiScene` (`cwRhiScene.cpp/.h`)** — the bulk of the work:
  - Add a **Scene offscreen `PassConfig`** (sceneColor + sceneDepth,
    sampler-readable, 1x), mirroring `ensurePointCloudPass` / `destroyPassConfig`.
  - Make the cloud pass's render target use **`sceneDepth` as a shared depth
    attachment** with `PreserveDepthStencilContents`; clear `cloudColor` only.
  - In `draw()`: branch on cloud presence. Cloud-present -> run Pass A
    (Background+Opaque into scene target), Pass B (cloud, shared depth), Pass C
    (composite to swap-chain). No-cloud -> current path.
  - Route `Background` + `Opaque` gather batches into the scene offscreen target
    (per-pass `renderData.renderPassDescriptor` already supports this).
- **`cwEDLEffect` (`cwEDLEffect.cpp/.h`) + `EDL.frag`**: becomes the **final
  composite**. Add a `sceneColor` sampler (binding 4) to layout/SRB; point the
  depth binding at the shared combined depth; switch the cloud-mask source to
  neighbor `cloudColor.a` (already in place from Phase 1). Output
  `over(sceneColor, cloudColor*shade)` directly — **remove the premultiplied-
  alpha blend and the `gl_FragDepth` forwarding** (Opaque already rendered into
  sceneColor; depth no longer needed downstream).
- **`cwRHIPointCloud` (`cwRHIPointCloud.cpp`)**: confirm pipeline depth-test =
  `Less`, depth-write = on against the shared `sceneDepth` (occlusion). No
  fragment-shader change needed (HW test handles occlusion under Option B).
- **MSAA**: scene + cloud offscreen at **1x** for this phase; composite to the
  (possibly MSAA) swap-chain. Accept temporary AA loss on grid/lines *only while
  a cloud is shown*. MSAA-resolve preservation is a tracked follow-up, not in
  scope here.

### Lifetime / ordering care (call-outs for implementation)

- `sceneDepth` is a depth attachment of **both** the scene target and the cloud
  target — sequential passes, RHI inserts barriers; destroy order: effects ->
  cloud target -> scene target -> textures.
- Rebuild scene + cloud targets together on resize (size-change path like the
  existing `needsRebuild`).
- Pipeline cache keys on rpDesc — evict on rebuild (existing `evictPipelinesFor`).

### Phase 2 verification

- Offscreen render test (model after `test_cwRenderPointCloud.cpp` /
  `test_cwRhiSceneDispatch.cpp`): place a quad/scrap behind a point cloud; assert
  (1) cloud is occluded where geometry is in front, (2) cloud-edge pixels over
  distant geometry are darkened, (3) cloud flush against same-depth geometry shows
  little/no darkening, (4) no-cloud path renders identically to baseline.
- QML: extend `tst_LiDARNotes.qml` to confirm the 3D view still renders and notes
  interact correctly with a cloud present.
- Manual: reproduce the issue scenario (cloud against scraps + LiDAR notes),
  screenshot, compare to issue #525 image.
- Run **both** suites offscreen, tee logs, before pushing.

### Implementation notes (as built)

The plan assumed per-pass `renderData.renderPassDescriptor` was enough to route
Background + Opaque into the offscreen. Two things were missing and had to be
added:

1. **Per-pass MSAA sample count.** The swap chain is 4× MSAA
   (`cw3dRegionViewer::setSampleCount(4)`) but the offscreen is 1×, and every
   RHI object keyed its pipeline `sampleCount` off the swap chain. Added
   `int sampleCount` to `cwRHIObject::RenderData` and a per-frame routing table
   in `cwRhiScene` (`passRenderPassDescriptor` / `passSampleCount`), so each
   pass's pipeline matches the target it actually draws into. This also fixed a
   latent Phase 1 mismatch (cloud pipeline was 4× into a 1× offscreen).

2. **Textured-item pipeline timing.** `cwRhiTexturedItems` built pipelines in
   `updateResources` (only when an item was dirty), keyed on the swap chain — so
   scraps/notes would never have rebuilt for the offscreen when a cloud appears.
   Moved their pipeline validation into `gather()` (self-guarding on the key,
   like the other objects), resolving the target from the scene routing. The SRB
   layout is rpDesc-independent, so it stays valid across a routing rebuild.

`usesPointCloudPass()` (a new `cwRHIObject` virtual) lets `cwRhiScene` detect a
visible cloud *before* gathering, so routing is settled before any pipeline is
built that frame. The old generic `PassConfig` map was replaced by a dedicated
`EdlOffscreen` struct (two color targets sharing one depth texture), since the
shared-depth design doesn't fit one-target-per-pass.

Automated suites cover the no-cloud path; the cloud-composite path (Pass A/B/C)
has no offscreen-readback test and still needs visual verification.

---

## Sequencing & risk

1. **Phase 1** (low risk, isolated to EDL shader + small UBO change) -> review the
   background outline in-app -> land.
2. **Phase 2** (touches the core render loop) -> gated behind cloud-presence so
   non-cloud rendering is unchanged -> review against the issue screenshot -> land.
3. **Follow-up issue**: MSAA preservation for the offscreen scene path.

### Deferred altitude follow-ups (from /simplify review)

- **Generalize pass membership.** `cwRHIObject::usesPointCloudPass()` is a narrow
  predicate read only by the EDL routing. If a second offscreen consumer appears
  (shadow map, another post effect), prefer a general `usesPass(RenderPass)` (or
  letting the scene discover non-empty passes) over adding more `usesXPass()`.
- **Table-drive `render()` pass sequencing.** The cloud and no-cloud branches each
  hand-order the same passes; the begin/endPass walk could be driven off the
  `m_passRpDesc` routing table (start a new pass when the target changes, run the
  composite at the PointCloud->swap-chain boundary). Deferred: restructuring the
  live render loop risks regressing the just-fixed path; do it as its own change.
- **Enforce "semi-transparent objects must not write shared depth."** Today only
  the grid is moved to Transparent + depthWrite-off. Scraps/LiDAR notes are
  `BlendMode::None` (opaque) and correctly write shared depth, so this is not a
  live bug — but the invariant lives only as a comment. A material/pass contract
  (`blendMode != None` => Transparent, no shared-depth write) would prevent a
  future alpha-blended Opaque item from silently swallowing the cloud.

## Relevant files

- `cavewherelib/shaders/EDL.frag`, `cavewherelib/shaders/EDL.vert`
- `cavewherelib/shaders/PointCloud.frag`, `cavewherelib/shaders/PointCloud.vert`
- `cavewherelib/src/cwEDLEffect.cpp/.h`
- `cavewherelib/src/cwRhiScene.cpp/.h`
- `cavewherelib/src/cwRHIPointCloud.cpp/.h`, `cwRenderPointCloud.cpp/.h`
- `cavewherelib/src/cwRhiRadialGradient.cpp` (Background pass)
- `cavewherelib/src/cwRHIObject.h` (RenderPass enum)
- Tests: `testcases/test_cwRenderPointCloud.cpp`,
  `testcases/test_cwRhiSceneDispatch.cpp`, `test-qml/tst_LiDARNotes.qml`
