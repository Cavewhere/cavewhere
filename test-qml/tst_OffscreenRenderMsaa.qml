import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// MSAA parity (A3) regression for the offscreen renderer. OffscreenRenderTester's
// renderToImage carries a sample count: the scene is rasterized at that count into a
// multisample target
// and resolved down to the 1x read-back image, so anti-aliased edges match the live
// view. A broken MSAA target / resolve / read-back would black out or mis-size the
// image; these assert the resolved image is the requested size and holds real
// rendered content (flat and EDL-composite paths).
MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRenderMsaa"
        when: windowShown

        function loadAndFindViewer() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let regionViewer = glTerrain.renderer;
            verify(regionViewer, "found the RegionViewer");

            // The readback path needs a live QRhi; the headless offscreen QPA has none.
            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(regionViewer); ++i) {
                wait(50);
            }
            return regionViewer;
        }

        function test_flatMsaaRenderProducesResolvedImage() {
            let regionViewer = loadAndFindViewer();
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            // Non-uniform background so the assertion holds before camera-dependent
            // geometry draws.
            RootData.regionSceneManager.background.color1 = "#203050";
            RootData.regionSceneManager.background.color2 = "#000000";

            let path = OffscreenRenderTester.tempPngPath("offscreen_msaa_flat");
            OffscreenRenderTester.removeFile(path);

            let outputSize = Qt.size(400, 300);
            OffscreenRenderTester.renderToImage(regionViewer, path, outputSize, 4); // force 4x MSAA

            tryVerify(function() { return OffscreenRenderTester.fileExists(path); }, 10000,
                      "MSAA offscreen PNG was written");

            let savedSize = OffscreenRenderTester.imageSize(path);
            compare(savedSize.width, outputSize.width, "MSAA image honours requested width");
            compare(savedSize.height, outputSize.height, "MSAA image honours requested height");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "MSAA image has rendered content, not a flat clear");

            OffscreenRenderTester.removeFile(path);
        }

        function test_edlMsaaRenderShowsScene() {
            let regionViewer = loadAndFindViewer();
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            // A visible point cloud engages the EDL composite, which under MSAA runs
            // the per-sample composite into the multisample target before resolve.
            verify(OffscreenRenderTester.addSyntheticPointCloud(RootData), "added a synthetic point cloud");

            RootData.regionSceneManager.background.color1 = "#3050a0";
            RootData.regionSceneManager.background.color2 = "#203060";

            let path = OffscreenRenderTester.tempPngPath("offscreen_msaa_edl");
            OffscreenRenderTester.removeFile(path);

            let outputSize = Qt.size(400, 300);
            OffscreenRenderTester.renderToImage(regionViewer, path, outputSize, 4); // force 4x MSAA

            tryVerify(function() { return OffscreenRenderTester.fileExists(path); }, 10000,
                      "MSAA EDL offscreen PNG was written");

            let savedSize = OffscreenRenderTester.imageSize(path);
            compare(savedSize.width, outputSize.width, "MSAA EDL image honours requested width");
            compare(savedSize.height, outputSize.height, "MSAA EDL image honours requested height");

            // The gradient scene fills the frame behind the cloud (the EDL composite
            // and resolve produced a real image, not a black wash).
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "scene fills the frame behind the EDL cloud under MSAA");

            OffscreenRenderTester.removeFile(path);
        }
    }
}
