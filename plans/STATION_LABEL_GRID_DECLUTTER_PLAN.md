# Station label declutter: replace the kd-tree with a uniform grid

Issue: https://github.com/Cavewhere/cavewhere/issues/714 ("Station Label rendering is slow with large caves")

## Context

`cwLabel3dView::updatePositions()` runs on the GUI thread on every camera
`viewMatrixChanged` / `projectionChanged`. For every station label that passes
the viewport clip it inserts a screen-space rect into `cwCollisionRectKdTree`
(`m_labelKdTree.addRect`) and accepts the label when the insert succeeds.

Profile of the Release app (`sample`, 8 s turntable drag) on a synthetic cave
with 1000 trips and 49,001 stations (`testcases/test_cwLargeCaveGenerator.cpp`,
hidden tag `[.LargeCave]`):

| Main thread (8 s) | samples |
|---|---|
| total | 6,681 |
| busy in `cwLabel3dView::updatePositions` | 2,541 |
| of which inside `cwCollisionRectKdTree::addRect` / `rectIntersects` | 2,385 |

A standalone benchmark of the kd-tree alone (uniform random 80x14 rects on a
2000x1200 viewport):

| labels | accepted | ms per pass |
|---|---|---|
| 1,000 | 537 | 0.7 |
| 10,000 | 1,066 | 12.3 |
| 50,000 | 1,207 | 72.6 |

Why: `rectIntersects` splits on a stored rect's left/top edge. A rect stored on
the left of a split can still extend across it, so the query always descends the
left subtree and only prunes the right. Every insert degenerates to a near
linear scan of all ~1,200 accepted rects, for every one of the ~50k candidates,
every frame. The tree is also deleted and rebuilt with per-node `new` each frame.

## Goal

Make the per-insert cost independent of how many labels are already placed, so
the declutter pass at 50k labels costs a few milliseconds instead of 70+.
Placement results must stay identical: the same exact rect-overlap test, in the
same candidate order, accepts the same labels.

## Design: `cwCollisionRectGrid`

New class `cavewherelib/src/cwCollisionRectGrid.{h,cpp}` replacing
`cwCollisionRectKdTree` (which has no other users; `cwRhiViewer.h` only has a
commented-out include). Delete the kd-tree files once the view is switched.

```cpp
class cwCollisionRectGrid
{
public:
    cwCollisionRectGrid();

    // Resets to an empty grid covering `bounds` with cells of `cellSize`.
    // Reuses existing storage; allocation only happens when the grid grows.
    // cellSize components are clamped to >= kMinCellSize (1 px).
    void reset(const QRect& bounds, const QSize& cellSize);

    // Clears placed rects but keeps bounds, cell size and storage.
    void clear();

    // Returns true and records the rect when it overlaps no placed rect
    // (QRect::intersects semantics, identical to the kd-tree), else false.
    // Rects partially or fully outside `bounds` are still tested: their cell
    // range is clamped to the grid edge cells.
    bool addRect(const QRect& rect);

    int rectCount() const;
    QRect bounds() const;
    QSize cellSize() const;

private:
    struct Entry {
        QRect rect;
        int next;   // index of the next entry in the same cell, or -1
    };
    QRect m_bounds;
    QSize m_cellSize;
    int m_columns = 0;
    int m_rows = 0;
    std::vector<int> m_cellHeads;   // columns * rows, -1 when empty
    std::vector<Entry> m_entries;   // one per cell a rect covers (a rect
                                    // spanning 4 cells has 4 entries)
};
```

Semantics and details:

- `addRect` computes the cell range the rect covers (clamped to the grid),
  walks each cell's chain testing `QRect::intersects`, returns false on the
  first hit. Otherwise it appends one `Entry` per covered cell, linking each
  into that cell's head. A rect must be tested against every covered cell
  before any entry is appended.
- Duplicate hits are harmless: a rect spanning several cells may be visited
  more than once by a query; that only costs a redundant intersects call.
- Null / empty rects: `QRect::intersects` returns false for an empty rect, so
  the kd-tree accepted them. Preserve that (accept and record nothing, or
  accept and record; either way they must never block others). Pick "accept,
  record nothing" and document it.
- Cell size is chosen by the caller. `cwLabel3dView` passes its tracked
  `m_averageLabelSize` scaled by the visible margin factor, rounded up, so an
  average label covers at most 2x2 cells. Add named constants for the minimum
  cell size and for any fallback when the viewport is empty.
- No per-frame heap allocation after the first frame: `reset`/`clear` only
  resize or `assign` existing vectors; `m_entries.clear()` keeps capacity.
