# Station labels: one scene-graph renderer for all labels

Issue: https://github.com/Cavewhere/cavewhere/issues/714 ("Station Label rendering is slow with large caves")
Follows: `plans/STATION_LABEL_GRID_DECLUTTER_PLAN.md` (deferred follow-up 1)

## Context

After the declutter grid landed, the render thread is the frame-time limiter.
Instruments capture of the Release app spinning the 1000-trip / 49,001-station
synthetic cave (`cavewhere-test "[LargeCave]"`), about 7 s, render thread:

| Render loop (4.82 s) | time | share |
|---|---|---|
| `cwRHIBillboards::gather` → `cwItem2DRenderer::prepare` | 1.81 s | 38% |
| `drainBatches` → `QSGRenderer::renderSceneInline` | 1.64 s | 34% |
| `QRhi::beginFrame` freeing last frame's Metal buffers | 0.22 s | 5% |
| `cwRHIBillboards::synchronize` destroying churned renderers | 0.10 s | 2% |
| main QML scene (`QSGRenderer::renderScene`) | 0.46 s | 10% |
| scene-graph sync, 190 ms of it `QSGOpacityNode::setOpacity` (label fade) | 0.54 s | 11% |

Why: every visible station label is one `QQuickItem` (`Label3d.qml`) pulled
into the 3D pass by `cwQuickItemSubscene` and drawn by its own
`cwItem2DRenderer`, i.e. its own `QSGBatchRenderer`. With N placed labels each
frame runs N `prepareSceneInline` passes (render-list rebuild, a fresh Metal
vertex buffer and uniform buffer each, allocated through IOKit), N pipeline
binds and N draw calls. The scene graph amortizes those costs over one scene;
here they are paid per label.

N is not a constant. The declutter accepts labels until the viewport is full,
so N is set by viewport area divided by the label footprint (including the 2.0
margin factor for labels that were hidden last frame), and it changes with
window size, zoom and how the cave projects. The "1,200" that appears in the
spike numbers below is the synthetic worst case from the kd-tree benchmark
(50k uniform random 80×14 rects on 2000×1200); the real app at a typical window
size places a few hundred. The per-label cost of the current path is therefore
higher than the 1,200 figure implies, and the text-layer timings scale down
with N.

The same path also carries the main-thread costs measured earlier: the 1 s
`NumberAnimation` fade per label (12% of the main thread), `setProperty("text")`
relayouts on every declutter flip, and `QQmlComponent::create` when a trip's
per-trip item pool is empty.

### Leads use the same layer and must keep working

`cwRenderBillboards` has two clients, both `cwBillboardOverlayItem` subclasses:
`cwLabel3dView` (station labels) and `cwLeadView` (leads). A lead's billboard
content is the question-mark `Image` inside `LeadPoint.qml`; the tap target is a
sibling item the billboard never references, and `cwLeadView::isOccluded` does
the per-pixel wall test. Leads are few, mostly static, and interactive. Future
interactive billboards will look like leads. That path is correct for them and is
left untouched by this change.

## Goal

Draw all station labels with one scene-graph renderer per frame instead of one
per label, without QQuickItems, while leads and any other QQuickItem billboard
keep using `cwRenderBillboards` exactly as today.

Target on the large cave: the Overlay pass for labels at or under 5 ms of
render thread per frame (the spike measured 5.6 ms standalone for 1,200
split-outline labels, the synthetic upper bound; a few hundred real labels
should land near 2 ms), no per-frame Metal buffer allocation proportional to
the label count, and no label work left on the main thread beyond projection +
declutter.

## Design

### The rule

- **QML item content** (leads, future interactive markers): `cwRenderBillboards`,
  one `cwItem2DRenderer` per billboard. Unchanged.
- **Bulk text** (station labels): new `cwRenderTextBillboards`, one renderer for
  the whole set.

`cwBillboardOverlayItem` stays the shared camera / scene / window plumbing for
both.

### Shared geometry helpers (new `cwBillboardGeometry.h/.cpp`)

Labels and leads sit in the same 3D space and must agree on placement. Lift the
file-scope helpers out of `cwRenderBillboards.cpp` into a small shared unit used
by both backends, with no behavior change:

