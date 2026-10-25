# Sprite billboards: one instanced draw for every lead marker

Issue: https://github.com/Cavewhere/cavewhere/issues/714 (follow-up to commit
`a8045347`, "Draw station labels with one scene-graph renderer")

Supersedes `plans/BILLBOARD_COMPOSITOR_PLAN.md` for the lead side. The
compositor kept one scene-graph draw and one offscreen layer per lead; this
plan draws every lead with one instanced RHI draw and one shared texture.

## Context

Every lead marker is the same 20 by 20 pixel question mark
(`cavewherelib/icons/question.png`, declared once per lead in `LeadPoint.qml`).
Today each lead is a `cwRenderBillboards` billboard: a referenced QQuickItem
subtree, one `cwQuickItemSubscene`, and on the render thread one
`cwItem2DRenderer` whose `QSGBatchRenderer` pass draws the marker's image node
(`cwRenderBillboards.cpp:21-164`). That is one renderer prepare, one render
pass and one depth post-pass per lead per frame.

Two problems follow from that shape:

- **Blending.** Every `RenderMode3D` pass ends with a depth post-pass that writes
  depth over each element's full quad with color writes off
  (`qsgbatchrenderer.cpp:4015-4032`). The lead layer draws before the text layer
  (`cwScene::textBillboardLayer()` registers it first), so a station label
  behind a lead loses every pixel under the marker's rectangle, transparent
  corners included, instead of showing through them.
- **Scale.** The scene graph has no instancing. Each billboard carries a general
  3D matrix, so its batch stays unmerged and costs one draw. At about 16 µs per
  draw on Metal, ten thousand leads would spend 160 ms per frame on draw
  encoding alone.

The RHI side of CaveWhere already instances: `cwRHILinePlot` draws every
centerline segment as one 4-vertex TriangleStrip expanded per instance in the
vertex shader from `gl_VertexIndex`, with all vertex bindings stepping per
instance (`cwRHILinePlot.cpp:83-96`, `LinePlot.vert`). The camera, device pixel
ratio and viewport size come from the shared global UBO slot
(`cwRhiFrameRenderer::GlobalUniform`), so the same draw renders correctly for
the live frame and for offscreen export tiles.

## Goal

All lead markers draw in one instanced draw that samples one texture. A label
behind a lead shows through the marker's transparent pixels, a label in front
of a lead paints over it, and both stay occluded by cave geometry. Lead content
is still declared in QML, once. The per-lead `LeadPoint` keeps its tap target,
its popup and `cwLeadView::isOccluded` unchanged. Completed leads render at the
0.6 opacity QML asks for, which the item layer dropped.

## Why one texture across leads is cheap and one draw is not, in the scene graph

A `QSGTexture` is a reference; any number of `QSGImageNode`s may share one. The
compositor plan could have keyed its layers by content item and reached one
layer texture for ten thousand image nodes. What it could never share is the
draw: `QSGBatchRenderer` merges geometry only for translate-only matrices
(it bakes x/y and drops z), and a camera-facing billboard needs a general
matrix. One node, one draw. The draw count is the cost that matters at ten
thousand leads, so the markers leave the scene graph and become a
`cwRHIObject` with `instanceCount = leads`.

## Design

### The rule

One shared sprite texture, one instance buffer, one draw. The marker's look is
still a QML item: it is rendered once into a texture by a `QSGLayer`, exactly as
`layer.enabled` does for any item, and that texture is sampled by CaveWhere's
own instanced pipeline.

```
cwScene
├── cwRenderBillboards          QQuickItem billboards (unchanged; kept for unique items, no producer today)
├── cwRenderSpriteBillboards    GUI store: one sprite prototype + N {position, bias, opacity, visible}
│     └── cwRHISpriteBillboards (render thread)
│           ├── cwQuickItemLayerTexture   QSGLayer wrapper -> QRhiTexture of the prototype
│           ├── instance vertex buffer    N * {vec3 world, float bias, float opacity}
│           ├── SpriteBlock UBO           sprite size in logical px, size mode
│           └── one NonIndexed drawable   4 vertices x N instances, global camera UBO slot
└── cwRenderTextBillboards      station labels (unchanged), registered after the sprite layer
```

