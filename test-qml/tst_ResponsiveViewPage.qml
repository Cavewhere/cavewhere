import QtQuick
import QtTest
import QtQuick.Controls as QC
import QmlTestRecorder
import cavewherelib

MainWindowTest {
    id: rootId

    Component.onCompleted: {
        rootId.mainWindow.objectName = "mainWindow"
    }

    // Loads whatever icon source it is handed, so a test can tell a resource that
    // resolves from one that only looks right as a string.
    Image {
        id: iconProbeId
        visible: false
    }

    TestCase {
        name: "ViewPageResponsive"
        when: windowShown

        readonly property string viewPageChain: "rootId->mainWindow->viewPage"

        function navigateToView() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function findViewPage() {
            navigateToView()
            let vp = ObjectFinder.findObjectByChain(rootId, viewPageChain)
            verify(vp !== null, "viewPage not found")
            return vp
        }

        function init() {
            rootId.width = 1024
            waitForRendering(rootId)
        }

        function cleanup() {
            // Disarmed here rather than at the end of the test that arms one, so
            // a failed assertion cannot leave a tool armed for everything that
            // runs after it — including the other TestCase in this file.
            let viewPage = ObjectFinder.findObjectByChain(rootId, viewPageChain)
            if (viewPage !== null && viewPage.interactionManager !== null) {
                viewPage.interactionManager.activeDefaultInteraction()
            }

            rootId.width = 1200
            waitForRendering(rootId)
        }

        function test_isNarrowAtSmallWidth() {
            rootId.width = 400
            let viewPage = findViewPage()
            verify(viewPage.isNarrow, "Should be narrow at 400")
        }

        function test_isNotNarrowAtWideWidth() {
            rootId.width = 1024
            let viewPage = findViewPage()
            verify(!viewPage.isNarrow, "Should not be narrow at 1024")
        }

        function test_floatingButtonsVisibleAtNarrow() {
            rootId.width = 400
            waitForRendering(rootId)
            navigateToView()

            let buttons = findChild(rootId, "floatingButtons")
            verify(buttons !== null, "floatingButtons not found")
            verify(buttons.visible, "Floating buttons should be visible at narrow width")
        }

        function test_floatingButtonsHiddenAtWide() {
            rootId.width = 1024
            waitForRendering(rootId)
            navigateToView()

            let buttons = findChild(rootId, "floatingButtons")
            verify(buttons !== null, "floatingButtons not found")
            verify(!buttons.visible, "Floating buttons should be hidden at wide width")
        }

        // At narrow widths the sidebar's tool rail is gone, so this button is
        // both the way to the tools and the only report of which one is armed.
        function test_toolsButtonWearsTheArmedToolsIcon() {
            rootId.width = 400
            waitForRendering(rootId)
            let viewPage = findViewPage()

            let toolsButton = findChild(rootId, "toolsButton")
            verify(toolsButton !== null, "toolsButton not found")
            verify(viewPage.armedTool === null, "nothing armed to begin with")
            compare(toolsButton.icon.color, Theme.text, "so it is a plain button")

            let tool = viewPage.tools[0]
            verify(tool !== null, "the View page publishes tools")
            tool.interaction.activate()

            tryVerify(function() { return viewPage.armedTool === tool }, 2000,
                      "the page sees the armed tool")
            compare(toolsButton.icon.source, tool.iconSource,
                    "and the button wears its icon")
            compare(toolsButton.icon.color, Theme.accent, "lit up while a tool is armed")
        }

        // The unarmed fallback is the branch nobody looks at, and a source string
        // is right whether or not the file behind it was ever put in the qrc —
        // so this loads it. Icons are listed in cwResources.qrc by hand.
        function test_theUnarmedToolsButtonHasAnIconThatLoads() {
            rootId.width = 400
            waitForRendering(rootId)
            let viewPage = findViewPage()

            let toolsButton = findChild(rootId, "toolsButton")
            verify(toolsButton !== null, "toolsButton not found")
            verify(viewPage.armedTool === null, "nothing armed, so it wears the fallback")

            iconProbeId.source = toolsButton.icon.source
            tryCompare(iconProbeId, "status", Image.Ready, 2000,
                       "the fallback icon is a resource that exists: " + toolsButton.icon.source)
        }

        // The Tools tab is the sidebar rail's stand-in, so it belongs only where
        // that rail is not on screen. Asserted on the wide side panel, which is
        // the case that would otherwise ship two ways to arm the same tool — the
        // narrow half lives in a closed drawer, whose own invisibility would
        // make a `visible` check say true regardless of the binding.
        function test_theToolsTabGivesWayToTheSidebarRail() {
            rootId.width = 1024
            waitForRendering(rootId)
            findViewPage()

            let tab = findChild(rootId, "toolsTabButton")
            verify(tab !== null, "toolsTabButton not found")
            verify(findChild(rootId, "layersTabButton").visible,
                   "the side panel itself is on screen")
            verify(!tab.visible, "but the Tools tab is not offered beside the rail")
        }

        // Hiding the tab button does not move the bar off it, so the panel would
        // come back up on a page the rail already owns, with no tab looking
        // selected and no way back except picking another one.
        function test_wideningLeavesTheToolsTabBehind() {
            rootId.width = 400
            waitForRendering(rootId)
            let viewPage = findViewPage()

            let tabBar = findChild(rootId, "renderingTabBar")
            verify(tabBar !== null, "renderingTabBar not found")

            // Selected directly rather than through the floating button, whose
            // clicks do not land under the offscreen platform this runs on — the
            // three drawer tests in this file fail for that same reason.
            tabBar.currentIndex = viewPage.toolsTabIndex
            compare(tabBar.currentIndex, viewPage.toolsTabIndex, "the Tools tab is selected")

            rootId.width = 1024
            waitForRendering(rootId)

            verify(tabBar.currentIndex !== viewPage.toolsTabIndex,
                   "widening moved the panel off the tab it no longer offers")
        }

        function test_drawerOpensOnCameraClick() {
            rootId.width = 400
            waitForRendering(rootId)
            navigateToView()

            let viewPage = ObjectFinder.findObjectByChain(rootId, viewPageChain)
            verify(viewPage !== null, "viewPage not found")
            let drawer = viewPage.viewDrawer
            verify(drawer !== null, "viewDrawer not found")

            // Ensure drawer is fully closed and animation settled from any prior test
            tryVerify(function() { return drawer.position === 0 }, 2000, "Drawer should start fully closed")

            let cameraBtn = findChild(rootId, "cameraButton")
            verify(cameraBtn !== null, "cameraButton not found")
            tryVerify(function() { return cameraBtn.visible && cameraBtn.width > 0 }, 1000, "cameraButton should be visible")
            waitForRendering(cameraBtn)
            mouseClick(cameraBtn)

            tryVerify(function() { return drawer.opened }, 2000, "Drawer should be open after camera click")

            let tabBar = findChild(rootId, "renderingTabBar")
            verify(tabBar !== null, "renderingTabBar not found")
            compare(tabBar.currentIndex, 0, "Tab should be on View (index 0) after camera click")

            drawer.close()
            tryVerify(function() { return drawer.position === 0 }, 1000)
        }

        function test_drawerOpensOnLayersClick() {
            rootId.width = 400
            waitForRendering(rootId)
            navigateToView()

            let viewPage = ObjectFinder.findObjectByChain(rootId, viewPageChain)
            verify(viewPage !== null, "viewPage not found")
            let drawer = viewPage.viewDrawer
            verify(drawer !== null, "viewDrawer not found")
            tryVerify(function() { return drawer.position === 0 }, 1000, "Drawer should start fully closed")

            let layersBtn = findChild(rootId, "layersButton")
            verify(layersBtn !== null, "layersButton not found")
            tryVerify(function() { return layersBtn.visible && layersBtn.width > 0 }, 1000, "layersButton should be visible")
            waitForRendering(layersBtn)
            mouseClick(layersBtn)

            tryVerify(function() { return drawer.opened }, 2000, "Drawer should be open after layers click")

            let tabBar = findChild(rootId, "renderingTabBar")
            verify(tabBar !== null, "renderingTabBar not found")
            compare(tabBar.currentIndex, 1, "Tab should be on Layers (index 1) after layers click")

            drawer.close()
            tryVerify(function() { return drawer.position === 0 }, 1000)
        }

        function test_drawerClosesOnWiden() {
            rootId.width = 400
            waitForRendering(rootId)
            navigateToView()

            let cameraBtn = findChild(rootId, "cameraButton")
            verify(cameraBtn !== null, "cameraButton not found")
            mouseClick(cameraBtn)

            let viewPage = ObjectFinder.findObjectByChain(rootId, viewPageChain)
            verify(viewPage !== null, "viewPage not found")
            let drawer = viewPage.viewDrawer
            verify(drawer !== null, "viewDrawer not found")
            tryVerify(function() { return drawer.opened }, 1000, "Drawer should be open")

            rootId.width = 1024
            waitForRendering(rootId)
            tryVerify(function() { return drawer.position === 0 }, 2000, "Drawer should fully close when widened")
        }

        function test_propertyAliasesPreserved() {
            let viewPage = findViewPage()
            verify(viewPage.renderer !== null, "renderer alias should not be null")
            verify(viewPage.scene !== undefined, "scene alias should exist")
            verify(viewPage.turnTableInteraction !== null, "turnTableInteraction alias should not be null")
            verify(viewPage.leadView !== null, "leadView alias should not be null")
        }
    }

    TestCase {
        name: "CompassResponsive"
        when: windowShown

        function init() {
            rootId.width = 1024
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function cleanup() {
            rootId.width = 1200
            waitForRendering(rootId)
        }

        function test_compassMaximumSize() {
            rootId.width = 1200
            waitForRendering(rootId)
            let compass = findChild(rootId, "compass")
            verify(compass !== null, "compass not found")
            verify(compass.width <= 175, "Compass should not exceed 175px, got " + compass.width)
        }

        function test_compassMinimumSize() {
            rootId.width = 400
            waitForRendering(rootId)
            let compass = findChild(rootId, "compass")
            verify(compass !== null, "compass not found")
            verify(compass.width >= 80, "Compass should be at least 80px, got " + compass.width)
        }
    }
}
