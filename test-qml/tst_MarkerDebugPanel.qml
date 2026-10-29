import QtQuick as QQ
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    readonly property RenderingSettings renderingSettings: RootData.settings.renderingSettings

    MarkerDebugPanel {
        id: panelId
        parent: rootId
    }

    // A view with no camera, scene or source: its ghost sits at the base knob.
    MarkerView {
        id: bareMarkerViewId
    }

    TestCase {
        name: "MarkerDebugPanel"
        when: windowShown

        readonly property list<string> rowNames: [
            "markerDepthTestCheckBox",
            "markerXRayCheckBox",
            "markerXRayOpacitySpinBox",
            "markerXRayOpacityReadout",
            "markerXRayOverheadOpacitySpinBox",
            "markerXRayRampStartSpinBox",
            "markerXRayRampEndSpinBox",
            "markerDebugResetButton",
            "markerDebugCloseButton"
        ]

        function init() {
            rootId.renderingSettings.resetToDefaults()
            panelId.open()
            tryCompare(panelId, "opened", true)
        }

        readonly property list<string> rampSpinBoxNames: [
            "markerXRayOverheadOpacitySpinBox",
            "markerXRayRampStartSpinBox",
            "markerXRayRampEndSpinBox"
        ]

        function cleanup() {
            panelId.close()
            panelId.markerView = null
            rootId.renderingSettings.resetToDefaults()
        }

        function row(name) {
            const found = findChild(panelId.contentItem, name)
            verify(found !== null, name + " not found")
            return found
        }

        function test_theKnobsStartAtTheirDefaults() {
            compare(rootId.renderingSettings.markerDepthTest, true)
            compare(rootId.renderingSettings.markerXRay, true)
            compare(rootId.renderingSettings.markerXRayOpacityPercent, 30)
            compare(rootId.renderingSettings.markerKnobsAtDefaults, true)

            compare(row("markerDepthTestCheckBox").checked, true)
            compare(row("markerXRayCheckBox").checked, true)
            compare(row("markerXRayOpacitySpinBox").value, 30)

            compare(rootId.renderingSettings.markerXRayOverheadOpacityPercent, 100)
            compare(rootId.renderingSettings.markerXRayRampStartTiltDegrees, 60)
            compare(rootId.renderingSettings.markerXRayRampEndTiltDegrees, 90)
            compare(row("markerXRayOverheadOpacitySpinBox").value, 100)
            compare(row("markerXRayRampStartSpinBox").value, 60)
            compare(row("markerXRayRampEndSpinBox").value, 90)
        }

        function test_theRampRowsRangeOverTheClampedLimits() {
            const overhead = row("markerXRayOverheadOpacitySpinBox")
            compare(overhead.from, rootId.renderingSettings.minimumMarkerXRayOverheadOpacityPercent)
            compare(overhead.to, rootId.renderingSettings.maximumMarkerXRayOverheadOpacityPercent)
            compare(overhead.from, 5)
            compare(overhead.to, 100)

            for (const name of ["markerXRayRampStartSpinBox", "markerXRayRampEndSpinBox"]) {
                const tilt = row(name)
                compare(tilt.from, rootId.renderingSettings.minimumMarkerXRayRampTiltDegrees, name)
                compare(tilt.to, rootId.renderingSettings.maximumMarkerXRayRampTiltDegrees, name)
                compare(tilt.from, 0, name)
                compare(tilt.to, 90, name)
            }
        }

        function test_everyKnobHasARow() {
            for (const name of rowNames) {
                row(name)
            }
        }

        function test_theOpacityRowRangesOverTheClampedLimits() {
            const opacity = row("markerXRayOpacitySpinBox")
            compare(opacity.from, rootId.renderingSettings.minimumMarkerXRayOpacityPercent)
            compare(opacity.to, rootId.renderingSettings.maximumMarkerXRayOpacityPercent)
            compare(opacity.from, 5)
            compare(opacity.to, 100)
        }

        function test_rowsFollowTheSettings() {
            rootId.renderingSettings.markerXRay = false
            rootId.renderingSettings.markerXRayOpacityPercent = 55
            rootId.renderingSettings.markerDepthTest = false

            tryCompare(row("markerXRayCheckBox"), "checked", false)
            tryCompare(row("markerXRayOpacitySpinBox"), "value", 55)
            tryCompare(row("markerDepthTestCheckBox"), "checked", false)
        }

        function test_rowsWriteTheSettings() {
            mouseClick(row("markerXRayCheckBox"))
            tryCompare(rootId.renderingSettings, "markerXRay", false)

            const opacity = row("markerXRayOpacitySpinBox")
            opacity.value = 70
            opacity.valueModified()
            compare(rootId.renderingSettings.markerXRayOpacityPercent, 70)

            mouseClick(row("markerDepthTestCheckBox"))
            tryCompare(rootId.renderingSettings, "markerDepthTest", false)
            compare(rootId.renderingSettings.markerKnobsAtDefaults, false)
        }

        function test_theRampRowsWriteTheSettings() {
            const overhead = row("markerXRayOverheadOpacitySpinBox")
            overhead.value = 75
            overhead.valueModified()
            compare(rootId.renderingSettings.markerXRayOverheadOpacityPercent, 75)

            const rampStart = row("markerXRayRampStartSpinBox")
            rampStart.value = 40
            rampStart.valueModified()
            compare(rootId.renderingSettings.markerXRayRampStartTiltDegrees, 40)

            const rampEnd = row("markerXRayRampEndSpinBox")
            rampEnd.value = 80
            rampEnd.valueModified()
            compare(rootId.renderingSettings.markerXRayRampEndTiltDegrees, 80)
            compare(rootId.renderingSettings.markerKnobsAtDefaults, false)
        }

        function test_theRampRowsFollowTheSettings() {
            rootId.renderingSettings.markerXRayOverheadOpacityPercent = 65
            rootId.renderingSettings.markerXRayRampStartTiltDegrees = 20
            rootId.renderingSettings.markerXRayRampEndTiltDegrees = 70

            tryCompare(row("markerXRayOverheadOpacitySpinBox"), "value", 65)
            tryCompare(row("markerXRayRampStartSpinBox"), "value", 20)
            tryCompare(row("markerXRayRampEndSpinBox"), "value", 70)
        }

        function test_theReadoutShowsTheViewsLiveOpacity() {
            const readout = row("markerXRayOpacityReadout")
            panelId.markerView = bareMarkerViewId
            tryCompare(readout, "text", "now 30%")

            rootId.renderingSettings.markerXRayOpacityPercent = 55
            tryCompare(readout, "text", "now 55%")
        }

        function test_theXRayRowsDisableWhileTheDepthTestIsOff() {
            const xRay = row("markerXRayCheckBox")
            const opacity = row("markerXRayOpacitySpinBox")
            compare(xRay.enabled, true)
            compare(opacity.enabled, true)

            for (const name of rampSpinBoxNames) {
                compare(row(name).enabled, true, name)
            }

            rootId.renderingSettings.markerDepthTest = false
            tryCompare(xRay, "enabled", false)
            tryCompare(opacity, "enabled", false)
            for (const name of rampSpinBoxNames) {
                tryCompare(row(name), "enabled", false)
            }
            compare(row("markerDepthTestCheckBox").enabled, true)

            rootId.renderingSettings.markerDepthTest = true
            tryCompare(xRay, "enabled", true)
            tryCompare(opacity, "enabled", true)
            for (const name of rampSpinBoxNames) {
                tryCompare(row(name), "enabled", true)
            }
        }

        function test_resetRestoresTheMarkerKnobsAndLeavesTheHudAlone() {
            rootId.renderingSettings.showRenderStatsHud = true
            rootId.renderingSettings.freezeCut = true
            rootId.renderingSettings.markerDepthTest = false
            rootId.renderingSettings.markerXRay = false
            rootId.renderingSettings.markerXRayOpacityPercent = 55
            rootId.renderingSettings.markerXRayOverheadOpacityPercent = 65
            rootId.renderingSettings.markerXRayRampStartTiltDegrees = 20
            rootId.renderingSettings.markerXRayRampEndTiltDegrees = 70
            compare(rootId.renderingSettings.markerXRayOverheadOpacityPercent, 65)
            compare(rootId.renderingSettings.markerXRayRampStartTiltDegrees, 20)
            compare(rootId.renderingSettings.markerXRayRampEndTiltDegrees, 70)
            compare(rootId.renderingSettings.markerKnobsAtDefaults, false)

            mouseClick(row("markerDebugResetButton"))
            tryCompare(rootId.renderingSettings, "markerKnobsAtDefaults", true)
            compare(rootId.renderingSettings.markerDepthTest, true)
            compare(rootId.renderingSettings.markerXRay, true)
            compare(rootId.renderingSettings.markerXRayOpacityPercent, 30)
            tryCompare(row("markerDepthTestCheckBox"), "checked", true)
            tryCompare(row("markerXRayCheckBox"), "checked", true)
            tryCompare(row("markerXRayOpacitySpinBox"), "value", 30)
            compare(rootId.renderingSettings.markerXRayOverheadOpacityPercent, 100)
            compare(rootId.renderingSettings.markerXRayRampStartTiltDegrees, 60)
            compare(rootId.renderingSettings.markerXRayRampEndTiltDegrees, 90)
            tryCompare(row("markerXRayOverheadOpacitySpinBox"), "value", 100)
            tryCompare(row("markerXRayRampStartSpinBox"), "value", 60)
            tryCompare(row("markerXRayRampEndSpinBox"), "value", 90)

            compare(rootId.renderingSettings.showRenderStatsHud, true)
            compare(rootId.renderingSettings.freezeCut, true)
        }

        function test_closeButtonClosesThePanel() {
            mouseClick(row("markerDebugCloseButton"))
            tryCompare(panelId, "opened", false)
        }
    }

    TestCase {
        name: "MarkerDebugPanelFromTheHud"
        when: windowShown

        function init() {
            rootId.renderingSettings.resetToDefaults()
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem.objectName === "viewPage")
        }

        function cleanup() {
            rootId.renderingSettings.resetToDefaults()
        }

        function renderer() {
            const found = findChild(RootData.pageView.currentPageItem, "renderer")
            verify(found !== null, "renderer not found")
            return found
        }

        function hudButton(view, name) {
            let button = null
            tryVerify(() => {
                button = findChild(view, name)
                return button !== null && button.visible
            }, 5000, name + " should show in the HUD")
            return button
        }

        function test_theHudOpensThePanel() {
            rootId.renderingSettings.showRenderStatsHud = true

            const view = renderer()
            const button = hudButton(view, "renderStatsHudMarkerPanelButton")
            compare(button.text, qsTr("Marker knobs…"))

            const panel = findChild(view, "markerDebugPanel")
            verify(panel !== null, "markerDebugPanel not found")
            compare(panel.opened, false)

            mouseClick(button)
            tryCompare(panel, "opened", true)

            // The open panel leaves the view's shortcuts live.
            view.forceActiveFocus()
            tryVerify(() => view.activeFocus)
            keyClick(Qt.Key_F)
            tryCompare(rootId.renderingSettings, "freezeCut", true)
            compare(panel.opened, true)

            mouseClick(view, view.width / 2, view.height / 2)
            compare(panel.opened, true)
            panel.close()
        }

        function test_thePanelReadsOutTheEntranceView() {
            const view = renderer()
            const panel = findChild(view, "markerDebugPanel")
            const entranceView = findChild(view, "entranceView")
            verify(panel !== null, "markerDebugPanel not found")
            verify(entranceView !== null, "entranceView not found")
            compare(panel.markerView, entranceView)
        }

        function test_openingOnePanelClosesTheOther() {
            rootId.renderingSettings.showRenderStatsHud = true

            const view = renderer()
            const markerButton = hudButton(view, "renderStatsHudMarkerPanelButton")
            const pointCloudButton = hudButton(view, "renderStatsHudDebugPanelButton")
            const markerPanel = findChild(view, "markerDebugPanel")
            const pointCloudPanel = findChild(view, "pointCloudDebugPanel")
            verify(markerPanel !== null, "markerDebugPanel not found")
            verify(pointCloudPanel !== null, "pointCloudDebugPanel not found")

            mouseClick(markerButton)
            tryCompare(markerPanel, "opened", true)

            mouseClick(pointCloudButton)
            tryCompare(pointCloudPanel, "opened", true)
            tryCompare(markerPanel, "opened", false)

            mouseClick(markerButton)
            tryCompare(markerPanel, "opened", true)
            tryCompare(pointCloudPanel, "opened", false)
            markerPanel.close()
        }
    }
}