### `cwRenderSpriteBillboards` (new, GUI thread, `cwRenderObject`)

```cpp
class CAVEWHERE_LIB_EXPORT cwRenderSpriteBillboards : public cwRenderObject
{
    Q_OBJECT
public:
    struct SpriteBillboard {
        QVector3D worldPosition;
        float depthBias = 0.0f;
        float opacity = 1.0f;
        bool visible = true;
    };

    // GPU layout of one instance; uploaded verbatim as the per-instance buffer.
    struct Instance {
        QVector3D worldPosition;
        float depthBias;
        float opacity;
    };

    explicit cwRenderSpriteBillboards(QObject* parent = nullptr);
    ~cwRenderSpriteBillboards() override;

    // The window whose scene graph renders the prototype; all billboards share it.
    void setWindow(QQuickWindow* window);
    QQuickWindow* window() const;

    // The one QML item every billboard draws. Referenced into the window's scene
    // graph (cwQuickItemSubscene, hidden from the 2D tree) and rendered to the
    // shared texture. Its width/height in logical pixels is the sprite size.
    void setSprite(QQuickItem* prototype);
    QQuickItem* sprite() const;
    QSizeF spriteSize() const;

    void setSizeMode(cwBillboardSizeMode sizeMode);   // ScreenConstant by default
    cwBillboardSizeMode sizeMode() const;

    cwBillboardId addBillboard(const SpriteBillboard& billboard);
    void updateBillboard(cwBillboardId id, const SpriteBillboard& billboard);  // early-outs when equal
    void removeBillboard(cwBillboardId id);
    bool hasBillboard(cwBillboardId id) const;
    int billboardCount() const;

    // The visible billboards as the render thread uploads them. Called from
    // synchronize() while the GUI thread is blocked.
    QVector<Instance> buildInstances() const;
    quintptr spriteRootNodeHandle() const;   // 0 until the prototype's nodes exist

protected:
    cwRHIObject* createRHIObject() override;

private:
    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_sprite;
    std::unique_ptr<cwQuickItemSubscene> m_spriteSubscene;
    cwBillboardSizeMode m_sizeMode = cwBillboardSizeMode::ScreenConstant;
    std::map<cwBillboardId, SpriteBillboard> m_billboards;  // ordered: stable instance order
    uint32_t m_nextId = 1;
};

using cwSpriteBillboardHandle =
    cwLayerBillboardHandle<cwRenderSpriteBillboards, cwRenderSpriteBillboards::SpriteBillboard>;
```

`cwSpriteBillboardHandle` lives in a new `cwSpriteBillboardHandle.h`, mirroring
`cwTextBillboardHandle.h`. The handle template needs only add/update/remove, so
it compiles unchanged.

A hidden billboard stays in the store with `visible = false` and is left out of
`buildInstances()`. That keeps a keyword filter toggle to one `update()` instead
of ten thousand removes and re-adds.

### `cwQuickItemLayerTexture` (new, render thread, private-API wrapper)

Wraps a `QSGLayer` (`qsgadaptationlayer_p.h`, created with
`QQuickWindowPrivate::get(window)->context->sceneGraphContext()->createLayer(renderContext)`).
It becomes the third file that touches Qt private API; update the "one of only
two files" comments in `cwQuickItemSubscene.h:12` and `cwItem2DRenderer.h:8`.

