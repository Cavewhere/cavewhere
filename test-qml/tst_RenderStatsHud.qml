import QtQuick as QQ
import QtQuick.Controls as QC
import QtTest
import QmlTestRecorder
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "rootId"

    readonly property RenderingSettings renderingSettings: RootData.settings.renderingSettings

    width: 600
    height: 400

    RenderStatsHud {
        id: hudId
        objectName: "hud"
        anchors.top: parent.top
        anchors.right: parent.right
    }

    // The same ledger the HUD's own model reads, so the test can state the
    // over-budget comparison in terms of real totals.
    RenderingStatsModel {
        id: statsModelId
        running: true
    }

    // The File → Debug toggle, written the way FileMenu.qml writes it: checked
    // reads the setting, triggering writes the inverse.
    QC.Menu {
        id: debugMenuId

        QC.MenuItem {
            id: hudMenuItemId
            objectName: "hudMenuItem"
            text: "Render Stats HUD"
            checked: rootId.renderingSettings.showRenderStatsHud
            checkable: true
            onTriggered: {
                rootId.renderingSettings.showRenderStatsHud = !rootId.renderingSettings.showRenderStatsHud
            }
        }
    }

    TestCase {
        name: "RenderStatsHud"
        when: windowShown

        function init() {
            rootId.renderingSettings.showRenderStatsHud = false
        }

        function cleanup() {
            rootId.renderingSettings.showRenderStatsHud = false
        }

        function test_hiddenByDefault() {
            compare(hudId.visible, false)
        }

        function test_visibleWhenSettingEnabled() {
            rootId.renderingSettings.showRenderStatsHud = true
            tryCompare(hudId, "visible", true)
        }

        function test_hiddenAgainAfterResetToDefaults() {
            rootId.renderingSettings.showRenderStatsHud = true
            tryCompare(hudId, "visible", true)

            rootId.renderingSettings.resetToDefaults()
            tryCompare(hudId, "visible", false)
        }

        function test_totalIsShown() {
            rootId.renderingSettings.showRenderStatsHud = true
            let total = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderStatsHudTotal")
            verify(total !== null, "renderStatsHudTotal not found")
            verify(total.text.length > 0)
        }

        function test_streamingRowIsShown() {
            rootId.renderingSettings.showRenderStatsHud = true
            let streaming = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderStatsHudStreaming")
            verify(streaming !== null, "renderStatsHudStreaming not found")
            verify(streaming.text.indexOf("Streaming:") === 0,
                   "unexpected streaming row text: " + streaming.text)
        }

        function test_pointCloudRowIsShown() {
            rootId.renderingSettings.showRenderStatsHud = true
            tryCompare(hudId, "visible", true)

            let pointCloud = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderStatsHudPointCloud")
            verify(pointCloud !== null, "renderStatsHudPointCloud not found")
            compare(pointCloud.text,
                    "Point cloud: " + statsModelId.residentNodes
                    + " / " + statsModelId.selectedNodes
                    + " nodes · " + statsModelId.nodeLoadsInFlight + " loading")

            let points = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderStatsHudPointCloudPoints")
            verify(points !== null, "renderStatsHudPointCloudPoints not found")
            compare(points.text,
                    " · points " + statsModelId.selectedPointsText
                    + " / " + rootId.renderingSettings.pointBudgetMillions + " M")

            let mirrors = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderStatsHudPointCloudMirrors")
            verify(mirrors !== null, "renderStatsHudPointCloudMirrors not found")
            compare(mirrors.text, " · mirrors " + statsModelId.pickMirrorText)

            // The multiplier is a budget-pressure signal, so it stays off the
            // row until a view actually coarsens its cut. Nothing in QML can
            // raise the inflation — the C++ [PointCloudStreaming] stats case
            // covers the frame that does.
            let sse = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderStatsHudPointCloudSse")
            verify(sse !== null, "renderStatsHudPointCloudSse not found")
            compare(sse.visible, statsModelId.sseInflation > 1)
            compare(sse.text,
                    " · SSE ×" + statsModelId.sseInflation.toFixed(hudId.inflationDecimals))
        }

        // The HUD reads the byte budget off the settings object instead of doing
        // its own megabyte math, so overBudget follows the same comparison the
        // render thread makes.
        // The budget half of the points reading is bound to the setting, so a
        // user who raises the point budget sees the row follow.
        function test_pointRowFollowsThePointBudgetSetting() {
            rootId.renderingSettings.showRenderStatsHud = true
            tryCompare(hudId, "visible", true)

            let points = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderStatsHudPointCloudPoints")
            verify(points !== null, "renderStatsHudPointCloudPoints not found")

            rootId.renderingSettings.pointBudgetMillions = 64
            tryCompare(points, "text",
                       " · points " + statsModelId.selectedPointsText + " / 64 M")

            rootId.renderingSettings.resetToDefaults()
            tryCompare(points, "text",
                       " · points " + statsModelId.selectedPointsText + " / "
                       + rootId.renderingSettings.pointBudgetMillions + " M")
        }

        function test_overBudgetFollowsTheSettingsByteBudget() {
            rootId.renderingSettings.showRenderStatsHud = true
            tryCompare(hudId, "visible", true)

            const bytesPerMegabyte = 1024 * 1024

            rootId.renderingSettings.gpuMemoryBudgetMb =
                    rootId.renderingSettings.maximumGpuMemoryBudgetMb
            compare(rootId.renderingSettings.gpuBudgetBytes,
                    rootId.renderingSettings.maximumGpuMemoryBudgetMb * bytesPerMegabyte)
            tryCompare(hudId, "overBudget",
                       statsModelId.totalGpuBytes > rootId.renderingSettings.gpuBudgetBytes)

            rootId.renderingSettings.gpuMemoryBudgetMb =
                    rootId.renderingSettings.minimumGpuMemoryBudgetMb
            compare(rootId.renderingSettings.gpuBudgetBytes,
                    rootId.renderingSettings.minimumGpuMemoryBudgetMb * bytesPerMegabyte)
            tryCompare(hudId, "overBudget",
                       statsModelId.totalGpuBytes > rootId.renderingSettings.gpuBudgetBytes)

            rootId.renderingSettings.resetToDefaults()
        }

        function test_menuItemChecksAndWritesTheSetting() {
            compare(hudMenuItemId.checked, false)

            rootId.renderingSettings.showRenderStatsHud = true
            tryCompare(hudMenuItemId, "checked", true)

            hudMenuItemId.triggered()
            tryCompare(rootId.renderingSettings, "showRenderStatsHud", false)
            tryCompare(hudMenuItemId, "checked", false)

            hudMenuItemId.triggered()
            tryCompare(rootId.renderingSettings, "showRenderStatsHud", true)
            tryCompare(hudMenuItemId, "checked", true)
        }
    }
}
