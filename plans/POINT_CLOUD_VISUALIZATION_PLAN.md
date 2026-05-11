# Point-Cloud Visualization: EDL + Distance-Attenuated Sizing

## Context

The LAZ point cloud currently renders as flat, uniform light-grey 2-pixel squares
(`cavewherelib/shaders/PointCloud.vert:27` — `gl_PointSize = 2.0 * devicePixelRatio`,
`PointCloud.frag` — constant grey). At any distance/angle this looks like a
fly-screen — no shape perception, no depth cue, no separation between foreground
and background. The user wants the CloudCompare-style look without committing to
per-point coloring yet.

This plan adds two complementary techniques:

1. **Distance-attenuated point size** — vertex-shader change. Points near the
   camera grow, far points shrink. Cheap. Removes the "all dots the same size"
   look that defeats parallax cues.
2. **Eye-Dome Lighting (EDL)** — screen-space post-process used by CloudCompare,
   Potree, and most modern point-cloud viewers. For each pixel, compares its depth
   to the depth of a small neighborhood. Pixels whose depth is greater than
   neighbors' (i.e. they're behind a silhouette edge) get darkened. Produces strong
   shape/edge perception with no per-point data, no normals, no preprocessing.

## Survey: techniques considered, why these two

| Technique | Cost | Quality | Notes |
|-----------|------|---------|-------|
| **Distance-attenuated size** | 1 shader edit | Medium | Picked — trivial and complements EDL |
| **Eye-Dome Lighting (EDL)** | Offscreen target + 1 post-pass | High | Picked — best quality/cost ratio; CloudCompare's choice |
| SSAO | Offscreen target + 1 post-pass | Medium | Skipped — needs normals (which we don't have) or a depth-only variant that mostly degenerates into EDL |
| Splatting (oriented disks) | Per-point normal estimation + new geometry path | Very high | Skipped — needs precomputed normals; significant engineering |
| Inverse-hull outlining | Second draw per point cloud at slightly larger size | Low | Skipped — coarse silhouettes only; EDL strictly better |
| Depth fog | Trivial fragment math | Low | Skipped — cosmetic, doesn't help shape perception |
| MSAA / point smoothing | RHI sample-count change | Low | Optional polish, not part of this plan |

EDL reference: Boucheny / Christian 2009, "Visualization of scientific data on
point clouds." CloudCompare implementation:
https://github.com/CloudCompare/CloudCompare/blob/master/libs/qCC_glWindow/ccGLWindow.cpp
(search `EDLFilter`).

## Design

### Part 0 — Per-pass target + effect chain (extends existing `RenderPass`)

The current `cwRhiScene::render()` runs every object in a single pass against
the swap-chain. There's no concept of "this pass renders to its own offscreen
target and then runs a post-process chain." If we wire EDL straight into
`render()` we'll redo the same surgery for every future technique (SSAO,
bloom, FXAA, etc.) AND we'll have to wrestle EDL not to murder thin lines and
scrap outlines (a screen-wide EDL pass over the line plot turns every line
pixel black because every line pixel is "in front of" the far plane).

The codebase already has the right scaffolding — `cwRHIObject::RenderPass`
(cwRHIObject.h:39-45) is plumbed through `gather()`/`GatherContext` and the
scene already loops a `passOrder` (currently single-element, multi-pass
version commented at cwRhiScene.cpp:180-184). The missing pieces are:

1. A `RenderPass::PointCloud` value so cwRenderPointCloud can opt into a
   distinct pass.
2. A per-pass configuration on the scene side that says "render to this
   target, run these effects after."

#### Pass enum extension

```cpp
// cwRHIObject.h — extend existing enum
enum class RenderPass : int {
    Background = 0,    // radial gradient — was implicit; promote to named pass
    PointCloud,        // NEW — LAZ point clouds, offscreen + EDL
    Opaque,            // line plot, grid, compass, scraps
    Transparent,       // future
    Overlay,           // future
    ShadowMap,         // future
    Count
};
```