- `cwBillboardSightLine` (already public) and the depth-bias convention.
- `buildModelMatrix(...)` and the `ScreenConstant` sizing math, taking plain
  parameters (world position, content size, size mode, depth bias, view,
  projection, viewProjection, logical viewport, sight line) rather than a
  `cwRenderBillboards::RenderSlot`, so a text slot can call it too.
- The back-to-front depth sort (`cwSortBillboardSlotsBackToFront`) templated or
  overloaded on the slot type, or reduced to a `sortDepthFor(worldPosition,
  viewProjection)` helper each backend sorts with.

`cwRenderBillboards.cpp` calls the shared versions; its tests keep passing.

### `cwRenderTextBillboards` (GUI-side render object)

```cpp
class CAVEWHERE_LIB_EXPORT cwRenderTextBillboards : public cwRenderObject
{
    Q_OBJECT
public:
    struct TextBillboard {
        QString text;
        QVector3D worldPosition;
        float depthBias = 0.0f;
    };

    // Visual style shared by every label in the layer. Set from QML via
    // Label3dView properties bound to Theme tokens.
    struct Style {
        QFont font;
        QColor color;
        QColor outlineColor;   // QSGTextNode::Outline style color
    };

    explicit cwRenderTextBillboards(QObject* parent = nullptr);
    ~cwRenderTextBillboards();

    void setWindow(QQuickWindow* window);   // for QQuickWindow::createTextNode
    QQuickWindow* window() const;

    void setStyle(const Style& style);
    Style style() const;

    // Stable-id API, mirroring cwRenderBillboards.
    cwBillboardId addBillboard(const TextBillboard& billboard);
    void updateBillboard(cwBillboardId id, const TextBillboard& billboard);  // no-op when unchanged
    void removeBillboard(cwBillboardId id);
    bool hasBillboard(cwBillboardId id) const;
    int billboardCount() const;

    // Laid-out size of `text` in logical pixels with the current style; the
    // view uses it as the exact declutter rect for a placed label. Cached per
    // distinct string.
    QSizeF textSize(const QString& text);

    // Render-thread handoff (GUI thread blocked): plain data plus a shared
    // pointer to the immutable, already laid-out QTextLayout per label.
    struct RenderSlot {
        cwBillboardId id;
        std::shared_ptr<const QTextLayout> layout;   // built on the GUI thread
        QSizeF contentSize;
        QVector3D worldPosition;
        float depthBias;
        float sortDepth = 0.0f;   // render-thread scratch
    };
    QVector<RenderSlot> buildRenderSlots() const;

protected:
    cwRHIObject* createRHIObject() override;
};
```

Details:

- **Text layout happens on the GUI thread.** `QTextLayout` shaping needs the
  font engines; it runs once per distinct string when a label is first added and
  is cached (`QHash<QString, std::shared_ptr<const QTextLayout>>`, dropped when no
  label references it). The render thread only reads the finished layout.
- **Style changes** (theme switch, font) invalidate the layout cache and mark
  every slot dirty.
- `cwBillboardHandle` is reused as-is for RAII ownership; it needs a second
  overload / template parameter for the owning layer type, or a thin
  `cwTextBillboardHandle`. Pick whichever keeps `cwBillboardHandle`'s tests
  unchanged.

### `cwRHITextBillboards` (render-thread backend, in the same `.cpp`)

- `synchronize`: snapshot `buildRenderSlots()`. Own one `QSGRootNode` (created
  on first use with the scene-graph-initialized window) with two child
  `QSGNode` subtrees, "outlines" and "fills" (see batching below). Maintain
  `std::unordered_map<cwBillboardId, LabelNodes>` where `LabelNodes` holds two
  `QSGTextNode*`: for a new id call `QQuickWindow::createTextNode()` twice, both
  with `QtRendering` and `addTextLayout(QPointF(0,0), layout.get())`; the
  outline node gets `Outline` style, `styleColor` = outline color and a fully
  transparent `color`, appended under "outlines"; the fill node gets `Normal`
  style and the text color, appended under "fills". Remove both nodes when the
  id is gone. Rebuild glyphs only when the layout pointer changed. `QSGTextNode`
  is itself a `QSGTransformNode`, so the per-frame model matrix is set on both
  nodes of a label.
