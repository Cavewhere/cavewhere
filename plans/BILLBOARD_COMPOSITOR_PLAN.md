# Billboard compositor: one scene-graph renderer for leads and station labels

Issue: https://github.com/Cavewhere/cavewhere/issues/714 (follow-up to commit
`a8045347`, "Draw station labels with one scene-graph renderer")

Status: superseded for leads by `plans/SPRITE_BILLBOARDS_PLAN.md`, which draws
every lead marker with one instanced RHI draw instead of one scene-graph node
and one offscreen layer per lead. Kept for the Qt scene-graph findings in
"Qt facts" and as the fallback if a future billboard needs unique QML content.

## Context

Commit `a8045347` split the Overlay-pass billboards into two layers, each with
its own render-thread backend:

| Layer | Content | Renderer passes per frame |
|---|---|---|
| `cwRenderBillboards` (leads) | one QQuickItem subtree per lead | one `cwItem2DRenderer` per lead |
| `cwRenderTextBillboards` (labels) | two `QSGTextNode`s per label | one `cwItem2DRenderer` for all |

`cwScene` registers the lead layer first, so its passes run before the text
pass. Every `QSGBatchRenderer` pass in `RenderMode3D` ends with a depth
post-pass that writes depth over each element's full quad with color writes
off (`qsgbatchrenderer.cpp:4015-4032`, unconditional, no public switch). The
result on screen: a label behind a lead loses every pixel under the lead's
rectangle, including the marker's transparent corners, instead of blending
under the question mark. Before `a8045347` each billboard had its own pass and
all of them were sorted back to front together, so the post-pass of a nearer
billboard always ran after everything behind it had drawn.

Dropping the text layer's own back-to-front sort does not touch this: inside
one pass the alpha batches draw with depth write off, so the sort only sets the
blend order where two labels' glyphs overlap.

Where the remaining render-thread time goes today (one 8 ms `sample` of the
large cave, 95 placed labels): 2 ms in the single text `prepare`, 1 ms
re-linking children after the sort, 3 ms recording unmerged draws.

## Goal

Leads and labels blend correctly in every depth order: a label behind a lead
shows through the marker's transparent pixels, a label in front of a lead
paints over it, and both are still occluded by cave geometry. Leads keep their
QQuickItem content, their tap target and `cwLeadView::isOccluded`. The Overlay
pass runs one scene-graph renderer pass per frame, so the per-lead renderer and
its per-frame Metal buffer churn go away too.

## Why the lead nodes cannot simply join the text tree

Qt parents a referenced item's effect `QSGRootNode` under the item's own
clip/opacity/item node (`qquickwindow.cpp:2284-2288`,
`parent->appendChildNode(root)`), and every later item sync relinks and
eventually deletes it there (`qquickwindow.cpp:2290-2297`). Re-parenting that
root under a CaveWhere-owned root would fight Qt's bookkeeping on every
children update and leak or dangle on teardown. The lead content therefore has
to enter the shared tree as a texture, the way `layer.enabled` and
`ShaderEffectSource` do it.

## Design

### The rule

One render-thread object draws every Overlay billboard. The two existing
layers stay as GUI-side stores with their public APIs unchanged; neither draws
anything on its own anymore.

```
cwScene
├── cwRenderBillboards          GUI store: QQuickItem billboards + subscenes (unchanged API)
├── cwRenderTextBillboards      GUI store: text billboards + layout cache (unchanged API)
└── cwRenderBillboardCompositor cwRenderObject whose backend draws both stores
        └── cwRHIBillboardCompositor (render thread)
              ├── QSGRootNode
              │     ├── "outlines"  text outline nodes, back to front
              │     └── "fills"     text fill nodes and lead image nodes, merged back to front
              ├── one cwQuickItemLayerTexture per lead  (QSGLayer wrapper)
              └── one cwItem2DRenderer
```

### `cwQuickItemLayerTexture` (new, private-API wrapper)

Renders a referenced item subtree into a texture on the render thread. It is
the third and last file allowed to touch Qt private API; update the "one of
only two files" comments in `cwQuickItemSubscene.h:12` and
`cwItem2DRenderer.h:8`.

```cpp
class cwQuickItemLayerTexture
{
public:
    // Creates the layer through the window's scene-graph context
    // (QSGContext::createLayer). Render thread only.
    cwQuickItemLayerTexture(QQuickWindow* window, quintptr rootNodeHandle);
    ~cwQuickItemLayerTexture();   // deletes the QSGLayer; render thread only

    // The content's logical size and the frame's device pixel ratio; the texture
    // is ceil(size * dpr) pixels. Marks the layer dirty only when they change.
    void setContentSize(const QSizeF& logicalSize, float devicePixelRatio);

    // Points the layer at a new effect root (the content item was replaced).
    void setRootNodeHandle(quintptr rootNodeHandle);

    // Re-renders the layer when its content is dirty. Must run outside any open
    // render pass, like cwItem2DRenderer::prepare.
    void updateTexture();

    QSGTexture* texture() const;   // the QSGLayer, owned here

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
```

