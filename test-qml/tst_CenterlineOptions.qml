import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// The map layer's Centerline options (Dots, Legs, Labels): hidden labels leave
// the exported SVG, and toggling an option re-places the preview's labels
// without moving or resizing the layer on the paper.
MainWindowTest {
    id: rootId

    SignalSpy {
        id: captureManagerFinished
        signalName: "finishedCapture"
    }

    SignalSpy {
        id: previewChangedSpy
        signalName: "previewItemChanged"
    }

    SignalSpy {
        id: viewportFinishedSpy
        signalName: "finishedCapture"
    }

    TestCase {
        name: "CenterlineOptions"
        when: windowShown

        function hasRhi() {
            // The export render path needs a live QRhi; the headless offscreen QPA has none.
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer");
            return OffscreenRenderTester.windowHasRhi(renderer)
        }

        function mapPage() {
            return ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage")
        }

        function cleanup() {
            // Layers outlive a project reload, so remove them to give each test
            // a fresh captureItem0. A skipped (headless) test has no manager.
            let mapPageItem = mapPage()
            let manager = mapPageItem ? findChild(mapPageItem, "screenCaptureManager") : null
            if (!manager) {
                return
            }
            while (manager.numberOfCaptures > 0) {
                let capture = manager.data(manager.index(0), CaptureManager.LayerObjectRole)
                manager.removeCaptureViewport(capture)
            }
        }

        // Loads Phake Cave 3000, adds one map layer around the cave and selects
        // it so the layer properties (incl. the Centerline group box) are bound.
        // Returns the layer's capture viewport.
        function setupLayer() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            // Let scrap triangulation / image upload futures drain so the 3D
            // scene is fully populated before the capture.
            RootData.futureManagerModel.waitForFinished()

            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.2;

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });

            // The SplitView lays out its panes on the next polish pass; until
            // then the Add Layer button maps into the left half and the click
            // lands on the scene view.
            let addLayerButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->addLayerButton")
            tryVerify(()=>{ return addLayerButton.mapToItem(null, 0, 0).x > mainWindow.width / 2 });
            mouseClick(addLayerButton)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let selectionButton = null
            tryVerify(() => {
                          selectionButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton")
                          return selectionButton !== null && selectionButton.visible
                      })
            mouseClick(selectionButton)

            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectAreaInteraction")
            let dragX = 389.645
            let dragY = 137.965
            let dragDx = 340
            let dragDy = 349
            // On a fresh window the view page may still be laying out; wait
            // until the whole drag fits inside the interaction.
            tryVerify(() => { return interaction.width > dragX + dragDx && interaction.height > dragY + dragDy })
            mouseDrag(interaction, dragX, dragY, dragDx, dragDy)

            tryVerify(() => { return selectionButton.enabled === true })
            let done = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton->label")
            mouseClick(done)

            // wait() needed — capture viewport geometry is computed asynchronously
            // after Done click; without it the capture item has wrong dimensions
            wait(100)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });

            let captureItem0 = null
            tryVerify(() => {
                          captureItem0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")
                          return captureItem0 !== null
                      })
            mouseClick(captureItem0)
            tryVerify(() => { return captureItem0.selected === true })

            let captureItem = captureItem0.captureItem
            tryVerify(() => { return captureItem.previewItem !== null })
            return captureItem
        }

        function clickCheckBox(objectName) {
            let checkBox = findChild(mapPage(), objectName)
            verify(checkBox !== null, "found " + objectName)
            tryVerify(() => { return checkBox.visible })
            mouseClick(checkBox)
        }

        // Clicks all three boxes back to back, then waits for the viewport to
        // take each new value.
        function toggleCenterlineParts(captureItem, visible) {
            let parts = ["Dots", "Legs", "Labels"]
            for (let part of parts) {
                clickCheckBox("centerline" + part + "CheckBox")
            }
            for (let part of parts) {
                tryCompare(captureItem, "centerline" + part + "Visible", visible)
            }
        }

        function exportSvg(fileName) {
            let outUrl = TestHelper.toLocalUrl(RootData.urlToLocal(TestHelper.tempDirectoryUrl()) + "/" + fileName)
            TestHelper.removeFile(outUrl)
            verify(!TestHelper.fileExists(outUrl))

            let screenCaptureManager = findChild(mapPage(), "screenCaptureManager")
            captureManagerFinished.target = screenCaptureManager
            captureManagerFinished.clear()
            screenCaptureManager.filename = outUrl
            screenCaptureManager.fileType = CaptureManager.SVG
            screenCaptureManager.capture()
            captureManagerFinished.wait(20000)

            verify(TestHelper.fileExists(outUrl))
            return outUrl
        }

        function test_hiddenLabelsLeaveSvg() {
            if (!hasRhi()) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            let captureItem = setupLayer()

            // With the scale bar off and leads off (the default), every SVG
            // text element is a station label.
            captureItem.scaleBarVisible = false
            compare(captureItem.leadsVisible, false)

            let withLabelsUrl = exportSvg("cavewhere_centerline_labels_on.svg")
            verify(SvgOverlap.passageOverlaps(withLabelsUrl).length > 0,
                   "the default export has station labels")

            clickCheckBox("centerlineLabelsCheckBox")
            tryCompare(captureItem, "centerlineLabelsVisible", false)

            let withoutLabelsUrl = exportSvg("cavewhere_centerline_labels_off.svg")
            compare(SvgOverlap.passageOverlaps(withoutLabelsUrl).length, 0)
        }

        function test_toggleKeepsLayerPlacement() {
            if (!hasRhi()) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            let captureItem = setupLayer()
            // Let the initial preview finish, so every finishedCapture below
            // comes from a relabel run.
            tryVerify(() => { return CaptureLayerInspector.previewPlaced(captureItem) }, 20000,
                      "the initial preview placed its labels")
            let position = captureItem.positionOnPaper
            let paperSize = captureItem.paperSizeOfItem

            previewChangedSpy.target = captureItem
            previewChangedSpy.clear()
            viewportFinishedSpy.target = captureItem
            viewportFinishedSpy.clear()

            // A relabel replaces the preview's centerline with one new item,
            // revealed once its placement finishes.
            let waitForRelabel = () => {
                tryVerify(() => {
                              let centerlines = CaptureLayerInspector.previewCenterlines(captureItem)
                              return viewportFinishedSpy.count >= 1
                                  && centerlines.count === 1
                                  && centerlines.visibleCount === 1
                          }, 20000, "the relabel placed one visible centerline")
                compare(previewChangedSpy.count, 0, "the preview group was not rebuilt")
                compare(captureItem.positionOnPaper, position)
                compare(captureItem.paperSizeOfItem, paperSize)
            }

            toggleCenterlineParts(captureItem, false)
            waitForRelabel()

            viewportFinishedSpy.clear()
            toggleCenterlineParts(captureItem, true)
            waitForRelabel()
        }

        function test_exportDuringRelabelKeepsPreviewLabels() {
            if (!hasRhi()) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            let captureItem = setupLayer()
            tryVerify(() => { return CaptureLayerInspector.previewPlaced(captureItem) }, 20000,
                      "the initial preview placed its labels")

            // Start a relabel, then export while it is still placing: the
            // export supersedes it, and the preview gets its labels back.
            clickCheckBox("centerlineLabelsCheckBox")
            tryCompare(captureItem, "centerlineLabelsVisible", false)
            exportSvg("cavewhere_centerline_export_during_relabel.svg")

            tryVerify(() => {
                          let centerlines = CaptureLayerInspector.previewCenterlines(captureItem)
                          return centerlines.count === 1 && centerlines.visibleCount === 1
                      }, 20000, "the preview shows its centerline after the export")
        }
    }
}