- `gather` (Overlay pass): read camera, target and DPR from `RenderData` (the
  offscreen-camera rule in `cwRHIBillboards::gather` applies verbatim). Sort
  slots back to front, set each node's matrix from the shared
  `buildModelMatrix`, then **one** `cwItem2DRenderer::prepare` with
  `rootNodeHandle` = our root node and `mvp` = viewProjection (the model is in
  the nodes), and **one** custom drawable that calls `render()`. `cwItem2DRenderer`
  is reused unchanged.
- `precludesAtlasBatching()` returns true while slots exist, for the same
  one-MVP-per-gather reason as the item layer.
- Window change **or scene-graph invalidation**: drop the root node and the
  text nodes (they belong to the old render context), like `m_renderers.clear()`
  does today. `QSGTextNode` docs require deletion on invalidation.
- Layout lifetime: `QTextLayout` is reentrant, not thread-safe, and its const
  reads mutate caches. The render side touches a layout only inside
  `synchronize` (`addTextLayout` copies the glyph runs and keeps no pointer), and
  releases its `shared_ptr` references only inside `synchronize` or teardown,
  never in `gather`. The first use of the style font on the render thread
  reloads its engines into the render thread's `QFontCache`; this is Qt's
  normal `QQuickText` path.

### Node matrices and batching (from the spike)

`QSGBatchRenderer` in `RenderMode3D` merges a batch only when every element
matrix is translate-only (`isTranslateOnlyToRoot`; the distance-field text
material sets `RequiresDeterminant`, which implies this). A merged batch bakes
node matrices into vertex x/y and drops z, so **merging is wrong for 3D labels:
the camera-facing model matrices must stay general, and the batches stay
unmerged**. Unmerged means one shared vertex upload and one draw per node with
its own matrix in the uniform buffer, which is the intended steady state. Never
"optimize" the nodes to translate-only matrices; depth silently flattens.

Unmerged draws compute clip = projection × (root × node) with the projection
untouched (no per-element z injection in 3D mode), so VP in the renderer and M
on the node reproduces today's `frame.mvp = VP × M` exactly. Verified
pixel-identical for `Normal` style at two camera distances.

**Outline style costs batches.** `QSGInternalTextNode` emits a second glyph node
per label for `Outline`, and each fill overlaps its own outline that already sits
in an earlier batch, so 1,200 outlined labels produce about 1,200 one-node
batches. **Decision: split subtrees.** Per label, one `Outline` text node with a fully
transparent fill color (suppresses the extra fill node) under an "outlines"
subtree, plus one `Normal` fill node under a "fills" subtree, both carrying the
same model matrix. Two batches total. Trade-off: where two labels overlap, a
farther label's fill can paint over a nearer label's outline ring; the
declutter grid keeps placed labels from overlapping, so this is rare.

Document the alternative in the backend source, next to where the two
subtrees are built, so it can be restored if the exact look is ever needed:
a single `Outline` node per label with the fill color set reproduces
`Label3d.qml` exactly, at the cost of about one batch per label (measured
11.1 ms vs 5.6 ms render-thread CPU per frame at 1,200 labels). The comment
should state why (`QSGInternalTextNode::addGlyphs` emits a second fill node
whose bounding box overlaps its own outline, which ends the batch group).

Spike timings, render-thread CPU per frame, 1,200 labels (synthetic upper
bound), Metal, DPR 1: `Outline` 11.1 ms, split 5.6 ms, `Normal` 2.9 ms. All
three scale roughly linearly with the placed count. For comparison the current
per-label renderers consumed about 3.8 s of the 4.82 s render loop in the
profile, at whatever count the large cave actually placed.

### Draw order between the two layers

In `RenderMode3D` the batch renderer draws the alpha pass with depth-write off,
then a depth post-pass that writes depth over every glyph quad, transparent
margins included; no Qt Quick fragment shader discards. Within one renderer
labels therefore never bite each other, and the back-to-front sort only decides
paint order where labels overlap. Across the two layers the lists cannot
interleave, and **whichever layer draws second is clipped by the first layer's
quad depth.** So order the **item layer (leads) before the text layer**: a lead
marker is drawn intact and a label behind it loses pixels under the marker's
quad, which is the right sacrifice. Make the order explicit (scene add order or
`sortKey`) and test it.