Render objects opt into a pass via their existing `gather()` (they already
receive a `GatherContext::renderPass`). Default behavior is unchanged for
Background/Opaque/Transparent/Overlay/ShadowMap — only PointCloud has a
non-default `PassConfig`.

`cwRenderPointCloud` reports `RenderPass::PointCloud` in its gather.
Everything else (line plot, grid, scraps, compass, radial gradient) stays in
the buckets it occupies today.

#### Per-pass config table

```cpp
// cwRhiScene.h
struct PassConfig {
    // Empty target → render directly to swap-chain (default for most passes).
    QRhiTexture* color = nullptr;
    QRhiTexture* depth = nullptr;
    QRhiTextureRenderTarget* target = nullptr;
    QRhiRenderPassDescriptor* rpDesc = nullptr;
    QSize size;

    std::vector<std::unique_ptr<cwRhiPostProcessEffect>> effects;
    bool clearOnBegin = true;
};

class cwRhiPostProcessEffect {
public:
    virtual ~cwRhiPostProcessEffect() = default;
    virtual void initialize(QRhi*, QRhiRenderPassDescriptor* outputRPDesc,
                            QRhiBuffer* globalUBO) = 0;
    virtual void resize(QSize) {}
    // `inputColor`/`inputDepth` are the pass's offscreen attachments (or the
    // previous effect's output for chained effects). `output` is the next
    // stage's target — typically the swap-chain for the last effect.
    virtual void apply(QRhiCommandBuffer* cb,
                       QRhiTexture* inputColor,
                       QRhiTexture* inputDepth,
                       QRhiRenderTarget* output,
                       QSize outputSize) = 0;
};

QHash<cwRHIObject::RenderPass, PassConfig> m_passConfigs;
```

Default `PassConfig{}` = "draw directly to swap-chain, no effects" — every
existing pass behaves identically to today without any per-pass config entry.
Only `m_passConfigs[RenderPass::PointCloud]` gets a non-default entry: an
offscreen color+depth target and `[cwEDLEffect]`.

Per the design-review feedback:
- **No `GlobalUniformContext` parameter** — effects bind the global UBO
  directly at `binding = 0` (passed in via `initialize`).
- **Single-effect chains only for MVP** — chains with multiple effects need
  ping-pong intermediate textures, which we don't need yet. Add a runtime
  assert if a chain longer than 1 is registered; revisit when SSAO+EDL
  composes.
- **Cache-eviction on offscreen destruction** — when a `PassConfig`'s
  `rpDesc` is destroyed (on resize), iterate `m_pipelineCache` and evict any
  `PipelineRecord` whose `key.renderPass` matches the freed descriptor.
  This avoids dangling pointers in cached keys.

#### Render loop

`cwRhiScene::render()` becomes:

```cpp
const std::array<RenderPass, 5> passOrder = {
    RenderPass::Background, RenderPass::PointCloud,
    RenderPass::Opaque, RenderPass::Transparent, RenderPass::Overlay,
};

for (RenderPass pass : passOrder) {
    const PassConfig& cfg = m_passConfigs.value(pass);  // default → swap-chain
    auto* targetRT = cfg.target ? cfg.target : renderer->renderTarget();
    auto* targetRPDesc = cfg.rpDesc ? cfg.rpDesc
                                    : renderer->renderTarget()->renderPassDescriptor();

    cb->beginPass(targetRT, clearColorFor(pass), {1.0f, 0}, resources);
    drawBatchesForPass(pass, ...);          // existing per-pass gather/batch loop
    cb->endPass();

    // Effect chain. For single-effect MVP, the effect renders directly into
    // the next stage's target — typically the swap-chain.
    runEffectChain(cfg, cb, renderer->renderTarget(),
                   renderer->renderTarget()->pixelSize());
}
```