```cpp
class cwQuickItemLayerTexture
{
public:
    explicit cwQuickItemLayerTexture(QQuickWindow* window);
    ~cwQuickItemLayerTexture();   // deletes the layer; must run before the scene graph invalidates

    void setRootNodeHandle(quintptr rootNodeHandle);         // QSGLayer::setItem
    void setContentSize(const QSizeF& logicalSize, float devicePixelRatio);
                                                             // setRect, setSize, setDevicePixelRatio
    // Re-renders the prototype when its subtree changed since the last call,
    // otherwise a no-op. Records its own render pass on the current frame's
    // command buffer, so call it outside any open pass (from gather).
    void updateTexture();
    // The texture of the last updateTexture(). Null before the first one. The
    // pointer changes when the layer resizes, so callers compare it each frame
    // and rebuild their shader resource bindings on a change.
    QRhiTexture* rhiTexture() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
```

Settings fixed in the constructor: `setLive(true)`, `setFormat(RGBA8)`, and
`mirrorVertical` stays at its default `true` (`qsgrhilayer.cpp:18`): `grab()`
builds its projection from that flag so that texture row 0 is the item's top on
every graphics API (`qsgrhilayer.cpp:347-363`), which gives the texture an
Image's orientation, so `uv = corner` samples the top-left corner at `(0, 0)`.
The layer marks itself dirty from the root node's `sceneGraphChanged`
(`qsgrhilayer.cpp:404`) and `grab()` returns immediately when clean
(`qsgrhilayer.cpp:71`), so a static question mark renders once and costs
nothing afterwards. A dirty layer emits `QSGLayer::updateRequested`; the
constructor connects it to the owning `cwRenderObject::update()` (queued, the
receiver is on the GUI thread) so the next frame re-renders the texture, the way
`QQuickShaderEffectSource` does (`qquickshadereffectsource.cpp:201`).

### `cwRHISpriteBillboards` (render thread backend, in `cwRenderSpriteBillboards.cpp`)

- `synchronize`: bind the window (the `sceneGraphInvalidated` DirectConnection
  pattern from `cwRHITextBillboards::bindWindow`, tearing down the layer
  texture, sampler and SRB there). Snapshot `buildInstances()`, `spriteSize()`,
  `sizeMode()`, the window's device pixel ratio and `spriteRootNodeHandle()`.
  Create the `cwQuickItemLayerTexture` when a window and a handle exist; push the
  handle and content size into it.
- `initialize`: dynamic per-instance vertex buffer, `SpriteBlock` uniform
  buffer, `QRhiSampler` (Linear/Linear, ClampToEdge), vertex input layout with
  one PerInstance binding of `sizeof(Instance)` bytes (Float3 at 0, Float at 12,
  Float at 16).
- `updateResources`: upload the instance buffer when the snapshot changed
  (`uploadPerInstance` as in `cwRHILinePlot.cpp:22-32`), upload `SpriteBlock`
  when size or size mode changed.
- `gather` (Overlay only): return false with zero instances or no layer. Call
  `updateTexture()`; if `rhiTexture()` differs from the one bound in the SRB,
  rebuild the SRB. `ensurePipeline` keyed on the pass's render-pass descriptor
  and sample count through `m_pipelines.acquire`, as the line plot does.
  Append one `NonIndexed` drawable: 4 vertices, `instanceCount = instances`,
  `globalCameraBinding = 0`.
- Pipeline state: TriangleStrip, cull None, depth test on, **depth write on**,
  blend enabled premultiplied (`One, OneMinusSrcAlpha`, the format `QSGLayer`
  renders in).
- `precludesAtlasBatching()` stays false: the camera comes from the per-tile UBO
  slot, so atlas tiles render leads with their own cameras. `worldBounds()` stays
  `nullopt` like the other overlays.

Bindings: 0 = global camera UBO with dynamic offset (vertex), 1 = `SpriteBlock`
(vertex), 2 = sampled texture (fragment).

### Shaders (`shaders/SpriteBillboard.vert`, `shaders/SpriteBillboard.frag`)

Add both to the `cavewherelib_shaders_lineplot` `qt_add_shaders` block
(`cavewherelib/CMakeLists.txt:444`): `gl_VertexIndex` needs the same GLSL
150 / 300 es floor the line plot already requires.