### `cwLabel3dView` integration

- Replace the QQuickItem pool, `m_component`, `acquireLabelItem` /
  `releaseLabelItem` / `labelItem`, `cwLabel3dGroup::m_labelItems` and
  `m_itemPool` with a `cwBillboardHandle`-style handle per label into the text
  layer. Accept = `setTextBillboard(handle, text, world, bias)`; reject = reset
  the handle.
- Declutter rect: `renderTextBillboards()->textSize(label.text())` for labels
  already placed (exact), `m_averageLabelSize` for candidates (as now). Keep the
  running average from the placed labels' exact sizes.
- Add `Q_PROPERTY`s `font`, `color`, `outlineColor` (plain members with explicit
  setters and `NOTIFY`, per CLAUDE.md) forwarded to the layer's `Style`.
  `RenderingView.qml` binds them to `Theme.fontSizeUI`, `Theme.textInverse`,
  `Theme.text`, which is what `Label3d.qml` hardcodes today.
- `cwBillboardOverlayItem` gains `renderTextBillboards()` /
  `ensureRenderTextBillboards()` alongside the existing item-layer accessors, and
  `cwScene` gains `textBillboardLayer()` mirroring `billboardLayer()`.
- `Label3d.qml` stays: `Compass.qml` uses it for N/S/E/W. Only its station-label
  use goes away. The `cwLinePlotLabelView` keyword-visibility and group plumbing
  is unchanged.

### Behavior changes to call out

- **No fade-in** for station labels. They appear on the frame they are placed.
  If missed, a `QSGOpacityNode` per label driven from one C++ clock is a cheap
  follow-up and still one renderer.
- Label text is drawn by `QSGTextNode` with `QtRendering` instead of a
  `QC.Label`; glyphs come from the same engine and the spike showed only
  anti-aliasing-level differences. The split outline differs from today only
  where two labels overlap on screen (a farther fill over a nearer outline
  ring). Verify against a screenshot of a small cave.
- Labels inside one renderer no longer clip each other's quad corners (#538
  within the label set), since depth is written in a post-pass after all labels
  are drawn.
- Station labels have no QQuickItem to click. If per-station interaction is
  wanted later, the placed rects in `cwLabel3dView` give a GUI-thread pick,
  followed by the same `isOccluded` test leads use.

## Tests (Catch2 unless noted; written first, failing, then made to pass)

1. **Shared helpers** (`test_cwBillboardGeometry.cpp`): `buildModelMatrix` with
   plain parameters equals the matrix `cwRenderBillboards` produced before the
   extraction for the same inputs (perspective and orthographic, both size
   modes, non-zero depth bias). Existing `cwBillboardSightLine` cases move or
   stay; they must pass unmodified.
2. **`cwRenderTextBillboards` GUI API** (`test_cwRenderTextBillboards.cpp`):
   add/update/remove with stable ids; `updateBillboard` with identical data
   emits no change; `textSize` is positive for a non-empty string and cached
   (same pointer in two slots with the same text); style change invalidates
   layouts; `buildRenderSlots` carries text, position, bias and a non-null
   layout for every billboard.
3. **Layer ownership** (`test_cwScene_billboardLayer.cpp` extended):
   `cwScene::textBillboardLayer()` is created once and shared by two overlays;
   the item layer (`billboardLayer()`) gathers before the text layer in the
   Overlay pass regardless of which overlay fetched its layer first.
4. **`cwLabel3dView`** (`test_cwLinePlotLabelView.cpp` extended): after
   `setRegion` with the two-trip fixture, the view has no `QQuickItem` children
   for labels and the text layer's `billboardCount()` equals the number of
   placed labels; the shared station appears once. Hiding a trip via keyword
   visibility removes its text billboards.
5. **Render smoke** (GPU-backed `QQuickWindow`, same pattern as
   `test_cwRhiTexturedItemsStaleBindings.cpp`, which `SKIP`s when the platform
   is `offscreen` because that QPA has no QRhi): a scene with three text labels
   renders a frame without errors and a label pixel differs from the
   background; swapping the window drops and rebuilds the nodes without a
   crash. A label behind a cave wall (an opaque quad in front of it) leaves the
   background pixel unchanged, which checks depth against RHI-drawn geometry
   under the real threaded render loop, the one case the spike could not run.
   Batch count (two for N labels, none merged) is checked manually with
   `QSG_RENDERER_DEBUG=render` and recorded in the commit message.
