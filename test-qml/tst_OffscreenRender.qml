import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRender"
        when: windowShown

        // Drives OffscreenRenderTester.renderToImage end-to-end: it queues an
        // offscreen request on the resident scene, the render thread renders it
        // and reads the texture back, and the resulting QImage is saved as a PNG.
        // Asserts the saved image honours the requested size and holds real
        // rendered content (the radial-gradient background) rather than a flat
        // clear — i.e. the offscreen pass actually drew the scene.
        function test_renderToImageProducesNonUniformImage() {
            // Load a project so the 3D view is shown and actively rendering.
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");

            let regionViewer = glTerrain.renderer;
            verify(regionViewer, "found the RegionViewer");

            // The readback path needs a live QRhi. The headless `offscreen` QPA
            // provides none, so skip there; a GPU-backed platform exercises it.
            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(regionViewer); ++i) {
                wait(50);
            }
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            // Guarantee a non-uniform background so the assertion holds even
            // before any camera-dependent geometry draws.
            RootData.regionSceneManager.background.color1 = "#203050";
            RootData.regionSceneManager.background.color2 = "#000000";

            let path = OffscreenRenderTester.tempPngPath("offscreen_render");
            OffscreenRenderTester.removeFile(path);

            let outputSize = Qt.size(400, 300);
            OffscreenRenderTester.renderToImage(regionViewer, path, outputSize);

            tryVerify(function() { return OffscreenRenderTester.fileExists(path); }, 10000,
                      "offscreen PNG was written");

            let savedSize = OffscreenRenderTester.imageSize(path);
            compare(savedSize.width, outputSize.width, "offscreen image honours requested width");
            compare(savedSize.height, outputSize.height, "offscreen image honours requested height");
            verify(OffscreenRenderTester.imageIsNonUniform(path),
                   "offscreen image has rendered content, not a flat clear");

            OffscreenRenderTester.removeFile(path);
        }
    }
}