Vertex shader, the port of `cw::billboard::buildModelMatrix` with the sight line
done in view space so no matrix inverse is needed:

```glsl
layout(location = 0) in vec3 iWorldPosition;
layout(location = 1) in float iDepthBias;
layout(location = 2) in float iOpacity;
layout(location = 0) out vec2 vUv;
layout(location = 1) out float vOpacity;

layout(std140, binding = 0) uniform GlobalBlock { mat4 viewProjectionMatrix; mat4 viewMatrix;
                                                  mat4 projectionMatrix; float devicePixelRatio;
                                                  vec2 viewportSize; };
layout(std140, binding = 1) uniform SpriteBlock { vec2 spriteSizePx; float sizeMode; float pad; };

const float kSizeModeWorldSized = 0.0;
const float kWorldPerPixel = 0.02;           // mirrors cwBillboardGeometry.cpp
const float kPerspectiveDepthDivide = -0.5;  // mirrors isOrthographic()
const float kMinClipW = 1e-4;
const float kMinProjectionScale = 1e-6;

void main() {
    // Corner from gl_VertexIndex: (0,0) (1,0) (0,1) (1,1); content pixel space is y-down.
    vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
    vec2 offsetPx = (corner - 0.5) * spriteSizePx;
    vUv = corner;
    vOpacity = iOpacity;

    vec4 viewPos = viewMatrix * vec4(iWorldPosition, 1.0);
    bool orthographic = projectionMatrix[2][3] >= kPerspectiveDepthDivide;
    // Toward the viewer: +z in view space under a parallel projection, toward the
    // eye (the view-space origin) under perspective.
    vec3 towardViewer = orthographic ? vec3(0.0, 0.0, 1.0) : normalize(-viewPos.xyz);
    viewPos.xyz += towardViewer * iDepthBias;

    vec2 worldPerPixel;
    if (sizeMode == kSizeModeWorldSized) {
        worldPerPixel = vec2(kWorldPerPixel);
    } else {
        // Screen constant: invert the projection so one content pixel covers one
        // logical screen pixel at the unbiased depth, so the bias changes depth
        // and never size (the view-space form of cwBillboardGeometry.cpp:88-92).
        float w = max(abs((viewProjectionMatrix * vec4(iWorldPosition, 1.0)).w), kMinClipW);
        vec2 logicalViewport = viewportSize / devicePixelRatio;
        vec2 projectionScale = max(abs(vec2(projectionMatrix[0][0], projectionMatrix[1][1])),
                                   vec2(kMinProjectionScale));
        worldPerPixel = 2.0 * w / (projectionScale * logicalViewport);
    }

    // Content pixel space is y-down, view space is y-up.
    viewPos.xy += vec2(offsetPx.x, -offsetPx.y) * worldPerPixel;
    gl_Position = projectionMatrix * viewPos;
}
```

Check the matrix indexing against `cwBillboardGeometry.cpp`'s `projection(3, 2)`
when writing it: GLSL `m[c][r]` is column-major, so the element Qt calls row 3,
column 2 is `projectionMatrix[2][3]`.

Fragment shader:

```glsl
layout(location = 0) in vec2 vUv;
layout(location = 1) in float vOpacity;
layout(location = 0) out vec4 fragColor;
layout(binding = 2) uniform sampler2D sprite;

// Fragments below this alpha are discarded so they write no depth: the depth the
// pass leaves behind follows the marker's silhouette, not its quad. Low enough
// that the anti-aliased rim keeps blending.
const float kAlphaCutoff = 0.05;

void main() {
    vec4 color = texture(sprite, vUv);   // premultiplied
    if (color.a < kAlphaCutoff) discard;
    fragColor = color * vOpacity;
}
```

### Blending with labels, without a shared tree

The sprite layer registers before the text layer, so it draws first in the
Overlay pass with depth write following the silhouette. The text pass then
depth-tests each glyph fragment against that:

| Case | Result |
|---|---|
| label behind a lead, under the marker's transparent corner | fragment passes, label shows through |
| label behind a lead, under the question mark's ink | fragment fails, hidden by the marker |
| label in front of a lead | fragment passes, label paints over the marker |
| lead behind a wall | sprite fragments fail the scene depth test |
| two overlapping leads | per-pixel depth resolves them, no CPU sort |

The text pass's own depth post-pass runs after everything else in the Overlay
pass has drawn, so it clips nothing anymore. The remaining imperfection is the
anti-aliased rim: pixels with alpha between `kAlphaCutoff` and 1 blend and write
depth, so a label behind a lead loses that one-pixel rim. Opacity is applied
after the cutoff, so a 0.6 completed lead still occludes what is behind its ink;
it blends with what drew before it, which at a 1-pixel marker scale is fine.

### Draw order in `cwScene`

Add `cwRenderSpriteBillboards* spriteBillboardLayer()`, lazily created and
parented like the others. `textBillboardLayer()` calls `billboardLayer()` and
`spriteBillboardLayer()` first, so registration order is item layer, sprite
layer, text layer whichever is requested first. Update its order comment.

### `cwBillboardOverlayItem`

- Add `ensureRenderSpriteBillboards()` and `renderSpriteBillboards()` mirroring
  the text pair, and
  `setSpriteBillboard(cwSpriteBillboardHandle&, const QVector3D& worldPosition, float depthBias, float opacity, bool visible)`.
- Fetch the item layer lazily too: `ensureRenderBillboards()` moves from
  `itemChange`/`setScene` into `setBillboard()`. An overlay that never calls
  `setBillboard` then creates no `cwRenderBillboards`, so a scene with only
  leads and labels has no per-item renderers at all.

### QML

`GLTerrainRenderer.qml`, inside the existing `LeadView` block: declare the
prototype once and hand it over.

```qml
LeadView {
    id: leadViewId
    ...
    sprite: leadSprite

    // The one marker every lead draws. Referenced into the 3D pass as a
    // texture, so it is hidden from the 2D tree.
    QQ.Image {
        id: leadSprite
        source: "qrc:/icons/question.png"
    }
    ...
}
```

`LeadPoint.qml`: `questionImage` goes away. `hitTarget` becomes the marker's
stand-in, exposed as `property alias marker: hitTarget`, sized to
`leadView.spriteSize`, with the two bindings that used to live on the image:

```qml
QQ.Item {
    id: hitTarget
    width: pointId.leadView.spriteSize.width
    height: pointId.leadView.spriteSize.height
    anchors.centerIn: parent

    // cwLeadView reads these for the lead's sprite instance; a hidden marker is
    // also untappable. Opacity has no 2D effect on an empty item.
    visible: !lead.completed || pointId.selected
    opacity: lead.completed ? 0.6 : 1.0

    QQ.TapHandler { ... unchanged ... }
}
```

`QuoteBox.pointAtObject` points at `hitTarget`. The occlusion gate and
`selectionManager` routing are untouched.

### `cwLeadView`

- `Q_PROPERTY(QQuickItem* sprite READ sprite WRITE setSprite NOTIFY spriteChanged)`
  and `Q_PROPERTY(QSizeF spriteSize READ spriteSize NOTIFY spriteSizeChanged)`.
  `setSprite` pushes the item into the scene's sprite layer once the layer
  exists; `spriteSize` reads the prototype's width and height and re-emits on
  its `widthChanged`/`heightChanged`.
- `ScrapEntry::billboardHandles` becomes `std::vector<cwSpriteBillboardHandle>`.
- `addOrUpdateBillboard` reads `item->property("marker")`, and calls
  `setSpriteBillboard(handle, world, kLeadDepthBias, marker->opacity(), marker->isVisible())`.
  `isVisible()` is effective visibility, so a keyword-hidden `LeadPoint` hides its
  sprite exactly as `content->isVisible()` did in `buildRenderSlots`.