Batches built by `gather()` are already partitioned by `RenderPass` value
(`GatherContext::renderPass`) — no new partitioning code is needed. The
existing `passBatches` array in `cwRhiScene::render()` (cwRhiScene.cpp:175-177)
already indexes by `RenderPass`; we just expand `passOrder` and check
`m_passConfigs` for each pass.

#### renderPassDescriptor plumbing

Each render-object back-end creates its pipeline against a specific
`QRhiRenderPassDescriptor*` (keyed in the pipeline cache via
`cwRhiPipelineKey::renderPass`). Today they all reach into
`renderer->renderTarget()->renderPassDescriptor()` — which is wrong once
some passes target an offscreen `rpDesc`.

Fix: add `QRhiRenderPassDescriptor* renderPassDescriptor` to **both**
`cwRHIObject::RenderData` and `cwRHIObject::ResourceUpdateData` (pipeline
creation happens inside `initialize()` / `updateResources()`, which receive
`ResourceUpdateData`; the runtime draw path uses `RenderData`). cwRhiScene
populates the field with the active pass's descriptor — `cfg.rpDesc` for
offscreen passes, swap-chain descriptor otherwise.

Back-ends touched (just swap one expression):
- `cwRHIPointCloud` (sees the PointCloud-pass offscreen descriptor)
- `cwRHILinePlot`, `cwRHIGridPlane` (see the Opaque-pass offscreen descriptor)
- `cwRhiRadialGradient` (continues to see swap-chain descriptor)

#### Depth-correct compositing — Opaque also gets an offscreen target

EDL alone is a per-pass effect; *compositing the EDL-shaded point cloud with
the rest of the scene* is cross-pass and needs depth from both sides. The
swap-chain's depth attachment is a `QRhiRenderBuffer` (not sampleable), so we
can't read line-plot/grid depth from there. Solution: give the Opaque pass
its own offscreen color+depth target with sampleable depth, identical in
shape to the PointCloud pass's target.

After both passes complete, `cwEDLEffect` runs once as the compositor — it
reads four textures (Opaque color+depth, PointCloud color+depth), applies the
EDL response to the PointCloud color using PointCloud depth + neighbor
depths, then resolves the per-pixel winner against Opaque depth and writes to
the swap-chain over the Background that's already there.

```
Pass order:
  Background    → swap-chain (radial gradient, far-plane depth, no offscreen)
  Opaque        → m_passConfigs[Opaque].target (color + sampleable depth)
  PointCloud    → m_passConfigs[PointCloud].target (color + sampleable depth)
  → cwEDLEffect samples all four, writes composite to swap-chain
```

Per-pixel composite logic (in `EDL.frag`):

```glsl
vec4 op = texture(uOpaqueColor, uv);
float odepth = texture(uOpaqueDepth, uv).r;
vec4 pc = texture(uPCColor, uv);
float pdepth = texture(uPCDepth, uv).r;

float shade = edlResponse(uv, pdepth);    // 8-tap response over uPCDepth
vec3 pcShaded = pc.rgb * shade;

// Front-to-back: PC wins if it has content AND is in front of Opaque.
bool pcHasContent = pc.a > 0.0;
bool opHasContent = op.a > 0.0;
bool pcInFront    = pdepth < odepth;

if (pcHasContent && (!opHasContent || pcInFront)) {
    fragColor = vec4(pcShaded, 1.0);
} else if (opHasContent) {
    fragColor = vec4(op.rgb, 1.0);
} else {
    fragColor = vec4(0.0);           // alpha 0 → Background shows through
}
```

`cwEDLEffect` is therefore not a generic "operates on one pass's output"
post-process — it's the **scene compositor that knows about both opaque and
point-cloud passes**. This is honest: cross-pass compositing inherently
reaches across passes. We don't pretend EDL is plug-and-play; we ship a
real compositor and call it that.

Future cleanly-generic post-process effects (FXAA, tonemap) would apply to
the compositor's *output*, layered after it. Out of scope for this PR.

