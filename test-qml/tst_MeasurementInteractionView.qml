import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "MeasurementInteractionView"
        when: windowShown

        // Tests run in one process; deactivate the tool after each so the next
        // starts with the turn-table as the default interaction.
        function cleanup() {
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer")
            if (renderer && renderer.measurementInteraction
                && renderer.measurementInteraction.enabled) {
                renderer.measurementInteraction.deactivate()
            }
        }

        // Verifies the view wiring: the toolbar button activates the interaction
        // via InteractionManager (disabling the turn-table), placing two points
        // fills the readout popup, the copy button invokes copyToClipboard(), and
        // toggling off restores the turn-table and resets the measurement.
        //
        // The snap/measurement numerics (cwScenePick + cwMeasurementMath) are
        // covered by the [cwMeasurementInteraction] and [cwMeasurementMath] C++
        // unit tests; here we drive the real interaction against the loaded cave.
        // Scans a coarse grid over the central viewport, returning the screen
        // points whose ray lands on the cave. Picking is CPU-side (the BVH), so
        // this works under the offscreen platform even without a live RHI.
        function _scanForHits(measurement, w, h) {
            let points = []
            let x0 = Math.round(w * 0.2)
            let x1 = Math.round(w * 0.8)
            let y0 = Math.round(h * 0.2)
            let y1 = Math.round(h * 0.8)
            let stepX = Math.max(4, Math.round((x1 - x0) / 40))
            let stepY = Math.max(4, Math.round((y1 - y0) / 40))
            for (let y = y0; y <= y1; y += stepY) {
                for (let x = x0; x <= x1; x += stepX) {
                    measurement.hover(Qt.point(x, y))
                    if (measurement.hoverValid) {
                        // Capture the world hit too, so callers can maximise the
                        // real measured distance, not just on-screen separation.
                        // Copy the vector explicitly — hoverPoint returns a shared
                        // proxy that mutates on the next hover.
                        let p = measurement.hoverPoint
                        points.push({ screen: Qt.point(x, y),
                                      world: Qt.vector3d(p.x, p.y, p.z) })
                    }
                }
            }
            return points
        }

        function _worldDistance(a, b) {
            return Math.sqrt((a.x - b.x) * (a.x - b.x)
                             + (a.y - b.y) * (a.y - b.y)
                             + (a.z - b.z) * (a.z - b.z))
        }

        // The pair of hits with the greatest real (world) separation, returned
        // as their screen points {a, b}.
        function _farthestPair(hits) {
            let bestA = hits[0]
            let bestB = hits[0]
            let best = -1
            for (let i = 0; i < hits.length; i++) {
                for (let j = i + 1; j < hits.length; j++) {
                    let d = _worldDistance(hits[i].world, hits[j].world)
                    if (d > best) {
                        best = d
                        bestA = hits[i]
                        bestB = hits[j]
                    }
                }
            }
            return { a: bestA.screen, b: bestB.screen }
        }

        // Recursive object-name search that also descends into contentItem, so
        // it can reach inside a Popup (whose content reparents to the window
        // overlay and is therefore unreachable by ObjectFinder's visual chain).
        function _findByObjectName(root, name) {
            if (!root) {
                return null
            }
            if (root.objectName === name) {
                return root
            }
            let kids = root.children
            for (let i = 0; kids && i < kids.length; i++) {
                let found = _findByObjectName(kids[i], name)
                if (found) {
                    return found
                }
            }
            if (root.contentItem && root.contentItem !== root) {
                return _findByObjectName(root.contentItem, name)
            }
            return null
        }

        // Loads the cave, activates the tool, and places A + B at the two
        // farthest pickable points, returning the renderer with a completed
        // measurement.
        function _setupCompletedMeasurement() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "View page should be active")

            let renderer = null
            tryVerify(() => {
                renderer = ObjectFinder.findObjectByChain(rootId.mainWindow,
                    "rootId->viewPage->SplitView->renderer")
                return renderer !== null
            }, 5000, "Renderer found")

            let measurement = renderer.measurementInteraction
            let measureButton = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer->measurementButton")
            mouseClick(measureButton)
            tryVerify(() => measurement.enabled === true, 1000, "Measurement enabled")

            let hits = []
            tryVerify(() => {
                hits = _scanForHits(measurement, renderer.width, renderer.height)
                return hits.length >= 2
            }, 5000, "Found at least two pickable points on the cave")

            let pair = _farthestPair(hits)
            measurement.place(pair.a)
            tryVerify(() => measurement.hasFirst, 1000, "First point placed")
            measurement.hover(pair.b)
            measurement.place(pair.b)
            tryVerify(() => measurement.hasMeasurement, 1000, "Measurement completed")
            return renderer
        }

        function test_lifecycle_and_measurement() {
            TestHelper.loadProjectFromFile(RootData.project, TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "View page should be active")

            let renderer = null
            tryVerify(() => {
                renderer = ObjectFinder.findObjectByChain(rootId.mainWindow,
                    "rootId->viewPage->SplitView->renderer")
                return renderer !== null
            }, 5000, "Renderer found")

            let turnTable = renderer.turnTableInteraction
            let measurement = renderer.measurementInteraction
            verify(measurement !== null, "Measurement interaction exposed on renderer")
            verify(turnTable !== null, "Turn table exposed on renderer")

            let measureButton = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer->measurementButton")
            verify(measureButton !== null, "Measure button found")

            // Default: turn-table active, measurement idle.
            verify(turnTable.enabled === true, "Turn-table active by default")
            verify(measurement.enabled === false, "Measurement inactive by default")
            verify(measurement.hasFirst === false, "No first point on startup")
            verify(measurement.hasMeasurement === false, "No measurement on startup")

            // Activate via the button.
            mouseClick(measureButton)
            tryVerify(() => measurement.enabled === true, 1000, "Measurement enabled after toggle on")
            tryVerify(() => turnTable.enabled === false, 1000, "Turn-table disabled while measuring")

            // A click in empty space is a no-op (the ray misses all geometry).
            measurement.place(Qt.point(2, 2))
            tryCompare(measurement, "hasFirst", false, 500)

            // Find two well-separated points on the cave to measure between. The
            // scan re-runs until the geometry's BVH has finished building.
            let hits = []
            tryVerify(() => {
                hits = _scanForHits(measurement, renderer.width, renderer.height)
                return hits.length >= 2
            }, 5000, "Found at least two pickable points on the cave")

            // Use the pair with the greatest real separation so A and B are
            // distinct and the distance is comfortably non-zero.
            let pair = _farthestPair(hits)
            verify(Math.hypot(pair.b.x - pair.a.x, pair.b.y - pair.a.y) > 20,
                   "Two distinct screen points")

            measurement.place(pair.a)
            tryVerify(() => measurement.hasFirst, 1000, "First point placed")

            measurement.hover(pair.b)
            measurement.place(pair.b)
            tryVerify(() => measurement.hasMeasurement, 1000, "Measurement completed")
            verify(measurement.distance > 0.0, "Measured a non-zero distance")

            // copyToClipboard() (what the popup's Copy button calls) writes the
            // SI readout. The QC.Popup reparents to the window overlay, so it's
            // not reachable by visual chain — exercise its handler directly.
            measurement.copyToClipboard()

            // Toggle off — restores the turn-table and clears the measurement.
            mouseClick(measureButton)
            tryVerify(() => turnTable.enabled === true, 1000, "Turn-table restored when measuring toggled off")
            tryVerify(() => measurement.enabled === false, 1000, "Measurement disabled after toggle off")
            tryVerify(() => measurement.hasMeasurement === false, 1000, "Measurement reset on deactivate")
        }

        // Reproduces the distance-readout truncation: a distance of 10 m or more
        // has a two-or-more-digit integer part, and the user saw the leading
        // digit clipped (14.2 -> 4.2, 234.2 -> 34.2). The numeric string is
        // correct; the bug was the readout TextField's text region (width minus
        // the style's default 7 px side padding, which `padding: 0` did NOT
        // override) being narrower than the text — so right-aligned text lost its
        // leading digit. This asserts the text region keeps a real margin over the
        // content; it fails against the pre-fix sizing and passes after it.
        function test_distanceReadoutNotTruncated() {
            let renderer = _setupCompletedMeasurement()
            let measurement = renderer.measurementInteraction

            verify(measurement.distance >= 10.0,
                   "Need a >=10 m measurement to exercise the bug (got "
                   + measurement.distance + " m)")

            let popup = renderer.measurementReadoutPopup
            verify(popup !== null, "Readout popup exposed")
            popup.collapsed = false
            tryVerify(() => popup.visible === true, 1000, "Readout popup visible")

            let distField = _findByObjectName(popup.contentItem, "measurementDistanceValue")
            verify(distField !== null, "Distance value field found")

            // The underlying string is right; the bug is purely visual clipping.
            let expected = Number(measurement.distance).toFixed(2)
            verify(distField.text.indexOf(expected) !== -1,
                   "Field text '" + distField.text + "' should contain '" + expected + "'")

            // With AlignRight, a text region no wider than the text clips the
            // leading characters (the layout must not squeeze the field below its
            // content, and HiDPI rounding must have slack). Require a real margin
            // in the region the text actually draws into (width minus padding).
            // tryVerify so the assertion waits for the layout to settle.
            tryVerify(() => {
                let textRegion = distField.width - distField.leftPadding - distField.rightPadding
                return textRegion >= distField.contentWidth + 4.0
            }, 2000, "Distance field text region too tight: width " + distField.width
                     + " padding " + distField.leftPadding + "/" + distField.rightPadding
                     + " vs contentWidth " + distField.contentWidth)

            // The on-line chip uses one decimal — this is the readout in the
            // user's report ("14.2" -> "4.2"). Its label and background must both
            // be comfortably wider than the text.
            let chip = _findByObjectName(renderer, "measurementLineDistanceLabel")
            verify(chip !== null, "On-line distance chip found")
            compare(chip.text, qsTr("%1 m").arg(Number(measurement.distance).toFixed(1)),
                    "Chip shows the full distance string")
            verify(chip.parent.width >= chip.contentWidth + 2.0,
                   "Chip background has no margin for its text: rect " + chip.parent.width
                   + " vs contentWidth " + chip.contentWidth)
        }

        // The azimuth row carries a north-reference selector whose value tracks
        // the resolved bearing, and the Copy text reflects the selection. The
        // resolve numerics are covered by the [cwMeasurementInteraction] and
        // [cwAzimuthReference] C++ tests; here we verify the view wiring: the
        // combo reflects the property both ways and the value label binds.
        function test_azimuthReferenceSelector() {
            let renderer = _setupCompletedMeasurement()
            let measurement = renderer.measurementInteraction

            let popup = renderer.measurementReadoutPopup
            verify(popup !== null, "Readout popup exposed")
            popup.collapsed = false
            tryVerify(() => popup.visible === true, 1000, "Readout popup visible")

            let combo = _findByObjectName(popup.contentItem, "azimuthReferenceCombo")
            verify(combo !== null, "Azimuth reference combo found")
            let valueField = _findByObjectName(popup.contentItem, "azimuthReferenceValue")
            verify(valueField !== null, "Azimuth reference value found")

            // The expanded panel drives calculateDetails, which gates the detailed
            // reference resolve (the per-hover perf fix).
            tryVerify(() => measurement.calculateDetails === true, 1000,
                      "Expanded readout enables the detailed resolve")

            // Drive from a known reference (settings persist across runs, so don't
            // assume the default). Grid always resolves and mirrors the grid azimuth.
            measurement.azimuthReference = AzimuthReference.Grid
            tryCompare(combo, "currentIndex", 0, 1000, "Grid selects row 0")
            compare(valueField.text, qsTr("%1°").arg(Number(measurement.referenceAzimuth).toFixed(1)),
                    "Grid value mirrors the resolved azimuth")

            // The property drives the combo (a C++ coerce back to Grid must be
            // reflected), so setting it must move the selection.
            if (measurement.geoReferenced) {
                measurement.azimuthReference = AzimuthReference.True
                tryCompare(combo, "currentIndex", 1, 1000, "True selects row 1")
                let expected = measurement.referenceAvailable
                    ? qsTr("%1°").arg(Number(measurement.referenceAzimuth).toFixed(1))
                    : qsTr("n/a")
                compare(valueField.text, expected, "True value follows availability")

                measurement.copyToClipboard()
                verify(TestHelper.clipboardText().indexOf("Azimuth (true)") !== -1,
                       "Clipboard reports the True reference")
            } else {
                // Local-only project: True/Magnetic aren't selectable, so the
                // property snaps back to Grid and the combo stays on row 0.
                measurement.azimuthReference = AzimuthReference.True
                tryCompare(combo, "currentIndex", 0, 1000, "True coerces to Grid without a CRS")
                measurement.copyToClipboard()
                verify(TestHelper.clipboardText().indexOf("Azimuth (grid)") !== -1,
                       "Clipboard reports the Grid reference")
            }
        }
    }
}