- `createItem` connects the marker's `opacityChanged` and `visibleChanged` to a
  slot that re-sets that one lead's sprite billboard (function-pointer connect
  on the standard `QQuickItem` signals), replacing the two
  `requestBillboardRender` connections.

## Behavior changes

- Completed, selected leads render at 0.6 opacity. The item layer ignored the
  opacity above the effect root.
- Labels blend correctly against leads in every depth order (the artifact this
  thread started from).
- The per-lead `cwItem2DRenderer`, its per-frame renderer prepare and the
  back-to-front sort are gone for leads. Frame cost for leads becomes one
  instance-buffer upload when something changes plus one draw.
- An offscreen export or thumbnail that shows leads can be atlas-batched again
  as far as leads are concerned; labels still preclude it.
- The lead marker's one-pixel anti-aliased rim clips a label behind it.

## Tests

1. **`test_cwRenderSpriteBillboards.cpp`** (GUI, no GPU): add, update early-out,
   remove, has, count; `buildInstances` leaves hidden billboards out and carries
   position, bias and opacity in instance order; `spriteSize` follows the
   prototype; `setWindow`/`setSprite` recreate the subscene;
   `cwSpriteBillboardHandle` reset removes its billboard.
2. **`test_cwRenderSpriteBillboards_render.cpp`** (GPU window, SKIP under the
   offscreen QPA, modeled on `test_cwRenderTextBillboards_render.cpp`). The
   prototype is a 20 by 20 item whose top half is a solid blue rectangle and
   bottom half transparent, so one sprite tests orientation and the alpha cutoff
   at once. Scene: the red wall quad from the text render test. Probes:
   - sprite in front of the wall: top half blue, bottom half red (transparent
     half discarded, wall visible, orientation right);
   - sprite behind the wall: all red;
   - a station label behind a sprite with its glyphs under the transparent half:
     label color present (the blending fix);
   - a label in front of a sprite: label color over the blue;
   - drawn extent: a pixel just inside the expected `spriteSize` rect is blue,
     just outside is background, under both projections;
   - `scene.renderOffscreen` at a different device pixel ratio: the sprite's
     pixel extent scales with the job's ratio (the global UBO slot carries it);
   - window unbind and rebind drops and rebuilds the layer texture, with
     `RenderWarningCapture` quiet.
3. **`test_cwRenderSpriteBillboards_benchmark.cpp`**: ten thousand billboards,
   `buildInstances` under a millisecond; a render-thread profile line
   `spriteBillboards instances=N gatherUs=..` through the `lcProfileRender`
   hook the text layer uses, read the way
   `test_cwRenderTextBillboards_benchmark.cpp` reads `overlayUs`.
4. **`test_cwScene_billboardLayer.cpp`**: sprite layer registers before the text
   layer whichever is requested first; the item layer is created only when
   requested.
5. **`test_cwLeadView_occlusion.cpp`** and **`tst_Leads.qml`** unchanged in
   expectations; update the comments that name the image billboard. Tap on a
   visible lead selects it, a completed unselected lead is untappable, the
   popup anchors to the marker.
6. **`test_cwBillboardOverlayItem.cpp`**: `setSpriteBillboard` adds then updates;
   an overlay that never calls `setBillboard` leaves the scene without an item
   layer.

## Qt facts the design rests on

- `QSGRhiLayer::rhiTexture()` returns the resolved texture, also with MSAA
  (`qsgrhilayer.cpp:56-58`, resolve target at `:328`). `setSize` releases the
  resources (`:132`) and the next `grab` allocates a fresh `QRhiTexture`
  (`:302`, `:347`), so the pointer changes on resize.
- `grab()` records a pass on `currentFrameCommandBuffer()` (`:465`, `:506`) and
  is a no-op when clean (`:71`); dirtiness comes from the root node's
  `sceneGraphChanged` (`:404`).
- `QSGBatchRenderer` has no instancing and merges only translate-only matrices;
  its `RenderMode3D` depth post-pass is unconditional
  (`qsgbatchrenderer.cpp:4015-4032`).
