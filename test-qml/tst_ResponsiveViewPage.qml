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
