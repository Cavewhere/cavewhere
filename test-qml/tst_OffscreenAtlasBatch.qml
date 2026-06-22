import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Exercises the offscreen renderer's atlas-batch path (plan
// OFFSCREEN_ATLAS_BATCHING_PLAN, Phases 2-3). renderPannedBatch issues N compatible
// offscreen renders together, so >1 drains through the atlas (each tile copied into its
// own sub-rect, the atlas read back once and sliced). The single-tile path
// (count == 1, no panning) is the parity reference: an atlas slice of a tile must equal
// its standalone render bit-for-bit, and distinct cameras must give distinct tiles —
// the guard against the camera-slot collapse (all tiles rendering the last camera) and
// any sub-rect/camera mix-up.
MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenAtlasBatch"
        when: windowShown

        readonly property int tileSize: 128
        readonly property int batchCount: 8

        // RootData is a session singleton shared across the whole QML suite (one
        // process). loadAndShow() hides the station/lead billboards; restore them
        // after each test so the default-visible state doesn't leak into the
        // alphabetically-later render tests.
        function cleanup() {
            RootData.stationsVisible = true;
            RootData.leadsVisible = true;
        }

        function regionViewer() {
            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let viewer = glTerrain.renderer;
            verify(viewer, "found the RegionViewer");
            return viewer;
        }

        function ensureRhiOrSkip(viewer) {
            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(viewer); ++i) {
                wait(50);
            }
            if (!OffscreenRenderTester.windowHasRhi(viewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return false;
            }
            return true;
        }

        function loadAndShow() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            // Hide the station-label / lead billboards: they can't be atlas-batched
            // (their inline MVP UBO collapses across tiles), so a job that draws them
            // renders single-tile. This test exercises the multi-tile atlas path, whose
            // real consumer (thumbnails) suppresses billboards too — so hide them here.
            RootData.stationsVisible = false;
            RootData.leadsVisible = false;
            // Non-uniform background so even camera-independent pixels differ under a pan.
            RootData.regionSceneManager.background.color1 = "#203050";
            RootData.regionSceneManager.background.color2 = "#000000";
        }

        // Flat path: parity (atlas slice 0 == single render) + per-tile validity +
        // pairwise distinctness across the batch.
        function test_atlasBatchParityAndDistinctness() {
            loadAndShow();
            let viewer = regionViewer();
            if (!ensureRhiOrSkip(viewer)) {
                return;
            }

            // Single-tile reference (count == 1 -> the non-atlas path), index 0 unpanned.
            let singleDir = OffscreenRenderTester.makeTempDir("atlas_single");
            compare(OffscreenRenderTester.renderPannedBatch(viewer, singleDir, "t", 1, tileSize), 1,
                    "single reference tile rendered");

            // Batch of distinct-camera tiles (count > 1 -> the atlas path).
            let batchDir = OffscreenRenderTester.makeTempDir("atlas_batch");
            compare(OffscreenRenderTester.renderPannedBatch(viewer, batchDir, "t", batchCount, tileSize),
                    batchCount, "all batch tiles rendered");

            // Validity: each tile is the requested size.
            for (let i = 0; i < batchCount; ++i) {
                let size = OffscreenRenderTester.imageSize(batchDir + "/t_" + i + ".png");
                compare(size.width, tileSize, "batch tile " + i + " width");
                compare(size.height, tileSize, "batch tile " + i + " height");
            }

            // Parity: the atlas slice of tile 0 equals the standalone single render.
            verify(OffscreenRenderTester.imagesEqual(singleDir + "/t_0.png", batchDir + "/t_0.png"),
                   "atlas slice 0 is identical to the single-tile render of the same camera");

            // Distinctness: distinct cameras -> pairwise-distinct tiles (no slot collapse,
            // no sub-rect mix-up). Adjacent indices are the tightest check.
            for (let j = 0; j + 1 < batchCount; ++j) {
                verify(!OffscreenRenderTester.imagesEqual(batchDir + "/t_" + j + ".png",
                                                          batchDir + "/t_" + (j + 1) + ".png"),
                       "batch tiles " + j + " and " + (j + 1) + " are distinct");
            }

            OffscreenRenderTester.removeDir(singleDir);
            OffscreenRenderTester.removeDir(batchDir);
        }

        // Spillover (Phase 4): a batch larger than one atlas-full (the per-frame camera-slot
        // cap, cwRhiScene::kOffscreenBatchCameraSlots = 256) must drain across multiple
        // frames via the requestUpdate() re-arm loop, resolving every future with the right
        // image — no hang, no dropped tile, and tiles from the later frame(s) still render
        // distinctly. Uses tiny tiles so 300 renders stay cheap. (Assumes the count exceeds
        // the per-frame cap; if that constant grows past spillCount this stops testing
        // spillover.)
        function test_atlasBatchSpillsAcrossFrames() {
            loadAndShow();
            let viewer = regionViewer();
            if (!ensureRhiOrSkip(viewer)) {
                return;
            }

            let smallTile = 64;
            let spillCount = 300;

            // Single-tile reference for parity (index 0 unpanned).
            let singleDir = OffscreenRenderTester.makeTempDir("atlas_spill_single");
            compare(OffscreenRenderTester.renderPannedBatch(viewer, singleDir, "t", 1, smallTile), 1,
                    "single reference tile rendered");

            let batchDir = OffscreenRenderTester.makeTempDir("atlas_spill");
            compare(OffscreenRenderTester.renderPannedBatch(viewer, batchDir, "t", spillCount, smallTile),
                    spillCount, "every tile of the over-cap batch resolved (full drain across frames)");

            // Sizes are correct for a first-frame, a late-frame, and the final tile.
            let checkSizes = [0, spillCount - 1, 290];
            for (let k = 0; k < checkSizes.length; ++k) {
                let size = OffscreenRenderTester.imageSize(batchDir + "/t_" + checkSizes[k] + ".png");
                compare(size.width, smallTile, "spill tile " + checkSizes[k] + " width");
                compare(size.height, smallTile, "spill tile " + checkSizes[k] + " height");
            }

            // Parity still holds for the first tile in a spillover batch.
            verify(OffscreenRenderTester.imagesEqual(singleDir + "/t_0.png", batchDir + "/t_0.png"),
                   "spillover slice 0 matches the single-tile render");

            // Tiles render distinctly in the first frame (0 vs 1) and in a later frame
            // (290 vs 291, both past the 256 cap) — cross-frame state isn't corrupted.
            verify(!OffscreenRenderTester.imagesEqual(batchDir + "/t_0.png", batchDir + "/t_1.png"),
                   "first-frame tiles are distinct");
            verify(!OffscreenRenderTester.imagesEqual(batchDir + "/t_290.png", batchDir + "/t_291.png"),
                   "later-frame (spilled) tiles are distinct");

            OffscreenRenderTester.removeDir(singleDir);
            OffscreenRenderTester.removeDir(batchDir);
        }

        // EDL path (Phase 3): with a point cloud visible each tile renders through the EDL
        // composite into the scratch before being copied into the atlas. Confirm the batch
        // still produces valid, distinct, parity-matching tiles.
        function test_atlasBatchWithEdl() {
            loadAndShow();
            let viewer = regionViewer();
            if (!ensureRhiOrSkip(viewer)) {
                return;
            }
            verify(OffscreenRenderTester.addSyntheticPointCloud(RootData), "added a synthetic point cloud");

            let singleDir = OffscreenRenderTester.makeTempDir("atlas_edl_single");
            compare(OffscreenRenderTester.renderPannedBatch(viewer, singleDir, "t", 1, tileSize), 1,
                    "single EDL reference tile rendered");

            let batchDir = OffscreenRenderTester.makeTempDir("atlas_edl_batch");
            compare(OffscreenRenderTester.renderPannedBatch(viewer, batchDir, "t", batchCount, tileSize),
                    batchCount, "all EDL batch tiles rendered");

            verify(OffscreenRenderTester.imagesEqual(singleDir + "/t_0.png", batchDir + "/t_0.png"),
                   "EDL atlas slice 0 matches the single-tile EDL render");
            for (let j = 0; j + 1 < batchCount; ++j) {
                verify(!OffscreenRenderTester.imagesEqual(batchDir + "/t_" + j + ".png",
                                                          batchDir + "/t_" + (j + 1) + ".png"),
                       "EDL batch tiles " + j + " and " + (j + 1) + " are distinct");
            }

            OffscreenRenderTester.removeDir(singleDir);
            OffscreenRenderTester.removeDir(batchDir);
        }
    }
}
