import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Content oracle for the tiled map export. The other map tests assert only
// capture-item layout / UI state (tst_Map) or that a file was written with
// non-zero size (tst_MapMultiLayer) — none look at the rendered pixels, so a
// blank, opaque-background-leaked, mis-sized, or vertically-flipped export would
// pass them all. This test exports a fixed dataset to PNG and asserts the actual
// image content, so it can serve as the before/after gate for migrating the
// export render path onto cwScene::renderOffscreen (plan B).
//
// The setup mirrors tst_Map.qml::setupExport (fixed dataset, zoom, profile view,
// drag rectangle and margins) so the rendered geometry is deterministic.
MainWindowTest {
    id: rootId

    SignalSpy {
        id: captureManagerFinished
        signalName: "finishedCapture"
    }

    CWTestCase {
        name: "MapExportContent"
        when: windowShown

        function captureManager() {
            let mapPage = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->mapPage")
            return findChild(mapPage, "screenCaptureManager")
        }

        function cleanup() {
            // A test skipped under headless offscreen (no QRhi) never creates a
            // capture manager, so guard against null.
            let manager = captureManager()
            if (!manager) {
                return
            }
            while (manager.numberOfCaptures > 0) {
                let index = manager.index(0)
                let capture = manager.data(index, CaptureManager.LayerObjectRole)
                manager.removeCaptureViewport(capture)
            }
        }

        // Deterministic single-layer export setup, copied from tst_Map::setupExport.
        function setupExport() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.qmlTestDatasetPath("tst_ScrapInteraction/projectedProfile.cw"));

            RootData.pageSelectionModel.gotoPageByName(null, "View");
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.05;

            let profileButton
            tryVerify(() => {
                profileButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderingSidePanel->cameraOptions->GroupBox->profileButton")
                return profileButton !== null
            })
            mouseClick(profileButton)
            tryVerify(() => { return turnTableInteraction.pitch === 0.0})

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });

            let paperMargin = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->mapOptions->paperMargin")
            paperMargin.setDefaultLeft(1.0)
            paperMargin.setDefaultRight(1.0)
            paperMargin.setDefaultTop(1.0)
            paperMargin.setDefaultBottom(1.0)

            let addLayerButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->addLayerButton")
            waitForRendering(addLayerButton)
            mouseClick(addLayerButton)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let selectButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton")
            tryVerify(() => { return selectButton.visible });
            mouseClick(selectButton)

            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectAreaInteraction")
            mouseDrag(interaction, 358, 251, 164, 322)

            tryVerify(() => { return selectButton.enabled === true });
            tryVerify(() => { return selectButton.text === " Done" })
            mouseClick(selectButton);

            // capture viewport geometry is computed asynchronously after Done
            wait(100);
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });
        }

        function test_pngExportContent() {
            // The export render path needs a live QRhi; the headless offscreen QPA has none.
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer");
            if (!OffscreenRenderTester.windowHasRhi(renderer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            setupExport();

            let manager = captureManager()
            verify(manager !== null, "found the screenCaptureManager")
            verify(manager.numberOfCaptures === 1, "exactly one capture layer was created")

            let path = OffscreenRenderTester.tempPngPath("map_export_content")
            OffscreenRenderTester.removeFile(path)

            captureManagerFinished.target = manager
            captureManagerFinished.clear()
            manager.filename = TestHelper.toLocalUrl(path)
            manager.fileType = CaptureManager.PNG
            manager.capture()
            captureManagerFinished.wait(20000)

            verify(OffscreenRenderTester.fileExists(path), "export PNG was written")

            // (1) Dimensions follow the saveScene contract: paperSize (inches) *
            // resolution (DPI). This is fixed by the QGraphicsScene canvas size and is
            // independent of the tile render path or the display's device pixel ratio.
            let imageSize = OffscreenRenderTester.imageSize(path)
            compare(imageSize.width, Math.round(manager.paperSize.width * manager.resolution),
                    "PNG width == round(paperSize.width * resolution)")
            compare(imageSize.height, Math.round(manager.paperSize.height * manager.resolution),
                    "PNG height == round(paperSize.height * resolution)")

            // (2) Real geometry was drawn. The PNG export fills Qt::transparent, so the
            // opaque pixels are exactly the drawn map content. The lower bound catches a
            // blank export (tiles failed to render); the upper bound catches an
            // opaque-background leak (e.g. the setCapturing content gate failing and the
            // gradient/grid filling the page).
            let opaque = OffscreenRenderTester.opaqueFraction(path)
            verify(opaque > 0.05, "export has drawn content (opaqueFraction=" + opaque + " > 0.05)")
            verify(opaque < 0.40, "export is not an opaque wash (opaqueFraction=" + opaque + " < 0.40)")

            // (3) The drawn content is placed correctly. The opaque center of mass is a
            // robust placement/orientation probe (a handful of stray corner pixels from
            // QGraphicsScene::render barely move it). Recorded from the pre-migration
            // render of this fixed setup; a vertical flip would map y -> ~0.451.
            let centroid = OffscreenRenderTester.opaqueCentroid(path)
            tryFuzzyCompare(centroid.x, 0.288, 0.05, "opaque content horizontal placement preserved")
            tryFuzzyCompare(centroid.y, 0.549, 0.05, "opaque content vertical placement preserved (not flipped)")

            OffscreenRenderTester.removeFile(path)
        }

        // Regression guard for the tiled-export offscreen render path. Each tile is
        // rendered through cwScene::renderOffscreen; a multi-job batching bug made
        // tiles rendered in the same frame read back as copies of the last tile
        // (the page showed one tile repeated). SVG export embeds each tile as its
        // own base64 raster, so we assert that the tiles carrying content are all
        // byte-distinct — which the bug violated. The deterministic setup spans
        // more than one full content tile, so this is non-vacuous.
        function test_svgTileUniqueness() {
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer");
            if (!OffscreenRenderTester.windowHasRhi(renderer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            setupExport();

            let manager = captureManager()
            verify(manager.numberOfCaptures === 1, "exactly one capture layer was created")

            let path = OffscreenRenderTester.tempPngPath("map_export_tiles").replace(".png", ".svg")
            OffscreenRenderTester.removeFile(path)

            captureManagerFinished.target = manager
            captureManagerFinished.clear()
            manager.filename = TestHelper.toLocalUrl(path)
            manager.fileType = CaptureManager.SVG
            manager.capture()
            captureManagerFinished.wait(20000)

            verify(OffscreenRenderTester.fileExists(path), "export SVG was written")

            let contentTiles = OffscreenRenderTester.svgContentTileCount(path)
            let distinctTiles = OffscreenRenderTester.svgDistinctContentTileCount(path)
            verify(contentTiles >= 2, "export spans multiple content tiles (got " + contentTiles + ")")
            compare(distinctTiles, contentTiles,
                    "every content tile renders its own region (got " + distinctTiles
                    + " distinct of " + contentTiles + ")")

            OffscreenRenderTester.removeFile(path)
        }
    }
}