- `cwRhiFrameRenderer` applies the per-job camera slot as a dynamic offset to any
  drawable with `globalCameraBinding >= 0` (`cwRhiFrameRenderer.cpp:703-730`),
  and `GlobalUniform` carries `devicePixelRatio` and `viewportSize`.
- Gather runs before any render pass opens (`cwRenderBillboards.cpp:136-138`),
  which is where `updateTexture()` must run.

## Risks

- **Layer texture orientation.** Resolved: `mirrorVertical` stays at its default
  `true`, which puts the item's top in texture row 0 on every API
  (`qsgrhilayer.cpp:347-363`); the render test's half-blue prototype pins it.
- **Premultiplied alpha.** The layer renders premultiplied; the blend must be
  `One, OneMinusSrcAlpha` and opacity must scale all four channels, or the rim
  darkens. The render test's blend probe over the red wall catches a wrong
  factor.
- **Shader matrix indexing.** The orthographic test reads one matrix element;
  the render test under both projections catches a transposed index (the bias
  would move the sprite sideways instead of toward the viewer).
- **Teardown order.** The `QSGLayer` must be deleted on the render thread before
  the scene graph invalidates; the `sceneGraphInvalidated` DirectConnection
  already used by the text backend covers it, and the unbind/rebind test checks
  for warnings.
- **Prototype never materializes.** Resolved: the viewer syncs ahead of the
  prototype on the frame that referenced it, so `spriteRootNodeHandle()` calls
  `cwQuickItemSubscene::forceSync()` when the handle is 0 and re-reads it, as
  `QQuickShaderEffectSource::updatePaintNode` does for its source item.

## Verification

```bash
cmake --build build/<preset> --target cavewherelib cavewhere-test cavewhere-qml-test
./build/<preset>/cavewhere-test "[cwRenderSpriteBillboards]" 2>&1 | tee /tmp/sprite-test.log
./build/<preset>/cavewhere-test "[cwScene]" "[cwBillboardOverlayItem]" "[cwLeadView]" 2>&1 | tee /tmp/sprite-related.log
./build/<preset>/cavewhere-qml-test --platform offscreen -input test-qml/tst_Leads.qml 2>&1 | tee /tmp/sprite-qml.log
./build/<preset>/cavewhere-test -d yes 2>&1 | tee /tmp/cavewhere-test.log
./build/<preset>/cavewhere-qml-test --platform offscreen 2>&1 | tee /tmp/cavewhere-qml-test.log
```

Manual: open the large generated cave with leads and labels, orbit so a label
passes behind and in front of a lead, confirm the corner shows the label and
the ink hides it; mark a lead completed and select it, confirm the faded marker;
export a map with leads visible and compare marker size against the viewport.

## Landing order

1. `cwQuickItemLayerTexture` plus `cwRenderSpriteBillboards` store, backend,
   shaders, handle, `cwScene::spriteBillboardLayer()`, tests 1 to 4. Nothing
   uses it yet; the app is unchanged.
2. `cwBillboardOverlayItem` sprite API and lazy item layer, `cwLeadView` and the
   two QML files, tests 5 and 6. Leads move to sprites.
3. Separate commit: remove the lead-specific comments from `cwRenderBillboards`
   and `cwScene::billboardLayer()`, and the `tst_OffscreenAtlasBatch.qml` note
   about leads.

## Deferred follow-ups

- A sprite atlas: several prototypes rendered into one layer, with a per-instance
  uv rect selecting the icon (a different marker per lead state).
- The same shape for station labels: a glyph atlas with instanced quads removes
  the remaining 3 ms of unmerged text draws, the 1 ms child relink and the text
  sort.
- A `WorldSized` consumer; the shader supports it, no producer asks for it.
- Delete `cwRenderBillboards`, `cwQuickItemSubscene` and `cwItem2DRenderer` once
  labels have left the scene graph too.
