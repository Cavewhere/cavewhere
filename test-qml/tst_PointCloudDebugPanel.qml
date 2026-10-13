import QtQuick as QQ
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    readonly property RenderingSettings renderingSettings: RootData.settings.renderingSettings
    readonly property LazLayersSceneNode lazLayersSceneNode: RootData.regionSceneManager.lazLayersSceneNode

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
            "evictionHoldCheckBox",
            "maxLevelSpinBox",
            "gpuMemoryBudgetSpinBox",
            "cpuCacheBudgetSpinBox",
            "uploadBudgetSpinBox",
            "pointBudgetSpinBox",
            "screenSpaceErrorSpinBox",
            "thinningModeComboBox",
            "sizeModeComboBox",
            "fixedPointSizeSpinBox",
            "floorModeComboBox",
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

            tryCompare(row("thinningModeComboBox"), "currentIndex", 2)
            tryCompare(row("freezeCutCheckBox"), "checked", true)
            tryCompare(row("maxLevelSpinBox"), "value", 3)
            tryCompare(row("maxPointSizeSpinBox"), "value", 12)
        }

        function test_rowsWriteTheSettings() {
            const sizeMode = row("sizeModeComboBox")
            sizeMode.currentIndex = 1
            sizeMode.activated(1)
            compare(rootId.renderingSettings.sizeMode, 1)

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
            rootId.renderingSettings.thinningMode = 1
            rootId.renderingSettings.floorMode = 1
            rootId.renderingSettings.screenSpaceErrorPx = 4
            rootId.lazLayersSceneNode.spacingCoverage = 3
            rootId.renderingSettings.showRenderStatsHud = true
            verify(!rootId.renderingSettings.isAtDefaults)

            mouseClick(row("pointCloudDebugResetButton"))

            tryCompare(rootId.renderingSettings, "freezeCut", false)
            compare(rootId.renderingSettings.inflationGovernor, true)
            compare(rootId.renderingSettings.thinningMode, 0)
            compare(rootId.renderingSettings.floorMode, 0)
            compare(rootId.renderingSettings.screenSpaceErrorPx, 1.5)
            compare(rootId.lazLayersSceneNode.spacingCoverage,
                    rootId.lazLayersSceneNode.defaultSpacingCoverage)

            // The panel is opened from the HUD, so a reset leaves the HUD up.
            compare(rootId.renderingSettings.showRenderStatsHud, true)

            tryCompare(row("freezeCutCheckBox"), "checked", false)
            tryCompare(row("inflationGovernorCheckBox"), "checked", true)
            tryCompare(row("thinningModeComboBox"), "currentIndex", 0)
            tryCompare(row("floorModeComboBox"), "currentIndex", 0)
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
