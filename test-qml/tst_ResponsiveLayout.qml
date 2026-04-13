import QtQuick
import QtTest
import QtQuick.Controls as QC
import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 1024
    height: 700

    MainSideBar {
        id: sidebarId
        objectName: "testSidebar"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        fileMenu: null
    }

    SideBarButton {
        id: standaloneButton
        objectName: "standaloneButton"
        text: "Test"
        image: "qrc:/twbs-icons/icons/box.svg"
        width: 80
        y: 500
    }

    TestCase {
        name: "ThemeLayoutSizeEnum"
        when: windowShown

        function test_enumValues() {
            compare(Theme.LayoutSize.Narrow, 0)
            compare(Theme.LayoutSize.Medium, 1)
            compare(Theme.LayoutSize.Wide, 2)
        }

        function test_breakpointValues() {
            verify(Theme.breakpointMedium < Theme.breakpointWide)
            compare(Theme.breakpointMedium, 500)
            compare(Theme.breakpointWide, 800)
        }

        function test_sidebarDimensionTokens() {
            verify(Theme.sidebarWidthCompact < Theme.sidebarWidthFull)
            compare(Theme.sidebarWidthFull, 80)
            compare(Theme.sidebarWidthCompact, 50)
        }
    }

    TestCase {
        name: "MainSideBarResponsive"
        when: windowShown

        function init() {
            sidebarId.layoutSize = Theme.LayoutSize.Wide
            waitForRendering(sidebarId)
        }

        function test_wideLayoutFullWidth() {
            sidebarId.layoutSize = Theme.LayoutSize.Wide
            compare(sidebarId.width, Theme.sidebarWidthFull)
        }

        function test_mediumLayoutCompactWidth() {
            sidebarId.layoutSize = Theme.LayoutSize.Medium
            compare(sidebarId.width, Theme.sidebarWidthCompact)
        }

        function test_viewButtonCompactInMedium() {
            sidebarId.layoutSize = Theme.LayoutSize.Medium
            waitForRendering(sidebarId)
            let viewBtn = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->viewButton")
            verify(viewBtn !== null, "viewButton not found")
            verify(viewBtn.compactMode, "viewButton should be compact in medium layout")
        }

        function test_viewButtonNotCompactInWide() {
            sidebarId.layoutSize = Theme.LayoutSize.Wide
            waitForRendering(sidebarId)
            let viewBtn = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->viewButton")
            verify(viewBtn !== null, "viewButton not found")
            verify(!viewBtn.compactMode, "viewButton should not be compact in wide layout")
        }

        function test_autoUpdateLabelVisibleInWide() {
            sidebarId.layoutSize = Theme.LayoutSize.Wide
            waitForRendering(sidebarId)
            let label = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->autoUpdateLabel")
            verify(label !== null, "autoUpdateLabel not found")
            verify(label.visible, "Auto-update label should be visible in wide layout")

            let checkbox = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->autoUpdateCheckbox")
            verify(checkbox !== null, "autoUpdateCheckbox not found")
            verify(checkbox.visible, "Checkbox should be visible in wide layout")

            let toggle = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->autoUpdateToggle")
            verify(toggle !== null, "autoUpdateToggle not found")
            verify(!toggle.visible, "Toggle button should be hidden in wide layout")
        }

        function test_autoUpdateToggleVisibleInMedium() {
            sidebarId.layoutSize = Theme.LayoutSize.Medium
            waitForRendering(sidebarId)
            let label = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->autoUpdateLabel")
            verify(label !== null, "autoUpdateLabel not found")
            verify(!label.visible, "Auto-update label should be hidden in medium layout")

            let checkbox = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->autoUpdateCheckbox")
            verify(checkbox !== null, "autoUpdateCheckbox not found")
            verify(!checkbox.visible, "Checkbox should be hidden in medium layout")

            let toggle = ObjectFinder.findObjectByChain(rootId, "rootId->testSidebar->autoUpdateToggle")
            verify(toggle !== null, "autoUpdateToggle not found")
            verify(toggle.visible, "Toggle button should be visible in medium layout")
        }
    }

    TestCase {
        name: "SideBarButtonCompactMode"
        when: windowShown

        function init() {
            standaloneButton.compactMode = false
            waitForRendering(standaloneButton)
        }

        function test_defaultNotCompact() {
            verify(!standaloneButton.compactMode)
        }

        function test_compactShrinksText() {
            let label = ObjectFinder.findObjectByChain(rootId, "rootId->standaloneButton->textLabel")
            verify(label !== null, "textLabel not found")

            standaloneButton.compactMode = false
            waitForRendering(standaloneButton)
            let normalSize = label.font.pixelSize

            standaloneButton.compactMode = true
            waitForRendering(standaloneButton)
            let compactSize = label.font.pixelSize

            verify(compactSize < normalSize,
                   "Compact text (" + compactSize + ") should be smaller than normal (" + normalSize + ")")
        }

        function test_compactReducesIconSize() {
            let icon = ObjectFinder.findObjectByChain(rootId, "rootId->standaloneButton->icon")
            verify(icon !== null, "icon not found")

            standaloneButton.compactMode = false
            waitForRendering(standaloneButton)
            let normalSize = icon.sourceSize.width

            standaloneButton.compactMode = true
            waitForRendering(standaloneButton)
            let compactSize = icon.sourceSize.width

            verify(compactSize < normalSize,
                   "Compact icon (" + compactSize + ") should be smaller than normal (" + normalSize + ")")
        }
    }
}