Settings on the `QSGLayer`: `setItem(root)`, `setRect(0, 0, w, h)`,
`setSize(ceil(w * dpr), ceil(h * dpr))`, `setDevicePixelRatio(dpr)`,
`setLive(true)`, `setFormat(RGBA8)`, `setMirrorVertical(true)` (the
`ShaderEffectSource` default, `qquickshadereffectsource.cpp:161,694`). Dirty
tracking is automatic: `QSGRhiLayer::grab` registers its internal renderer on
the lead's root and connects `sceneGraphChanged` to `markDirtyTexture`
(`qsgrhilayer.cpp:404`), so a static question mark costs nothing after its
first frame. `grab` records into `currentFrameCommandBuffer()`
(`qsgrhilayer.cpp:465,506`); live frames and the offscreen jobs that ride after
them on the same command buffer (`cwRhiScene.cpp:142-148`) both qualify, and
`updateTexture()` runs in `gather`, before `cwRhiScene` opens a pass.

### `cwRenderBillboardCompositor` (new `cwRenderObject`)

GUI side:

```cpp
class CAVEWHERE_LIB_EXPORT cwRenderBillboardCompositor : public cwRenderObject
{
    Q_OBJECT
public:
    explicit cwRenderBillboardCompositor(cwRenderBillboards* items,
                                         cwRenderTextBillboards* text,
                                         QObject* parent = nullptr);
    cwRenderBillboards* itemLayer() const;
    cwRenderTextBillboards* textLayer() const;
protected:
    cwRHIObject* createRHIObject() override;
};
```

Both stores gain one signal, `void changed()`, emitted wherever they call
`update()` today (add/update/remove, `setWindow`, `setStyle`). The compositor
connects both to its own `update()`, so a store change marks the compositor
pending for the next sync. The stores keep calling `update()` too, so
`cwScene::needsRendering` still fires the way the existing tests expect.

The stores' `createRHIObject()` return an empty `cwRHIObject` (`gather` returns
false, `synchronize` does nothing). `cwRhiFrameRenderer::registerRenderObject`
dereferences the pointer (`cwRhiFrameRenderer.cpp:155`), so null is not an
option. Delete `cwRHIBillboards` from `cwRenderBillboards.cpp` and the whole
backend from `cwRenderTextBillboards.cpp`; the text node code moves into the
compositor.

Window: the backend reads `textLayer()->window()` and falls back to
`itemLayer()->window()` during `synchronize`. Every overlay in a scene lives in
one window (the invariant `cwScene.h` already documents).

### `cwRHIBillboardCompositor` (render thread)

`synchronize` (GUI thread blocked):

1. Bind or rebind the window; tear everything down on a window change or
   `sceneGraphInvalidated`, exactly as the text backend does now.
2. Snapshot `textLayer()->buildRenderSlots()`, `textLayer()->style()` and
   `itemLayer()->buildRenderSlots()`.
3. Text nodes: unchanged from today (two nodes per label, glyphs rebuilt only
   when the layout pointer or style changed, survivors moved into a fresh map).
4. Lead nodes: per item slot keep `LeadNodes { cwQuickItemLayerTexture layer;
   QSGTransformNode* transform; QSGImageNode* image; quintptr root; }`. Create
   on first sight with `window->createImageNode()`, `image->setTexture(layer.texture())`,
   `setOwnsTexture(false)`, `setFiltering(QSGTexture::Linear)`,
   `image->setRect(0, 0, w, h)`; the transform node is the parent of the image
   node and the child that gets linked into "fills". Call `setRootNodeHandle`
   when the slot's handle changed, skip slots whose handle is 0 (the subtree
   is not materialized yet, as today). Delete nodes whose id is gone.

`gather` (Overlay pass only):

1. Build `FrameCamera` from `RenderData` as today.
2. Depth-sort both slot vectors with `cw::billboard::sortBackToFront`, then
   merge them into one back-to-front sequence of `{kind, index}` by
   `sortDepth` (new helper `cw::billboard::mergeBackToFront`, ties: text before
   items, then ascending id).
3. Relink children only when the merged order changed (keep the
   `m_childOrder` check): "outlines" gets the text outline nodes in text order;
   "fills" gets text fill nodes and lead transform nodes in merged order.
4. Set matrices: `frameCamera.modelMatrix(...)` on both text nodes and on the
   lead transform node (`ScreenConstant` for text, the slot's `sizeMode` for
   leads). Keep the general-matrix comment: a translate-only matrix would let
   the renderer merge the batch and drop z.