**Cost**: one extra color+depth texture pair for the Opaque target. At 1440p
RGBA8 + D32F ≈ 22 MB; at 4K ≈ 64 MB. Both well within budget for any
GPU running CaveWhere.

#### Why this is enough and not premature

- One concept (`RenderPass`), one place to look. No parallel enum.
- `PassConfig{}` default keeps every today-pass behaving identically.
- EDL = 1 enum value + 1 `PassConfig` entry + 1 effect class.
- Future SSAO on the point cloud → append another effect to
  `m_passConfigs[PointCloud].effects` once ping-pong support lands.
- Future bloom on a future Overlay pass → `m_passConfigs[Overlay].effects`.
- Future ShadowMap (already in the enum) → its own offscreen `PassConfig`
  plus an outer-driver step in `render()` for the second camera render.
- Buckets-with-config doesn't paint us into a corner if we ever need a true
  framegraph — it's a strict subset of what a framegraph can express, and
  ~200 LOC of well-understood logic that's easy to fold into a framegraph
  executor later.

#### Explicitly NOT in this abstraction

- No multi-target rendering per object (MRT) — not needed yet.
- No multi-effect chains with ping-pong textures yet — assert single effect
  per pass for MVP.
- No effect graph / DAG.
- No per-effect parameters surfaced to QML yet — strength/radius stay
  constants in the EDL shader for this PR.
- No general post-compositor effect slot yet (FXAA / tonemap on the
  composited output). Add when an actual second effect lands.

### Part 1 — Distance-attenuated point size (small, ships first)

Modify `cavewherelib/shaders/PointCloud.vert` so `gl_PointSize` scales with
1/`gl_Position.w` (perspective-correct screen size of a fixed world-space radius):

```glsl
gl_PointSize = clamp(
    basePointSize * devicePixelRatio * projectionMatrix[1][1] * viewportSize.y * 0.5 / gl_Position.w,
    1.0,
    maxPointSizePx);
```

- `basePointSize` (world-space radius proxy) and `maxPointSizePx` go into a new
  per-layer uniform block bound at the existing `perDrawBinding`, OR we keep them
  as hard-coded constants in this PR and migrate to a uniform when per-layer
  size becomes user-configurable. Recommended: hard-code for now (constants
  `basePointSize = 0.05`, `maxPointSizePx = 16.0`), tune empirically, leave a
  TODO for the per-layer uniform once the UI sliders land.
- Add `vec2 viewportSize` to the existing `GlobalBlock` (`GlobalUniform` struct
  in `cwRhiScene.h:83-88`) so the shader can compute pixel size correctly.
  Populate it from `renderer->renderTarget()->pixelSize()` each frame; gate the
  write on a new `cwSceneUpdate::Flag::ViewportSize` set whenever the swap-chain
  size changes (the existing camera `devicePixelRatioChanged`/resize path already
  fires when size changes — verify and reuse).

Verify visually: panning toward the cloud now grows the points; pulling back
shrinks them. The donut-hole-when-zoomed-in artifact disappears.

### Part 2 — Eye-Dome Lighting + depth-correct compositor

With the per-pass abstraction from Part 0, EDL ships as a `cwEDLEffect`
that *is* the scene compositor for the Opaque and PointCloud passes. It runs
once after both passes, applies EDL shading to the PointCloud color, and
resolves the per-pixel winner against Opaque depth before writing the final
image over the Background already in the swap-chain.

#### 2a. Per-pass offscreen targets

In `cwRhiScene::initialize` / on resize, build two `PassConfig` entries:

`m_passConfigs[RenderPass::Opaque]`:
- `.color` — RGBA8, `UsedAsTransferSource | RenderTarget`.
- `.depth` — D24S8 or D32F (sampler-readable). Probe with
  `rhi->isTextureFormatSupported`; fall back to D24S8.
- `.target` — `QRhiTextureRenderTarget` with both attachments.
- `.rpDesc` — `newCompatibleRenderPassDescriptor()`.
- `.effects` — empty (the cross-pass compositor lives elsewhere; see 2d).

