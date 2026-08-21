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

    RenderMemoryHud {
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
            text: "Render Memory HUD"
            checked: rootId.renderingSettings.showRenderMemoryHud
            checkable: true
            onTriggered: {
                rootId.renderingSettings.showRenderMemoryHud = !rootId.renderingSettings.showRenderMemoryHud
            }
        }
    }

    TestCase {
        name: "RenderMemoryHud"
        when: windowShown

        function init() {
            rootId.renderingSettings.showRenderMemoryHud = false
        }

        function cleanup() {
            rootId.renderingSettings.showRenderMemoryHud = false
        }

        function test_hiddenByDefault() {
            compare(hudId.visible, false)
        }

        function test_visibleWhenSettingEnabled() {
            rootId.renderingSettings.showRenderMemoryHud = true
            tryCompare(hudId, "visible", true)
        }

        function test_hiddenAgainAfterResetToDefaults() {
            rootId.renderingSettings.showRenderMemoryHud = true
            tryCompare(hudId, "visible", true)

            rootId.renderingSettings.resetToDefaults()
            tryCompare(hudId, "visible", false)
        }

        function test_totalIsShown() {
            rootId.renderingSettings.showRenderMemoryHud = true
            let total = ObjectFinder.findObjectByChain(rootId, "rootId->hud->renderMemoryHudTotal")
            verify(total !== null, "renderMemoryHudTotal not found")
            verify(total.text.length > 0)
        }

        function test_menuItemChecksAndWritesTheSetting() {
            compare(hudMenuItemId.checked, false)

            rootId.renderingSettings.showRenderMemoryHud = true
            tryCompare(hudMenuItemId, "checked", true)

            hudMenuItemId.triggered()
            tryCompare(rootId.renderingSettings, "showRenderMemoryHud", false)
            tryCompare(hudMenuItemId, "checked", false)

            hudMenuItemId.triggered()
            tryCompare(rootId.renderingSettings, "showRenderMemoryHud", true)
            tryCompare(hudMenuItemId, "checked", true)
        }
    }
}
