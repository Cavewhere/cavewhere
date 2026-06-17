import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// EDL-path regression for the offscreen renderer. With a point cloud visible the
// offscreen render runs the eye-dome-lighting composite. A past bug cleared the
// cloud colour buffer opaque, so the composite classified every pixel as cloud
// and painted the whole scene black; this asserts the scene (gradient + grid)
// fills the frame behind the cloud.
MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRenderEdl"
        when: windowShown

        function test_edlOffscreenShowsSceneBehindCloud() {
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
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            // A visible point cloud engages the EDL composite on the offscreen path.
            verify(OffscreenRenderTester.addSyntheticPointCloud(RootData), "added a synthetic point cloud");

            // A bright, clearly non-black background so the assertion is robust.
            RootData.regionSceneManager.background.color1 = "#3050a0";
            RootData.regionSceneManager.background.color2 = "#203060";

            let path = OffscreenRenderTester.tempPngPath("offscreen_edl");
            OffscreenRenderTester.removeFile(path);

            let outputSize = Qt.size(400, 300);
            regionViewer.renderToImage(path, outputSize);

            tryVerify(function() { return OffscreenRenderTester.fileExists(path); }, 10000,
                      "offscreen EDL PNG was written");

            let savedSize = OffscreenRenderTester.imageSize(path);
            compare(savedSize.width, outputSize.width, "EDL image honours requested width");
            compare(savedSize.height, outputSize.height, "EDL image honours requested height");

            // The gradient background fills the frame behind the cloud. The
            // regression painted everything black (fraction ≈ 0).
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "scene fills the frame behind the EDL cloud, not painted black");

            OffscreenRenderTester.removeFile(path);
        }
    }
}