`m_passConfigs[RenderPass::PointCloud]`:
- Same shape as Opaque, separate textures.
- `.effects` — empty as well; the EDL compositor reads from this pass's
  textures but lives at the scene level.

The Background pass keeps the default-empty `PassConfig{}` — radial gradient
writes straight to the swap-chain as the initial paint.

#### 2b. Pass dispatch

Part 0's render loop covers it. Three passes contribute per frame:
Background → swap-chain, Opaque → offscreen, PointCloud → offscreen.

Clear color for the offscreen targets is `(0, 0, 0, 0)` so empty pixels stay
transparent and the compositor can let earlier content show through.

#### 2c. cwEDLEffect compositor + shaders

New class `cwEDLEffect` in `cavewherelib/src/cwEDLEffect.{h,cpp}`. It
implements an EDL-aware compositor — owns its pipeline, SRBs, EDL UBO,
references the shared linear-clamp sampler, and holds back-pointers to the
Opaque and PointCloud `PassConfig`s in cwRhiScene (so it can read live
texture handles at `apply()` time, surviving resizes that recreate textures).

cwRhiScene owns the compositor as a scene-wide member, not an entry in any
pass's `effects` chain — it's a cross-pass operation, not a per-pass effect:

```cpp
// cwRhiScene.h
std::unique_ptr<cwEDLEffect> m_compositor;
```

Its `QRhiShaderResourceBindings` references four sampler bindings (opaque
color/depth + PointCloud color/depth) plus the global UBO + EDL UBO. Use the
existing `sharedLinearClampSampler` helper (`cwRhiScene.cpp:412`) — point
sampling is correct for both color and depth.

Pipeline is created against the swap-chain renderPassDescriptor (stable —
not the offscreen one). Acquired via `cwRhiScene::acquirePipeline` so it
shares the cache and is destroyed with the scene.

Shaders `cavewherelib/shaders/EDL.vert` and `EDL.frag`. The vert is a
standard fullscreen-triangle:

```glsl
// EDL.vert
#version 440 core
layout(location = 0) out vec2 vUV;
void main() {
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vUV = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
```

The frag computes the EDL response over the PointCloud depth, then
depth-resolves against Opaque, then writes:

```glsl
// EDL.frag — pseudocode
#version 440 core
layout(binding = 0) uniform GlobalBlock { ...; vec2 viewportSize; float devicePixelRatio; };
layout(binding = 1) uniform EdlBlock {
    float strength;   // ~1.0–4.0
    float radiusPx;   // ~1.4
};
layout(binding = 2) uniform sampler2D uOpaqueColor;
layout(binding = 3) uniform sampler2D uOpaqueDepth;
layout(binding = 4) uniform sampler2D uPCColor;
layout(binding = 5) uniform sampler2D uPCDepth;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

float edlResponse(vec2 uv, float centerDepth) {
    // Boucheny log-depth formulation, stable across near/far ranges.
    float logD0 = log2(centerDepth + 1e-6);
    vec2 px = (radiusPx * devicePixelRatio) / viewportSize;
    const vec2 dirs[8] = vec2[](
        vec2( 1, 0), vec2(-1, 0), vec2(0,  1), vec2(0, -1),
        vec2( .707, .707), vec2(-.707, .707), vec2( .707,-.707), vec2(-.707,-.707));
    float response = 0.0;
    for (int i = 0; i < 8; ++i) {
        float di = textureLod(uPCDepth, uv + dirs[i] * px, 0).r;
        response += max(0.0, logD0 - log2(di + 1e-6));
    }
    return exp(-response * 0.125 * 300.0 * strength);  // 0.125 = 1/8 averaging
}

void main() {
    vec4 op = textureLod(uOpaqueColor, vUV, 0);
    float odepth = textureLod(uOpaqueDepth, vUV, 0).r;
    vec4 pc = textureLod(uPCColor, vUV, 0);
    float pdepth = textureLod(uPCDepth, vUV, 0).r;

    bool pcHasContent = pc.a > 0.0 && pdepth < 1.0;
    bool opHasContent = op.a > 0.0 && odepth < 1.0;
    bool pcInFront    = pdepth < odepth;

    if (pcHasContent && (!opHasContent || pcInFront)) {
        vec3 shaded = pc.rgb * edlResponse(vUV, pdepth);
        fragColor = vec4(shaded, 1.0);
    } else if (opHasContent) {
        fragColor = vec4(op.rgb, 1.0);
    } else {
        fragColor = vec4(0.0);  // transparent — Background shows through
    }
}
```

