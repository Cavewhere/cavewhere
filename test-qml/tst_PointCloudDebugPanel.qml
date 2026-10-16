import QtQuick as QQ
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    readonly property RenderingSettings renderingSettings: RootData.settings.renderingSettings
    readonly property LazLayersSceneNode lazLayersSceneNode: RootData.regionSceneManager.lazLayersSceneNode
    // cw::pointcloud::DebugView::SpriteDensity
    readonly property int spriteDensityView: 9

    PointCloudDebugPanel {
        id: panelId
        parent: rootId
    }

    TestCase {
        name: "PointCloudDebugPanel"
        when: windowShown

        readonly property list<string> rowNames: [
            "freezeCutCheckBox",
            "inflationGovernorCheckBox",
            "governorSlewSpinBox",
            "evictionHoldCheckBox",
            "maxLevelSpinBox",
            "minLevelSpinBox",
            "debugViewComboBox",
            "showNodeBoxesCheckBox",
            "gpuMemoryBudgetSpinBox",
            "cpuCacheBudgetSpinBox",
            "uploadBudgetSpinBox",
            "pointBudgetSpinBox",
            "screenSpaceErrorSpinBox",
            "thinningModeComboBox",
            "sizeModeComboBox",
            "fixedPointSizeSpinBox",
            "floorModeComboBox",
            "spacingSourceComboBox",
            "revealFramesSpinBox",
            "revealModeComboBox",
            "spacingCoverageSpinBox",
            "maxPointSizeSpinBox",
            "pointCloudDebugCounts",
            "pointCloudDebugResetButton",
            "pointCloudDebugCloseButton"
        ]

        function init() {
            rootId.renderingSettings.resetToDefaults()
            rootId.lazLayersSceneNode.spacingCoverage = rootId.lazLayersSceneNode.defaultSpacingCoverage
            panelId.open()
            tryCompare(panelId, "opened", true)
        }

        function cleanup() {
            panelId.close()
            rootId.renderingSettings.resetToDefaults()
            rootId.lazLayersSceneNode.spacingCoverage = rootId.lazLayersSceneNode.defaultSpacingCoverage
        }

        function row(name) {
            const found = findChild(panelId.contentItem, name)
            verify(found !== null, name + " not found")
            return found
        }

        function test_theColorByRowNamesEveryViewThroughSpriteDensity() {
            compare(panelId.debugViewNames.length, rootId.renderingSettings.debugViewCount)
            compare(panelId.debugViewNames[rootId.spriteDensityView], qsTr("Sprite density"))

            const debugView = row("debugViewComboBox")
            debugView.currentIndex = rootId.spriteDensityView
            debugView.activated(rootId.spriteDensityView)
            compare(rootId.renderingSettings.debugView, rootId.spriteDensityView)

            rootId.renderingSettings.debugView = 0
            tryCompare(debugView, "currentIndex", 0)
            rootId.renderingSettings.debugView = rootId.spriteDensityView
            tryCompare(debugView, "currentIndex", rootId.spriteDensityView)
        }

        function test_everyKnobHasARow() {
            for (const name of rowNames) {
                row(name)
            }
        }

        function test_rowsFollowTheSettings() {
            rootId.renderingSettings.thinningMode = 2
            rootId.renderingSettings.freezeCut = true
            rootId.renderingSettings.maxLevel = 3
            rootId.renderingSettings.maxPointSizePx = 12
            rootId.renderingSettings.debugView = 4
            rootId.renderingSettings.showNodeBoxes = true
            rootId.renderingSettings.minLevel = 2

            tryCompare(row("thinningModeComboBox"), "currentIndex", 2)
            tryCompare(row("freezeCutCheckBox"), "checked", true)
            tryCompare(row("maxLevelSpinBox"), "value", 3)
            tryCompare(row("maxPointSizeSpinBox"), "value", 12)
            tryCompare(row("debugViewComboBox"), "currentIndex", 4)
            tryCompare(row("showNodeBoxesCheckBox"), "checked", true)
            tryCompare(row("minLevelSpinBox"), "value", 2)
        }

        function test_rowsWriteTheSettings() {
            const sizeMode = row("sizeModeComboBox")
            sizeMode.currentIndex = 1
            sizeMode.activated(1)
            compare(rootId.renderingSettings.sizeMode, 1)

            const debugView = row("debugViewComboBox")
            debugView.currentIndex = 8
            debugView.activated(8)
            compare(rootId.renderingSettings.debugView, 8)

            mouseClick(row("showNodeBoxesCheckBox"))
            tryCompare(rootId.renderingSettings, "showNodeBoxes", true)

            const minLevel = row("minLevelSpinBox")
            minLevel.value = 3
            minLevel.valueModified()
            compare(rootId.renderingSettings.minLevel, 3)

            const evictionHold = row("evictionHoldCheckBox")
            mouseClick(evictionHold)
            tryCompare(rootId.renderingSettings, "evictionHold", true)

            const pointBudget = row("pointBudgetSpinBox")
            pointBudget.value = 32
            pointBudget.valueModified()
            compare(rootId.renderingSettings.pointBudgetMillions, 32)

            const coverage = row("spacingCoverageSpinBox")
            coverage.value = 300
            coverage.valueModified()
            compare(rootId.lazLayersSceneNode.spacingCoverage, 3)

            verify(!rootId.renderingSettings.isAtDefaults)
        }

        function test_resetReturnsEveryKnobToItsDefault() {
            rootId.renderingSettings.freezeCut = true
            rootId.renderingSettings.inflationGovernor = false
            rootId.renderingSettings.governorSlew = 0.25
            rootId.renderingSettings.thinningMode = 1
            rootId.renderingSettings.floorMode = 1
            rootId.renderingSettings.debugView = 2
            rootId.renderingSettings.showNodeBoxes = true
            rootId.renderingSettings.minLevel = 1
            rootId.renderingSettings.revealFrames = 8
            rootId.renderingSettings.revealMode = 1
            rootId.renderingSettings.screenSpaceErrorPx = 4
            rootId.lazLayersSceneNode.spacingCoverage = 3
            rootId.renderingSettings.showRenderStatsHud = true
            verify(!rootId.renderingSettings.isAtDefaults)

            mouseClick(row("pointCloudDebugResetButton"))

            tryCompare(rootId.renderingSettings, "freezeCut", false)
            compare(rootId.renderingSettings.inflationGovernor, true)
            compare(rootId.renderingSettings.governorSlew, 1)
            compare(rootId.renderingSettings.thinningMode, 0)
            compare(rootId.renderingSettings.floorMode, 0)
            compare(rootId.renderingSettings.debugView, 0)
            compare(rootId.renderingSettings.showNodeBoxes, false)
            compare(rootId.renderingSettings.minLevel, 0)
            compare(rootId.renderingSettings.revealFrames, 0)
            compare(rootId.renderingSettings.revealMode, 0)
            compare(rootId.renderingSettings.screenSpaceErrorPx, 1.5)
            compare(rootId.lazLayersSceneNode.spacingCoverage,
                    rootId.lazLayersSceneNode.defaultSpacingCoverage)

            // The panel is opened from the HUD, so a reset leaves the HUD up.
            compare(rootId.renderingSettings.showRenderStatsHud, true)

            tryCompare(row("freezeCutCheckBox"), "checked", false)
            tryCompare(row("inflationGovernorCheckBox"), "checked", true)
            tryCompare(row("thinningModeComboBox"), "currentIndex", 0)
            tryCompare(row("floorModeComboBox"), "currentIndex", 0)
            tryCompare(row("debugViewComboBox"), "currentIndex", 0)
            tryCompare(row("showNodeBoxesCheckBox"), "checked", false)
        }

        function test_governorSlewRowFollowsTheSetting() {
            const slew = row("governorSlewSpinBox")
            rootId.renderingSettings.governorSlew = 0.25
            tryCompare(slew, "value", Math.round(0.25 * slew.valueScale))
            compare(slew.from, Math.round(rootId.renderingSettings.minimumGovernorSlew * slew.valueScale))
            compare(slew.to, Math.round(rootId.renderingSettings.maximumGovernorSlew * slew.valueScale))
        }

        function test_governorSlewRowWritesTheSetting() {
            const slew = row("governorSlewSpinBox")
            slew.value = Math.round(0.5 * slew.valueScale)
            slew.valueModified()
            compare(rootId.renderingSettings.governorSlew, 0.5)
            verify(!rootId.renderingSettings.isAtDefaults)

            mouseClick(row("pointCloudDebugResetButton"))
            tryCompare(rootId.renderingSettings, "governorSlew", 1)
            tryCompare(slew, "value", slew.valueScale)
        }

        function test_spacingSourceRowFollowsTheSetting() {
            const source = row("spacingSourceComboBox")
            compare(source.count, rootId.renderingSettings.spacingSourceCount)
            compare(source.currentIndex, 1)

            rootId.renderingSettings.spacingSource = 0
            tryCompare(source, "currentIndex", 0)
        }

        function test_spacingSourceRowWritesTheSetting() {
            const source = row("spacingSourceComboBox")
            source.currentIndex = 0
            source.activated(0)
            compare(rootId.renderingSettings.spacingSource, 0)
            verify(!rootId.renderingSettings.isAtDefaults)

            mouseClick(row("pointCloudDebugResetButton"))
            tryCompare(rootId.renderingSettings, "spacingSource", 1)
            tryCompare(source, "currentIndex", 1)
        }

        function test_spacingSourceRowNamesEverySource() {
            const source = row("spacingSourceComboBox")
            compare(source.count, 3)
            compare(source.textAt(0), "Grid")
            compare(source.textAt(1), "Measured")
            compare(source.textAt(2), "Local")
        }

        function test_spacingSourceRowWritesLocal() {
            const source = row("spacingSourceComboBox")
            source.currentIndex = 2
            source.activated(2)
            compare(rootId.renderingSettings.spacingSource, 2)
            verify(!rootId.renderingSettings.isAtDefaults)

            rootId.renderingSettings.spacingSource = 1
            tryCompare(source, "currentIndex", 1)
            rootId.renderingSettings.spacingSource = 2
            tryCompare(source, "currentIndex", 2)

            mouseClick(row("pointCloudDebugResetButton"))
            tryCompare(rootId.renderingSettings, "spacingSource", 1)
            tryCompare(source, "currentIndex", 1)
        }

        function test_revealFramesRowFollowsTheSetting() {
            const reveal = row("revealFramesSpinBox")
            compare(reveal.value, 0)
            compare(reveal.from, rootId.renderingSettings.minimumRevealFrames)
            compare(reveal.to, rootId.renderingSettings.maximumRevealFrames)

            rootId.renderingSettings.revealFrames = 8
            tryCompare(reveal, "value", 8)
        }

        function test_revealFramesRowWritesTheSetting() {
            const reveal = row("revealFramesSpinBox")
            reveal.value = 12
            reveal.valueModified()
            compare(rootId.renderingSettings.revealFrames, 12)
            verify(!rootId.renderingSettings.isAtDefaults)

            mouseClick(row("pointCloudDebugResetButton"))
            tryCompare(rootId.renderingSettings, "revealFrames", 0)
            tryCompare(reveal, "value", 0)
        }

        function test_revealModeRowFollowsTheSetting() {
            const mode = row("revealModeComboBox")
            compare(mode.count, rootId.renderingSettings.revealModeCount)
            compare(mode.textAt(0), "Cover")
            compare(mode.textAt(1), "Grow")
            compare(mode.currentIndex, 0)

            rootId.renderingSettings.revealMode = 1
            tryCompare(mode, "currentIndex", 1)
        }

        function test_revealModeRowWritesTheSetting() {
            const mode = row("revealModeComboBox")
            mode.currentIndex = 1
            mode.activated(1)
            compare(rootId.renderingSettings.revealMode, 1)
            verify(!rootId.renderingSettings.isAtDefaults)

            mouseClick(row("pointCloudDebugResetButton"))
            tryCompare(rootId.renderingSettings, "revealMode", 0)
            tryCompare(mode, "currentIndex", 0)
        }

        function test_closeButtonClosesThePanel() {
            mouseClick(row("pointCloudDebugCloseButton"))
            tryCompare(panelId, "opened", false)
        }
    }

    TestCase {
        name: "PointCloudDebugKeys"
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

        function test_tCyclesThinningAndFTogglesTheFreeze() {
            const view = renderer()
            view.forceActiveFocus()
            tryVerify(() => view.activeFocus)

            for (let step = 1; step <= rootId.renderingSettings.thinningModeCount; step++) {
                keyClick(Qt.Key_T)
                tryCompare(rootId.renderingSettings, "thinningMode",
                           step % rootId.renderingSettings.thinningModeCount)
            }

            keyClick(Qt.Key_F)
            tryCompare(rootId.renderingSettings, "freezeCut", true)
            keyClick(Qt.Key_F)
            tryCompare(rootId.renderingSettings, "freezeCut", false)
        }

        function test_vCyclesTheDebugView() {
            const view = renderer()
            view.forceActiveFocus()
            tryVerify(() => view.activeFocus)

            for (let step = 1; step <= rootId.renderingSettings.debugViewCount; step++) {
                keyClick(Qt.Key_V)
                tryCompare(rootId.renderingSettings, "debugView",
                           step % rootId.renderingSettings.debugViewCount)
            }
        }

        function test_vCyclesThroughSpriteDensityBackToOff() {
            const view = renderer()
            view.forceActiveFocus()
            tryVerify(() => view.activeFocus)

            rootId.renderingSettings.debugView = rootId.spriteDensityView - 1
            keyClick(Qt.Key_V)
            tryCompare(rootId.renderingSettings, "debugView", rootId.spriteDensityView)
            keyClick(Qt.Key_V)
            tryCompare(rootId.renderingSettings, "debugView", 0)
        }

        function test_theHudOpensThePanel() {
            rootId.renderingSettings.showRenderStatsHud = true

            const view = renderer()
            let button = null
            tryVerify(() => {
                button = findChild(view, "renderStatsHudDebugPanelButton")
                return button !== null && button.visible
            })

            const panel = findChild(view, "pointCloudDebugPanel")
            verify(panel !== null, "pointCloudDebugPanel not found")
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
    }
}
