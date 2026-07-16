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

        // Shared setup for the render tests: load a project so the 3D view is
        // actively rendering, resolve the region viewer, wait for a live QRhi, and
        // set a non-uniform opaque background (so a render's opaque fraction is ~1).
        // Returns the region viewer, or null after calling skip() when the platform
        // has no QRhi (headless `offscreen` QPA) — callers must `return` on null.
        function loadRhiRegionViewer() {
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
                return null;
            }

            RootData.regionSceneManager.background.color1 = "#203050";
            RootData.regionSceneManager.background.color2 = "#000000";
            return regionViewer;
        }

        // Drives OffscreenRenderTester.renderToImage end-to-end: it queues an
        // offscreen request on the resident scene, the render thread renders it
        // and reads the texture back, and the resulting QImage is saved as a PNG.
        // Asserts the saved image honours the requested size and holds real
        // rendered content (the radial-gradient background) rather than a flat
        // clear — i.e. the offscreen pass actually drew the scene.
        function test_renderToImageProducesNonUniformImage() {
            let regionViewer = loadRhiRegionViewer();
            if (!regionViewer) { return; }

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

        // Drives the Tier-1 per-job visibility override (cwOffscreenRenderParameters::
        // hiddenObjectIds fed from cwRegionSceneManager::captureHiddenObjectIds()) — the
        // mechanism the tiled map exporter uses in place of the old global setCapturing.
        // Renders the resident scene twice with a transparent clear: once whole, once
        // with the export chrome (background gradient, grid plane, line plot) hidden. The
        // full render's opaque region is dominated by the frame-filling gradient, so
        // hiding the chrome must drop the opaque fraction sharply — proving the override
        // suppresses exactly those objects in the output, without touching live visibility.
        function test_hiddenObjectIdsDropsExportChrome() {
            let regionViewer = loadRhiRegionViewer();
            if (!regionViewer) { return; }

            let sceneManager = RootData.regionSceneManager;

            // Live visibility must be untouched by the per-job override (the whole point
            // of Tier 1): record it, render, and confirm it is unchanged afterwards.
            verify(sceneManager.background.visible, "background visible before render");
            verify(sceneManager.linePlot.visible, "line plot visible before render");
            verify(sceneManager.gridPlane.visible, "grid plane visible before render");

            let fullPath = OffscreenRenderTester.tempPngPath("offscreen_chrome_full");
            let hiddenPath = OffscreenRenderTester.tempPngPath("offscreen_chrome_hidden");
            OffscreenRenderTester.removeFile(fullPath);
            OffscreenRenderTester.removeFile(hiddenPath);

            let outputSize = Qt.size(400, 300);
            OffscreenRenderTester.renderTransparentImage(regionViewer, sceneManager, fullPath, outputSize, false);
            OffscreenRenderTester.renderTransparentImage(regionViewer, sceneManager, hiddenPath, outputSize, true);

            tryVerify(function() { return OffscreenRenderTester.fileExists(fullPath); }, 10000,
                      "full transparent PNG was written");
            tryVerify(function() { return OffscreenRenderTester.fileExists(hiddenPath); }, 10000,
                      "chrome-hidden transparent PNG was written");

            let fullOpaque = OffscreenRenderTester.opaqueFraction(fullPath);
            let hiddenOpaque = OffscreenRenderTester.opaqueFraction(hiddenPath);

            verify(fullOpaque > 0.9,
                   "full render is dominated by the opaque gradient (got " + fullOpaque + ")");
            verify(hiddenOpaque < 0.5,
                   "hiding the export chrome drops most opaque content (got " + hiddenOpaque + ")");
            verify(hiddenOpaque < fullOpaque,
                   "chrome-hidden render has strictly less opaque content than the full render");

            // The override is per-job: live visibility is unchanged.
            verify(sceneManager.background.visible, "background still visible after render");
            verify(sceneManager.linePlot.visible, "line plot still visible after render");
            verify(sceneManager.gridPlane.visible, "grid plane still visible after render");

            OffscreenRenderTester.removeFile(fullPath);
            OffscreenRenderTester.removeFile(hiddenPath);
        }
    }
}