Constants are the standard CloudCompare/Potree defaults; tune empirically.

#### 2d. Compositor submission

`cwRhiScene::render()` calls `m_compositor->apply(cb, swapchainRT, size)`
after all three pass-render `beginPass`/`endPass` cycles have completed.
The compositor's `apply()`:

```cpp
cb->beginPass(swapchainRT, QColor(), {1.0f, 0}, nullptr,
              QRhiCommandBuffer::ExternalContent);  // preserve Background
cb->setGraphicsPipeline(m_pipeline);
cb->setShaderResources(m_bindings);
cb->setViewport(QRhiViewport(0, 0, size.width(), size.height()));
cb->draw(3);
cb->endPass();
```

Blend state: standard alpha (`SrcAlpha`, `OneMinusSrcAlpha`) so the
shader's `vec4(0.0)` output lets the Background already in the swap-chain
show through. Depth test disabled, depth write disabled.

The Background pass writes directly to the swap-chain *before* the
compositor runs — verify with the Qt RHI begin-pass content-preservation
mechanism (`QRhiCommandBuffer::ExternalContent` or equivalent). Fallback:
let cwRhiScene begin-pass the swap-chain once at the top of `render()` with
clear, draw Background into it, end-pass; then begin-pass-with-preserve for
the compositor. Verify which form Qt RHI 6.10 actually exposes at
implementation time.

#### 2e. Resize handling

When the swap-chain `outputSize` changes, repeat the teardown for **both**
offscreen pass targets (Opaque and PointCloud):

For each of `m_passConfigs[Opaque]` and `m_passConfigs[PointCloud]`:
- Evict pipeline cache entries: iterate `m_pipelineCache` and `delete`
  any record whose `key.renderPass == cfg.rpDesc`. Without this step the
  cache can hold dangling-pointer keys that hash-collide with freshly-
  allocated descriptors at the same address.
- Destroy `cfg.target` → `cfg.rpDesc` → `cfg.color` / `cfg.depth`.
- Recreate at the new size.

Then call `m_compositor->resize(size)` to rebuild its SRBs against the
freshly-recreated four textures. The compositor pipeline itself stays alive
(bound to the stable swap-chain renderPassDescriptor).

#### 2f. Test seam

We can't easily render-test EDL/the offscreen path end-to-end without a real
RHI device, but the abstraction itself is testable on the CPU side:

- **Pass-dispatch partitioning** (no RHI). Construct mock cwRHIObjects whose
  `gather()` returns drawables tagged with different `RenderPass` values.
  Drive `cwRhiScene::render()` (or extract the batch-collection step into a
  testable helper) and assert that drawables land in the batch vector for
  the pass their object reported, and *only* that pass. Covers the
  invariant "object on RenderPass::PointCloud is never drawn in the Opaque
  bucket."

- **Effect chain ordering** (no RHI). Create a mock `cwRhiPostProcessEffect`
  subclass that appends its identity to a shared log on `apply()`. Register
  two such mocks on a `PassConfig.effects`. Run the effect chain. Assert
  the log matches the declared order. Also asserts the runtime guard that
  rejects chains longer than 1 in MVP fires when expected.

