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
