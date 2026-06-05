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

        function test_exportSvgWithLeads() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            // Wait for scrap triangulation / image upload futures to drain so
            // the 3D scene is fully populated before we capture. Without this,
            // the export's grabToImage runs before scrap textures are ready
            // and the alpha mask comes back empty.
            RootData.futureManagerModel.waitForFinished()

            // Zoom into the data, in the 3d view (keep default plan view; do
            // not click the profile button — we want labels exported against
            // the plan-view rendering).
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer");
            let turnTableInteraction = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->RenderingView->renderer->turnTableInteraction")
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
                          selectionButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionToolButton")
                          return selectionButton !== null && selectionButton.visible
                      })
            mouseClick(selectionButton)

            let interaction = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectAreaInteraction")
            // Drag a generous rectangle around the cave so most scraps land in it
            mouseDrag(interaction, 389.645, 137.965, 340, 349)

            tryVerify(() => { return selectionButton.enabled === true })
            let done = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderer->selectionExportAreaTool->selectionToolButton->label")
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

            // Enable the Leads option directly on the capture viewport
            captureItem0.captureItem.leadsVisible = true
            verify(captureItem0.captureItem.leadsVisible === true)

            // Export SVG to the per-test temp directory. Open it after for
            // visual inspection.
            let outPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl()) + "/cavewhere_lead_placement.svg"
            let outUrl  = TestHelper.toLocalUrl(outPath)
            TestHelper.removeFile(outUrl)
            verify(!TestHelper.fileExists(outUrl))

            let mapPage = ObjectFinder.findObjectByChain(mainWindow, "rootId->mapPage")
            let screenCaptureManager = findChild(mapPage, "screenCaptureManager")
            captureManagerFinished.target = screenCaptureManager
            screenCaptureManager.filename = outUrl
            screenCaptureManager.fileType = CaptureManager.SVG
            screenCaptureManager.capture()
            captureManagerFinished.wait(20000)

            verify(TestHelper.fileExists(outUrl));
            verify(TestHelper.fileSize(outUrl) > 0);

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
    }
}