- **PassConfig fallthrough**. Run the render loop with `m_passConfigs`
  containing only `Opaque` and `PointCloud` entries. Assert that other
  passes (Background, Transparent, Overlay) take the default-empty
  `PassConfig` codepath without crashing and target the swap-chain.

- **Pipeline cache eviction on rpDesc destruction**. Insert a fake
  `PipelineRecord` into `m_pipelineCache` whose `key.renderPass` matches a
  `PassConfig.rpDesc`. Trigger the resize teardown for that pass. Assert
  the cache entry is removed *before* the descriptor is destroyed. Verify
  for both Opaque and PointCloud `PassConfig`s.

- **Uniform plumbing** (existing recommendation). Verify `viewportSize` is
  present in `GlobalUniform` at the expected offset and that a flagged
  update lands in the dynamic buffer.

- **QML smoke test** (real RHI, offscreen platform). Create a `cwRhiViewer`
  with the offscreen Qt platform plugin, load a small synthetic LAZ via
  `LazFixtureHelper`, render one frame, assert no warnings/errors are
  logged. (No pixel comparison — RHI output differs per backend.)

The first four are the load-bearing ones; they're cheap, deterministic, and
catch the design's actual invariants. The uniform-plumbing and QML smoke
tests are existing nice-to-haves.

## Critical files

| File | Change |
|------|--------|
| `cavewherelib/shaders/PointCloud.vert` | Switch `gl_PointSize` to perspective-correct distance attenuation; reference new `viewportSize` uniform |
| `cavewherelib/shaders/EDL.vert` (new) | Fullscreen-triangle vertex shader |
| `cavewherelib/shaders/EDL.frag` (new) | 8-tap EDL response, depth-based darkening |
| `cavewherelib/CMakeLists.txt` (around line 354 `qt_add_shaders`) | Register `EDL.vert` and `EDL.frag` |
| `cavewherelib/src/cwRHIObject.h` | Extend `RenderPass` enum with `Background` + `PointCloud` values; declare `cwRhiPostProcessEffect` interface; add `renderPassDescriptor` field to both `RenderData` and `ResourceUpdateData` |
| `cavewherelib/src/cwRHIPointCloud.cpp` | Report `RenderPass::PointCloud` in its `gather()`; read `renderPassDescriptor` from `RenderData`/`ResourceUpdateData` |
| `cavewherelib/src/cwRhiRadialGradient.cpp` | Report `RenderPass::Background` in its `gather()` |
| `cavewherelib/src/cwRHILinePlot.cpp`, `cwRHIGridPlane.cpp` | Read `renderPassDescriptor` from `RenderData`/`ResourceUpdateData` (still report Opaque; now sees offscreen Opaque descriptor) |
| `cavewherelib/src/cwEDLEffect.{h,cpp}` (new) | Scene compositor: reads Opaque + PointCloud color/depth, applies EDL on PointCloud, depth-resolves against Opaque, writes to swap-chain. Owns pipeline, SRBs, EDL UBO; holds back-pointers to both `PassConfig`s |
| `cavewherelib/src/cwRhiScene.h` | Add `viewportSize` to `GlobalUniform`; add `PassConfig` struct and `m_passConfigs` table; add `std::unique_ptr<cwEDLEffect> m_compositor`; declare per-pass effect-chain runner |
| `cavewherelib/src/cwRhiScene.cpp` | Build/rebuild `m_passConfigs[Opaque]` and `m_passConfigs[PointCloud]` offscreen targets on initialize + resize (with pipeline-cache eviction on descriptor destruction); extend `passOrder` to include Background+Opaque+PointCloud; consult `m_passConfigs` per pass; populate `RenderData`/`ResourceUpdateData` `renderPassDescriptor` per pass; invoke `m_compositor->apply()` after pass loop |
| `cavewherelib/src/cwSceneUpdate.h` | Add `Flag::ViewportSize` |
| `cavewherelib/src/cwScene.{h,cpp}` (or `cwCamera`) | Detect viewport size change, set the flag — verify which class currently sets DevicePixelRatio (use the same pathway) |
| `testcases/test_cwRhiSceneDispatch.cpp` (new) | Pass-dispatch partitioning, effect chain ordering, PassConfig fallthrough, pipeline cache eviction tests (no RHI) |
| `testcases/test_cwRhiScene_uniforms.cpp` (new, optional) | Verify `viewportSize` UBO offset / update flag round-trips |

