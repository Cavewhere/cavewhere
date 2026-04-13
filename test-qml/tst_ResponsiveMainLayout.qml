import QtQuick
import QtTest
import QtQuick.Controls as QC
import QmlTestRecorder
import cavewherelib

MainWindowTest {
    id: rootId

    Component.onCompleted: {
        rootId.mainWindow.objectName = "mainWindow"
        // Page addresses are set in MainContent.onCompleted via page registration
    }

    TestCase {
        name: "MainContentLayoutSize"
        when: windowShown

        function test_wideLayoutAtFullWidth() {
            rootId.width = 1024
            waitForRendering(rootId)
            compare(rootId.mainWindow.layoutSize, Theme.LayoutSize.Wide)
        }

        function test_mediumLayoutAtMediumWidth() {
            rootId.width = 600
            waitForRendering(rootId)
            compare(rootId.mainWindow.layoutSize, Theme.LayoutSize.Medium)
        }

        function test_narrowLayoutAtSmallWidth() {
            rootId.width = 400
            waitForRendering(rootId)
            compare(rootId.mainWindow.layoutSize, Theme.LayoutSize.Narrow)
        }

        function test_sidebarVisibleAtWide() {
            rootId.width = 1024
            waitForRendering(rootId)
            let sidebar = ObjectFinder.findObjectByChain(rootId, "rootId->mainWindow->mainSideBar")
            verify(sidebar !== null, "mainSideBar not found")
            verify(sidebar.visible, "Sidebar should be visible at wide width")
        }

        function test_sidebarVisibleAtMedium() {
            rootId.width = 600
            waitForRendering(rootId)
            let sidebar = ObjectFinder.findObjectByChain(rootId, "rootId->mainWindow->mainSideBar")
            verify(sidebar !== null, "mainSideBar not found")
            verify(sidebar.visible, "Sidebar should be visible at medium width")
        }

        function test_sidebarHiddenAtNarrow() {
            rootId.width = 400
            waitForRendering(rootId)
            let sidebar = ObjectFinder.findObjectByChain(rootId, "rootId->mainWindow->mainSideBar")
            verify(sidebar !== null, "mainSideBar not found")
            verify(!sidebar.visible, "Sidebar should be hidden at narrow width")
        }

        function cleanup() {
            rootId.width = 1200
            waitForRendering(rootId)
        }
    }

    TestCase {
        name: "LinkBarResponsive"
        when: windowShown

        function test_syncButtonAlwaysVisibleAtWide() {
            rootId.width = 1024
            waitForRendering(rootId)
            let syncBtn = ObjectFinder.findObjectByChain(rootId, "rootId->mainWindow->linkBar->syncButton")
            verify(syncBtn !== null, "syncButton not found")
            verify(syncBtn.visible, "SyncButton should be visible at wide width")
        }

        function test_syncButtonAlwaysVisibleAtNarrow() {
            rootId.width = 400
            waitForRendering(rootId)
            let syncBtn = ObjectFinder.findObjectByChain(rootId, "rootId->mainWindow->linkBar->syncButton")
            verify(syncBtn !== null, "syncButton not found")
            verify(syncBtn.visible, "SyncButton should be visible at narrow width")
        }

        function test_hamburgerHiddenAtWide() {
            rootId.width = 1024
            waitForRendering(rootId)
            let hamburger = ObjectFinder.findObjectByChain(rootId, "rootId->mainWindow->linkBar->hamburgerButton")
            verify(hamburger !== null, "hamburgerButton not found")
            verify(!hamburger.visible, "Hamburger should be hidden at wide width")
        }

        function test_hamburgerVisibleAtNarrow() {
            rootId.width = 400
            waitForRendering(rootId)
            let hamburger = ObjectFinder.findObjectByChain(rootId, "rootId->mainWindow->linkBar->hamburgerButton")
            verify(hamburger !== null, "hamburgerButton not found")
            verify(hamburger.visible, "Hamburger should be visible at narrow width")
        }

        function cleanup() {
            rootId.width = 1200
            waitForRendering(rootId)
        }
    }
}
