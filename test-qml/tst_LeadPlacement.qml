import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Visual test for station-label and lead-label collision-based placement.
// Loads Phake Cave 3000, sets up an export region, enables the Leads layer
// option, and writes an SVG to the per-test temp directory, then opens it
// in the system default viewer for manual inspection.
MainWindowTest {
    id: rootId

    SignalSpy {
        id: captureManagerFinished
        signalName: "finishedCapture"
    }

    TestCase {
        name: "LeadPlacement"
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
        // it so its properties (incl. leadsVisible) are bound. Returns the
        // layer's map item (captureItem0).
        function setupLayer() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            // Wait for scrap triangulation / image upload futures to drain so
            // the 3D scene is fully populated before we capture. Without this,
            // the export's grabToImage runs before scrap textures are ready
            // and the alpha mask comes back empty.
            RootData.futureManagerModel.waitForFinished()

            // Zoom into the data, in the 3d view (keep default plan view; do
            // not click the profile button — we want labels exported against
            // the plan-view rendering).
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            turnTableInteraction.camera.zoomScale = 0.2;

            let mapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mainSideBar->mapButton")
            mouseClick(mapButton)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });
            wait(100)

            let addLayerButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->addLayerButton")
            mouseClick(addLayerButton)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "viewPage" });

            let selectionButton = null
            tryVerify(() => {
                          selectionButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton")
                          return selectionButton !== null && selectionButton.visible
                      })
            mouseClick(selectionButton)

            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectAreaInteraction")
            // Drag a generous rectangle around the cave so most scraps land in it
            mouseDrag(interaction, 389.645, 137.965, 340, 349)

            tryVerify(() => { return selectionButton.enabled === true })
            let done = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->SplitView->renderer->selectionExportAreaTool->selectionToolButton->label")
            mouseClick(done)

            // wait() needed — capture viewport geometry is computed asynchronously
            // after Done click; without it the capture item has wrong dimensions
            wait(100)
            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "mapPage" });

            // Select the new layer so its properties (incl. leadsVisible) are bound
            let captureItem0 = null
            tryVerify(() => {
                          captureItem0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage->SplitView->captureItem0")
                          return captureItem0 !== null
                      })
            mouseClick(captureItem0)
            tryVerify(() => { return captureItem0.selected === true })

            return captureItem0
        }

        function exportSvg(fileName) {
            let outPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl()) + "/" + fileName
            let outUrl  = TestHelper.toLocalUrl(outPath)
            TestHelper.removeFile(outUrl)
            verify(!TestHelper.fileExists(outUrl))

            let screenCaptureManager = findChild(mapPage(), "screenCaptureManager")
            captureManagerFinished.target = screenCaptureManager
            captureManagerFinished.clear()
            screenCaptureManager.filename = outUrl
            screenCaptureManager.fileType = CaptureManager.SVG
            screenCaptureManager.capture()
            captureManagerFinished.wait(20000)

            verify(TestHelper.fileExists(outUrl));
            verify(TestHelper.fileSize(outUrl) > 0);
            return outUrl
        }

        function test_exportSvgWithLeads() {
            if (!hasRhi()) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            let captureItem0 = setupLayer()

            // Enable the Leads option directly on the capture viewport
            captureItem0.captureItem.leadsVisible = true
            verify(captureItem0.captureItem.leadsVisible === true)

            // Export SVG to the per-test temp directory. Open it after for
            // visual inspection.
            let outUrl = exportSvg("cavewhere_lead_placement.svg")
            let outPath = RootData.urlToLocal(outUrl)

            console.log("[LeadPlacement] wrote", outPath, "size=", TestHelper.fileSize(outUrl))

            // Open the SVG BEFORE running asserts so failures still surface a
            // viewable file for inspection.
            Qt.openUrlExternally(outUrl)

            // Collect both checks BEFORE asserting anything so the log shows
            // the complete picture even when the first assertion fails.
            let passageOverlaps = SvgOverlap.passageOverlaps(outUrl)
            let passageBad = []
            for (let i = 0; i < passageOverlaps.length; i++) {
                let entry = passageOverlaps[i]
                // Lead markers render as a "?" glyph in the same font as labels
                // and intentionally sit on the passage drawing — skip them.
                let isLeadMarker = entry.text === "?"
                console.log("[passage]", entry.text,
                            "rect=", entry.rect.x.toFixed(1),
                                     entry.rect.y.toFixed(1),
                                     entry.rect.width.toFixed(1),
                                     entry.rect.height.toFixed(1),
                            "overlap=", entry.overlapPixels, "px",
                            "(" + entry.overlapPercent.toFixed(1) + "%)",
                            isLeadMarker ? "[lead-marker, skipped]" : "")
                if (!isLeadMarker && entry.overlapPixels > 0) {
                    passageBad.push(entry.text + "(" + entry.overlapPercent.toFixed(0) + "%)")
                }
            }

            let textCollisions = SvgOverlap.textCollisions(outUrl)
            for (let i = 0; i < textCollisions.length; i++) {
                let c = textCollisions[i]
                console.log("[text-collision]", c.textA, "vs", c.textB,
                            "overlapPct=", c.overlapPercent.toFixed(1))
            }

            let leaderCollisions = SvgOverlap.textLeaderCollisions(outUrl)
            let leaderBad = []
            for (let i = 0; i < leaderCollisions.length; i++) {
                let c = leaderCollisions[i]
                // "?" lead-marker glyphs sit AT the leader's endpoint by
                // design (the leader points to the marker). Skip them.
                let isLeadMarker = c.text === "?"
                console.log("[text-leader-collision]", c.text,
                            "rect=", c.textRect.x.toFixed(1), c.textRect.y.toFixed(1),
                                     c.textRect.width.toFixed(1), c.textRect.height.toFixed(1),
                            "leader=", c.leaderStart.x.toFixed(1), c.leaderStart.y.toFixed(1),
                                       "->",
                                       c.leaderEnd.x.toFixed(1), c.leaderEnd.y.toFixed(1),
                            isLeadMarker ? "[lead-marker, skipped]" : "")
                if (!isLeadMarker) {
                    leaderBad.push(c.text)
                }
            }

            let leaderCrossings = SvgOverlap.leaderLeaderCollisions(outUrl)
            for (let i = 0; i < leaderCrossings.length; i++) {
                let c = leaderCrossings[i]
                console.log("[leader-leader-collision]",
                            "A=", c.leaderAStart.x.toFixed(1), c.leaderAStart.y.toFixed(1),
                            "->", c.leaderAEnd.x.toFixed(1), c.leaderAEnd.y.toFixed(1),
                            "B=", c.leaderBStart.x.toFixed(1), c.leaderBStart.y.toFixed(1),
                            "->", c.leaderBEnd.x.toFixed(1), c.leaderBEnd.y.toFixed(1),
                            "at", c.intersection.x.toFixed(1), c.intersection.y.toFixed(1))
            }

            verify(passageBad.length === 0,
                   "Labels overlapping rendered passage ink: " + passageBad.join(", "))
            verify(textCollisions.length === 0,
                   "Labels overlap each other: " + textCollisions.length + " pairs")
            verify(leaderBad.length === 0,
                   "Labels overlap leader lines: " + leaderBad.join(", "))
            verify(leaderCrossings.length === 0,
                   "Leader lines cross each other: " + leaderCrossings.length + " pairs")
        }

        function test_leadsToggleAfterExport() {
            if (!hasRhi()) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            // Leads stay off (the default) for the first preview.
            let captureItem0 = setupLayer()
            let viewport = captureItem0.captureItem
            compare(viewport.leadsVisible, false)

            tryVerify(() => { return CaptureLayerInspector.previewPlaced(viewport) }, 20000,
                      "the preview's label placement finished")

            // Hidden leads take no space: the preview builds no leads item.
            compare(CaptureLayerInspector.previewLeads(viewport).count, 0)

            exportSvg("cavewhere_leads_toggle_after_export.svg")

            let leadsCheckBox = findChild(mapPage(), "leadsCheckBox")
            verify(leadsCheckBox !== null, "found leadsCheckBox")
            mouseClick(leadsCheckBox)
            tryCompare(viewport, "leadsVisible", true)

            // The toggle reaches the preview, not the hidden export group.
            tryVerify(() => { return CaptureLayerInspector.previewLeads(viewport).visibleCount === 1 }, 20000,
                      "the preview shows its leads after the export")

            mouseClick(leadsCheckBox)
            tryCompare(viewport, "leadsVisible", false)
            tryVerify(() => { return CaptureLayerInspector.previewLeads(viewport).count === 0 }, 20000,
                      "turning Leads off removes the preview's leads item")
        }
    }
}