CMakeLists for `testcases/` and `test-qml/` are GLOB-based — no manual registration needed.

## Reuse

- `cwRhiScene::sharedLinearClampSampler` (`cwRhiScene.cpp:412`) — reuse for EDL's depth+color sampling.
- `cwRhiScene::acquirePipeline` (`cwRhiScene.cpp:361`) — register the EDL pipeline through the cache so it's destroyed with the scene; key it on the swap-chain renderPassDescriptor.
- `GlobalBlock` uniform binding (binding 0) — append `viewportSize` to the existing struct; reused by both `PointCloud.vert` and `EDL.frag`.
- Existing per-object render-pass descriptor handling — the pipeline cache already supports rebuilding on descriptor change.
- LASlib LAZ fixtures (`testlib/LazFixtureHelper.{h,cpp}`) for the optional QML smoke test.

## Verification

Build:
```
cmake --build build/Qt_6_11_0_for_macOS-Debug --target CaveWhere cavewhere-test cavewhere-qml-test
```

Optional unit test:
```
./build/Qt_6_11_0_for_macOS-Debug/cavewhere-test "[cwRhiScene]" 2>&1 | tee /tmp/cavewhere-test.log
```

Manual smoke (desktop, required):
1. Launch CaveWhere, new project, add a georeferenced LAZ (e.g.
   `/Users/cave/Downloads/USGS_LPC_WY_FEMA_East_2019_D19_w1145n2342.laz`).
2. Auto-adopt (already shipped) centers the view on the cloud.
3. **Distance attenuation check**: zoom in — points grow (not pixel-locked
   squares). Zoom out — points shrink toward 1 pixel. No flickering at zoom
   extremes.
4. **EDL check**: edges of terrain features (ridges, building rooftops, tree
   crowns) appear darker; flat surfaces and points that occlude empty space
   stay bright. The cloud now reads as a 3D surface rather than confetti.
5. **Resize check**: drag the window to several sizes. No artifacts, no
   warnings in the console, no garbled depth.
6. **Other content unaffected (critical)**: open a cave project that has
   survey lines, scraps, and the grid plane, but no LAZ. Line plot, scraps,
   grid, and compass render *pixel-identical* to before this change. The
   PointCloud layer is empty so its effect chain runs over a fully
   transparent target, contributing nothing to the swap-chain composite.
7. **Mixed scene (depth-correct composite)**: open a project with both
   survey lines AND a LAZ where they geometrically overlap (e.g. surface
   entrance station above the terrain cloud, or a side view that puts cave
   passages behind the cloud). Verify:
   - Survey lines that pass *in front of* point-cloud pixels remain
     visible (lines win the depth resolve).
   - Point-cloud pixels in front of lines occlude the line as expected.
   - Lines stay bright and crisp (EDL does not darken them — EDL response
     is computed against PointCloud depth only).
   - Entrance/surface stations drawn at ground elevation poke through the
     terrain cloud where their depth is closer to camera.
8. **Multiple LAZ layers**: add a second LAZ. EDL respects depth between
   layers (a near layer occludes a far one with edge shading at the seam).

If any of those fail, the most likely culprits are:
- Offscreen depth texture format unsupported on this RHI backend → fall back
  to a different format and re-query.
- Pipelines still bound to the old (swap-chain) render-pass descriptor → the
  pipeline cache miss path isn't running; check that render-object back-ends
  now read the descriptor from `RenderData`.
- EDL response too strong / too weak → tune `strength` constant in shader.