5. Leads: `setContentSize(slot.contentSize, dpr)` then `updateTexture()`.
6. One `cwItem2DRenderer::prepare` with `mvp = viewProjection` and the shared
   root; one custom drawable calling `render()`.
7. Profile line, live frames only:
   `billboards labels=N items=M overlayUs=.. gatherUs=.. renderUs=..`.

`precludesAtlasBatching()` returns true while either slot vector is non-empty.

### Why leads sit in the "fills" subtree

The text keeps the split-outline layout from `a8045347` (all outlines, then
all fills, two batches). A lead node can only interleave with one of those
subtrees. In "fills", merged by depth:

- Label behind a lead: outline, then fill, then the lead image. Correct.
- Label in front of a lead: outline, then the lead image, then fill. The fill
  paints over the marker; the outline ring under the marker is covered. This is
  the same class of artifact the split already accepts between two labels, and
  the declutter grid keeps labels off each other but not off leads.

The exact alternative is one Outline node per label interleaved with leads in
a single subtree, which costs about one batch per label (11.1 ms versus 5.6 ms
at 1,200 labels in the spike). Keep that comment where the subtrees are built.

Batching: an image node between two fill nodes splits the fill batch only when
their bounding boxes overlap; the batch renderer reorders non-overlapping
elements freely. With a handful of leads the fill subtree stays at one or two
batches.

### `cwScene`

- `billboardCompositor()` creates both stores and the compositor on first call
  and returns the compositor. `billboardLayer()` and `textBillboardLayer()`
  call it and return their store. Drop the "item layer registered first"
  logic and rewrite the comments: draw order inside the Overlay pass is the
  compositor's depth merge, not registration order.
- `cwBillboardOverlayItem` is unchanged apart from the comment on
  `renderTextBillboards()`.

### Behavior changes

- A label behind a lead blends under the marker; a lead behind a label is
  covered by the label's fill and shows through the glyph gaps. Both were cut
  out along a rectangle before.
- A lead's content is rendered through a layer texture at the frame's DPR
  instead of drawn directly. Pixel-identical for the question mark bitmap at
  DPR 1 and 2; the smoke test checks it.
- A lead's `opacity` is still ignored by the 3D draw: the opacity node sits
  above the effect root in both the old path and the layer. `LeadPoint.qml`
  sets 0.6 on completed leads and gets 1.0 today as well. Out of scope; note it
  in the commit message.
- Removing a lead billboard must still happen before its content item dies
  (the subscene deref requires it today; the layer's root pointer requires it
  now).

## Tests

Written first, failing against stubs, then made to pass. Lead regression gate
(must pass with no edits after every change): `[cwRenderBillboards]`,
`[cwBillboardOverlayItem]`, `[cwBillboardHandle]`, `[cwLeadView]`,
`[billboardQuad]`, `test-qml/tst_Leads.qml`, `test-qml/tst_LeadPlacement.qml`.

1. **`test_cwBillboardGeometry.cpp`**: `mergeBackToFront` on mixed text and
   item slots gives farthest-first across both kinds, with ties resolved text
   first then by id; two empty inputs give an empty merge.
2. **`test_cwRenderBillboardCompositor.cpp`** (CPU only): `cwScene` hands out
   one compositor for both store accessors in either order;
   `CwRhiSceneTestAccess::renderObjects` holds exactly one compositor backend
   after sync; a change on either store (add, remove, `setStyle`, `setWindow`)
   emits `changed()` and makes the scene emit `needsRendering`.
3. **`test_cwScene_billboardLayer.cpp`**: replace "gathers the item billboard
   layer before the text layer" with "both layers share one compositor
   registered once"; keep the shared-layer cases.
4. **`test_cwQuickItemLayerTexture.cpp`** (GPU window, `SKIP` under offscreen,
   following `test_cwRenderTextBillboards_render.cpp`): a referenced item with
   a red top half and blue bottom half renders into a texture whose read-back
   through a compositor-less `cwItem2DRenderer` or the smoke scene has red on
   top. This pins the mirror setting.
5. **`test_cwRenderTextBillboards_render.cpp`** extended to a compositor smoke
   test (rename to `test_cwRenderBillboardCompositor_render.cpp`): keep the
   wall and the three labels; add a lead content item (a `QQuickImage` with a
   known opaque disk and transparent corners, or a `QQuickRectangle` with
   `radius` for the same effect) through `cwRenderBillboards`:
   - lead in front of a label: a probe in the lead's transparent corner over
     the label differs from baseline (label visible), a probe in the opaque
     disk shows the lead color;
   - lead behind a label: glyph probes match the labels-only image;
   - lead behind the wall: no difference from baseline;
   - unbind and rebind the window: nothing draws while unbound, everything
     returns after;
   - no `qt.rhi` / `qt.scenegraph` warnings.