6. **Lead regression gate, unchanged files:** `test_cwRenderBillboards.cpp`,
   `test_cwBillboardHandle.cpp`, `test_cwBillboardOverlayItem.cpp`,
   `test_cwLeadView_occlusion.cpp`, `test_cwGeometryItersecter_billboardQuad.cpp`,
   `tst_Leads.qml`, `tst_LeadPlacement.qml`. These must pass with no edits.
   `test_cwBillboardOverlayItem.cpp` may gain cases for the text-layer accessors
   but existing cases are not modified.
7. **Benchmark, hidden `[.][Benchmark]`:** on the large cave, log the Overlay
   pass time through `cw.profile.render` for 60 frames of a scripted camera
   orbit (drive `cwCamera` directly; no real input, per CLAUDE.md) and print
   the median together with the placed label count (`billboardCount()` on the
   text layer) and the viewport size, so the per-label cost is a measurement.
   No hard bound; the numbers go in the commit message.

Tests must be safe to run as concurrent processes (no fixed temp paths).

## Verification

```
cmake --build build/Qt_6_11_2_for_macOS_Release --target cavewhere-test cavewhere-qml-test CaveWhere
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test "[cwBillboardGeometry],[cwRenderTextBillboards],[cwLabel3dView]" 2>&1 | tee /tmp/text-billboards.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test "[cwRenderBillboards],[cwBillboardOverlayItem],[cwBillboardHandle],[cwLeadView]" 2>&1 | tee /tmp/lead-gate.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test -d yes 2>&1 | tee /tmp/cavewhere-test.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-qml-test --platform offscreen 2>&1 | tee /tmp/cavewhere-qml-test.log
```

Manual: open `largeCave.cw` at `--page View`, orbit, and re-run the Instruments
Time Profiler on the render thread. `cwItem2DRenderer::prepare` should appear
once per frame; Metal `AGXBuffer` allocation should no longer scale with label
count. Open a small real cave and compare label appearance and lead markers
(including tapping a lead and the occlusion test behind a wall) against `dev_2`.

## Spike results (2026-10-24, against the qt6 6.11 checkout and 6.11.2 frameworks)

Spike code and images: scratchpad `spike/` (`spike.cpp`, `build.sh`,
`frame_*.png`, `dbg*.log`); nothing in the repo was touched.

1. **Batch merging:** confirmed that 3D matrices block merging, and confirmed
   that merging would be wrong anyway (z dropped). Resolved in the design above.
   `Outline` style multiplies batches; split-subtree workaround measured at 2
   batches.
2. **`createTextNode` threading:** confirmed OK. It requires an initialized
   scene graph and returns render-context objects; `cwRHIObject::synchronize`
   runs in the same context as `QQuickText::updatePaintNode`. Glyph
   rasterization and atlas uploads happen in `preprocess()`, which
   `prepareSceneInline()` runs, so the inline `cwItem2DRenderer` path handles
   new glyphs.
3. **`QTextLayout` across threads:** confirmed OK under the sync barrier, same
   pattern `QQuickText` uses. Caveats folded into the design (render reads and
   releases only inside `synchronize`).
4. **Glyph cache:** confirmed per (font face × glyph index), keyed by
   `FontKey` without pixel size. Bounded by the character set.

Not exercised by the spike: the threaded render loop at runtime (offscreen
forces the software backend, so the proof used `QQuickRenderControl` on one
thread), DPR 2, GPU time, and depth against real cave geometry. The render smoke
test (test 5) and the manual check cover these.

## Deferred follow-ups (not in this change)

1. Fade-in for text labels via `QSGOpacityNode` and one C++ clock.
2. Renderer caching across declutter flips in `cwRHIBillboards::synchronize`
   (only matters if the lead count grows large).
3. Per-trip frustum culling before the per-station projection loop.
4. Station-label picking from the placed rects, if labels become interactive.
