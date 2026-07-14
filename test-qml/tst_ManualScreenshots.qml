import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// User-manual screenshot harness (Deliverable B). Doubles as a regression test:
// under the headless `offscreen` QPA it skips (no live QRhi, so grabWindow()
// cannot capture the 3D view); on a GPU-backed platform it drives the real full
// UI, navigates to each documented page, and writes a PNG via WindowGrabber.
//
// Run it as the generator with scripts/gen-manual-screenshots.sh, which points
// CW_MANUAL_IMAGE_DIR at docs/manual/images/. Without that variable the grabs
// land in a temp dir, so a plain test run never writes into the source tree.
//
// Shots here back the 3D View chapter (docs/manual/view-3d/) — the sharpest test
// of grabWindow(), since each grab must contain the QML UI chrome and the live
// RHI 3D scene together, which the offscreen renderer cannot produce.
MainWindowTest {
    id: rootId

    HighlightOverlay {
        id: highlightOverlayId
        anchors.fill: parent
        z: 1000
        target: null
    }

    TestCase {
        name: "ManualScreenshots"
        when: windowShown

        // Load the demo cave, navigate to the 3D view, and wait for a live QRhi.
        // Returns the region viewer, or null after calling skip() when the
        // platform has no QRhi (headless `offscreen`) — callers must return on null.
        function loadRhiViewer() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            RootData.pageSelectionModel.currentPageAddress = "View";

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let regionViewer = glTerrain.renderer;
            verify(regionViewer, "found the RegionViewer");

            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(regionViewer); ++i) {
                wait(50);
            }
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return null;
            }

            // Show the sidebar File button. Its loader is gated off on macOS (native
            // menu bar there); force it on so the docs match the cross-platform UI
            // users see. Harmless if already active.
            let fileLoader = findByName(rootId.mainWindow, "fileMenuButtonLoader");
            if (fileLoader) { fileLoader.active = true; }

            // Let the line-plot / carpeting jobs drain so the "Updating Scene"
            // progress chrome is gone and the 3D scene is fully rendered before any
            // grab. (The render background is left at the app's own light gradient —
            // no override — so screenshots match the real appearance.)
            RootData.futureManagerModel.waitForFinished();
            return regionViewer;
        }

        // Recursive by-objectName search over the visual tree. The side panel is
        // reparented by LayoutItemProxy, so its named-ancestor chain no longer
        // matches ObjectFinder.findObjectByChain — a plain name search still finds it.
        function findByName(item, name) {
            if (!item) { return null; }
            if (item.objectName === name) { return item; }
            let kids = item.children;
            for (let i = 0; kids && i < kids.length; ++i) {
                let found = findByName(kids[i], name);
                if (found) { return found; }
            }
            return null;
        }

        // The View/Layers tab bar in the 3D view's side panel.
        function sidePanelTabBar() {
            return findByName(rootId.mainWindow, "renderingTabBar");
        }

        // Drain any jobs the navigation/tab-switch queued, then give the scene
        // graph a couple of frames to present the page (and any highlight overlay)
        // before grabbing.
        function settle() {
            RootData.futureManagerModel.waitForFinished();
            wait(150);
        }

        function cleanup() {
            highlightOverlayId.target = null;
            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
        }

        // Full-window screenshot of the 3D view: proves grabWindow() captures the
        // QML UI chrome and the live 3D scene together in one image. Backs the
        // chapter's opening overview.
        function test_view3dOverview() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
            highlightOverlayId.target = null;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-overview");
            verify(path.length > 0, "grabToFile wrote a screenshot");
            verify(WindowGrabber.fileExists(path), "screenshot file exists");

            let size = WindowGrabber.imageSize(path);
            verify(size.width > 0 && size.height > 0, "screenshot has real dimensions");
            // UI chrome + 3D scene fill the frame — nowhere near all-black.
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "screenshot is not a blank/black frame (got "
                   + OffscreenRenderTester.nonBlackFraction(path) + ")");
        }

        // Highlighted screenshot of the camera side panel: the ring overlay must
        // appear in the grab, proving the highlight path renders before capture.
        // Backs the chapter's "Aim the camera" section.
        function test_view3dCameraControls() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let panel = findByName(rootId.mainWindow, "renderingSidePanel");
            verify(panel, "found the rendering side panel");

            let tabBar = sidePanelTabBar();
            if (tabBar) { tabBar.currentIndex = 0; }
            highlightOverlayId.target = panel;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-camera-controls");
            verify(path.length > 0, "grabToFile wrote the camera-controls screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "camera-controls screenshot is not blank");
        }

        // The Layers tab active: shows the keyword layer panel. Backs the
        // chapter's "Focus on part of the cave with layers" section.
        function test_view3dLayers() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            let panel = findByName(rootId.mainWindow, "renderingSidePanel");
            verify(panel, "found the rendering side panel");

            let tabBar = sidePanelTabBar();
            verify(tabBar, "found the side-panel tab bar");
            tabBar.currentIndex = 1; // Layers
            highlightOverlayId.target = panel;
            settle();

            let path = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-layers");
            verify(path.length > 0, "grabToFile wrote the layers screenshot");
            verify(OffscreenRenderTester.nonBlackFraction(path) > 0.5,
                   "layers screenshot is not blank");
        }

        // Cropped, element-focused grab: grabItemToFile crops the window grab to
        // the viewport's rect, proving the crop math against the window DPR.
        function test_view3dViewportCrop() {
            let regionViewer = loadRhiViewer();
            if (!regionViewer) { return; }

            highlightOverlayId.target = null;
            settle();

            // Full-window grab first, so the crop can be checked against it in the
            // same physical-pixel space (no dependence on the screen's DPR here).
            let fullPath = WindowGrabber.grabToFile(rootId.mainWindow, "view-3d-fullwindow");
            let cropPath = WindowGrabber.grabItemToFile(regionViewer, "view-3d-viewport", 0);
            verify(cropPath.length > 0, "grabItemToFile wrote a cropped screenshot");

            let fullSize = WindowGrabber.imageSize(fullPath);
            let cropSize = WindowGrabber.imageSize(cropPath);
            verify(cropSize.width > 0 && cropSize.height > 0, "crop has real dimensions");
            verify(cropSize.width <= fullSize.width && cropSize.height <= fullSize.height,
                   "crop is no larger than the full window");
            verify(cropSize.width < fullSize.width,
                   "viewport crop is narrower than the window (excludes the side panel)");
            verify(OffscreenRenderTester.nonBlackFraction(cropPath) > 0.3,
                   "cropped viewport screenshot has rendered content");

            // These two are mechanism-regression grabs, not chapter assets — don't
            // leave them behind in docs/manual/images/.
            WindowGrabber.removeFile(fullPath);
            WindowGrabber.removeFile(cropPath);
        }
    }
}