- Memory: ~3,100 cells x 4 B plus ~20 B per placed-rect cell entry for the
  current viewport; the viewport alone sets the cell count, not the label count.

### Integration in `cwLabel3dView`

- Replace the `cwCollisionRectKdTree m_labelKdTree` member with
  `cwCollisionRectGrid m_labelGrid`.
- In `updatePositions()`, before iterating groups, call
  `m_labelGrid.reset(camera()->viewport(), cellSizeFromAverageLabel())`, where
  the cell size derives from `m_averageLabelSize` and the existing
  `visibleMarginFactor`. Remove the trailing `m_labelKdTree.clear()`.
- `updateGroupPositions` keeps calling `addRect(stationRect.toAlignedRect())`.
  Hoist `visibleMarginFactor` / `invisibleMarginFactor` to file-scope
  `constexpr` so the cell-size helper can share them.
- Behavior otherwise unchanged: same candidate order (previously visible first,
  sorted by depth), same accept/reject actions.

## Tests (Catch2, `testcases/test_cwCollisionRectGrid.cpp`)

Written first, failing against a stub, then made to pass:

1. **Exactness vs brute force.** Seeded random rects (varying sizes 10..200 by
   8..40, positions across and partly outside a 2000x1200 bounds, several
   cell sizes including ones smaller than the rects). Insert each into the grid
   and into a plain `QList<QRect>` brute-force checker; the accept/reject
   sequence must match exactly.
2. **Non-overlapping rects all accepted;** a rect touching an edge only
   (`QRect::intersects` false for adjacent integer rects) is accepted.
3. **Rect larger than a cell** spanning many cells is detected from any
   covered cell, including a later small rect inside its interior.
4. **Rects outside bounds** (negative coordinates, beyond width/height) still
   collide with each other and with edge rects.
5. **reset / clear reuse:** after `clear()` the same rects are accepted again;
   after `reset` with a new bounds/cell size, `rectCount()` is 0 and inserts
   work; capacity reuse is observable only indirectly, so assert behavior, not
   allocation.
6. **Empty rect** never blocks a later rect and is reported accepted.
7. **Benchmark, hidden `[.][Benchmark]`:** 50,000 uniform random 80x14 rects
   on 2000x1200 with a 84x15 cell; print elapsed ms with `WARN`; assert it is
   under a generous bound (e.g. 20 ms in Release, skip the bound in Debug via
   `#ifdef QT_DEBUG`).
8. **cwLabel3dView smoke:** extend `testcases/test_cwLinePlotLabelView.cpp`
   only if a cheap assertion exists (e.g. the two-trip fixture still shows the
   shared station once). Do not add test-only members to production classes
   (CLAUDE.md).

Tests must be safe to run as concurrent processes (no fixed temp paths).

## Verification

```
cmake --build build/Qt_6_11_2_for_macOS_Release --target cavewhere-test cavewhere-qml-test CaveWhere
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test "[cwCollisionRectGrid]" 2>&1 | tee /tmp/grid-test.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test "[Benchmark]" 2>&1 | tee /tmp/grid-bench.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-test -d yes 2>&1 | tee /tmp/cavewhere-test.log
./build/Qt_6_11_2_for_macOS_Release/cavewhere-qml-test --platform offscreen 2>&1 | tee /tmp/cavewhere-qml-test.log
```

Manual: open the large cave `.cw` produced by
`CAVEWHERE_LARGE_CAVE_OUTPUT=<path> cavewhere-test "[LargeCave]"`, spin the
turntable, `sample` the process: `cwCollisionRect*` should no longer dominate the
main thread.

## Deferred follow-ups (not in this change)

1. **One scene-graph renderer per visible label.** `cwRHIBillboards::gather`
   runs a `cwItem2DRenderer::prepare` + `renderSceneInline` per billboard
   (~1,200 per frame, ~64% of busy render-thread samples in the profile).
   Investigate batching labels through one renderer or a glyph atlas.
2. **Flip churn and the 1 s fade.** Each label that flips accepted/rejected
   relayouts text, restarts `Label3d.qml`'s fade (whose `opacityChanged`
   requests a frame for a second), and destroys/recreates its billboard
   subscene. The item pool is per trip, so items freed in one trip cannot
   serve another. Investigate a view-wide pool, keeping the subscene with the
   pooled item, and suppressing the fade during camera motion.
3. Coarse frustum culling per trip (bounding box) before the per-station loop,
   to skip projecting labels of trips wholly off-screen.