6. **Benchmark** (`test_cwRenderTextBillboards_benchmark.cpp`): read the new
   `billboards` profile prefix and also log the item count. Add a hidden
   variant that places 20 synthetic leads to show the per-lead cost is a quad,
   not a pass.

## Facts verified in the Qt 6.11 source (`/Users/cave/qt6`)

- Depth post-pass is unconditional in `RenderMode3D`:
  `qsgbatchrenderer.cpp:4015-4032`. Render nodes are skipped by it.
- `QSGNode::markDirty` notifies every `QSGRootNode` ancestor's renderers
  (`qsgnode.cpp:624-639`), so a change inside a lead subtree reaches the layer's
  renderer registered on that root.
- Item effect roots are created and relinked by Qt under the item's own node
  (`qquickwindow.cpp:2284-2297`).
- `QSGLayer` API: `qsgadaptationlayer_p.h:179-212`; created with
  `QSGContext::createLayer(QSGRenderContext*)` (`qsgcontext_p.h:107`,
  `qsgdefaultcontext.cpp:171`); `QQuickWindowPrivate::get(window)->context`
  is the render context, as `cwItem2DRenderer.cpp` already uses.
- `QSGRhiLayer::grab` uses `currentFrameCommandBuffer()`
  (`qsgrhilayer.cpp:465,506`) and marks itself dirty from the root's
  `sceneGraphChanged` (`qsgrhilayer.cpp:404`); `updateTexture` is a no-op when
  clean (`qsgrhilayer.cpp:71`).
- `ShaderEffectSource` defaults to `MirrorVertically`
  (`qquickshadereffectsource.cpp:161`) and passes it to the layer (`:694`).

## Risks for the implementer

- **Layer texture orientation and premultiplied alpha.** Test 4 settles the
  mirror flag; `QSGImageNode`'s default material expects premultiplied alpha,
  which is what layers produce.
- **Layer MSAA.** `QSGRhiLayer::grab` falls back to the window's MSAA sample
  count; the smoke test window uses the default (none). Nothing to do unless
  the app enables MSAA on the main window.
- **First frame of a lead.** The layer renders on the first `updateTexture`
  after its root materializes; a lead can be one frame late, as today.
- **Teardown order.** Delete the renderer, then the lead nodes (image node
  before its layer texture, since the node holds the texture pointer), then
  the text nodes, then the root. Everything on the render thread, on window
  change, `sceneGraphInvalidated` and in the destructor.
- **Software backend.** `createLayer` returns a software layer that ignores
  RHI; the compositor already does nothing without a QRhi, and the GPU tests
  skip.

## Verification

```
cmake --build build/Qt_6_11_2_for_macOS_Release --target cavewhere-test cavewhere-qml-test CaveWhere
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test "[cwBillboardGeometry],[cwRenderBillboardCompositor],[cwQuickItemLayerTexture],[cwRenderTextBillboards],[cwLabel3dView],[cwScene]" 2>&1 | tee /tmp/compositor-$$.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test "[cwRenderBillboards],[cwBillboardOverlayItem],[cwBillboardHandle],[cwLeadView],[billboardQuad]" 2>&1 | tee /tmp/lead-gate-$$.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-qml-test --platform offscreen -input test-qml/tst_Leads.qml
./build/Qt_6_11_2_for_macOS_Release/cavewhere-qml-test --platform offscreen -input test-qml/tst_LeadPlacement.qml
CAVEWHERE_LARGE_CAVE_INPUT=/Users/cave/Desktop/largeCave.cw ./build/Qt_6_11_2_for_macOS_Release/cavewhere-test "[TextBillboardsBenchmark]" 2>&1 | tee /tmp/compositor-bench-$$.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test -d yes 2>&1 | tee /tmp/cavewhere-test-$$.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-qml-test --platform offscreen 2>&1 | tee /tmp/cavewhere-qml-test-$$.log
```

Known unrelated failures: `test_cwStreamingSrbSwap.cpp:341`,
`test_cwPointOctreeBuilder.cpp:1406`, and the timing-dependent
`test_cwPointCloudStreaming.cpp:4568`; `Leads::test_tapSelectedLeadTogglesClosed`
can miss its tap under full-suite load and passes alone.

Manual: open `/Users/cave/Desktop/largeCave.cw`, select a lead near dense
labels, orbit so labels pass behind and in front of the marker. The marker's
transparent corners show the label behind; a label in front covers the marker.

## Deferred follow-ups

1. Honor the lead content's `opacity` in the 3D draw (an opacity node under
   the lead's transform node fed from the slot).
2. One draw per subtree instead of one per node: a custom material carrying the
   per-billboard transform as vertex attributes, so the unmerged-batch cost
   (3 ms of the current 6 ms) goes away.
3. Fade-in for labels via a `QSGOpacityNode` per label.
