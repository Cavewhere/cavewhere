import QtQuick
import QtTest
import cavewherelib
import QQuickGit as QG

Item {
    id: rootId
    width: 800
    height: 600

    QG.GitRepository {
        id: testRepo
    }

    Component {
        id: comparePageComponent
        GitImageComparePage {
            anchors.fill: parent
        }
    }

    property var currentPage: null

    function createPage(props) {
        if (currentPage) {
            currentPage.destroy()
            currentPage = null
        }

        let defaults = {
            repository: testRepo,
            commitSha: "0000000000000000000000000000000000000000",
            parentIndex: 0,
            filePath: "test.png",
            statusText: "Modified"
        }

        let merged = Object.assign(defaults, props || {})
        currentPage = comparePageComponent.createObject(rootId, merged)
        return currentPage
    }

    TestCase {
        name: "GitImageComparePage"
        when: windowShown

        function cleanup() {
            if (rootId.currentPage) {
                rootId.currentPage.destroy()
                rootId.currentPage = null
            }
        }

        function test_requiredPropertiesBound() {
            let page = rootId.createPage({
                filePath: "images/cave.png",
                statusText: "Modified",
                commitSha: "abc123abc123abc123abc123abc123abc123abc1"
            })
            compare(page.filePath, "images/cave.png")
            compare(page.statusText, "Modified")
            compare(page.commitSha, "abc123abc123abc123abc123abc123abc123abc1")
            compare(page.parentIndex, 0)
            compare(page.workingTree, false)
        }

        function test_dividerRatioDefaultsToCenter() {
            let page = rootId.createPage()
            compare(page._dividerRatio, 0.5)
        }

        function test_modifiedState_legendVisibility() {
            let page = rootId.createPage({ statusText: "Modified" })
            waitForRendering(page)

            let beforeLabel = findChild(page, "beforeLabel")
            let afterLabel = findChild(page, "afterLabel")
            verify(beforeLabel.visible, "Before label should be visible for Modified")
            verify(afterLabel.visible, "After label should be visible for Modified")
        }

        function test_modifiedState_dividerVisible() {
            let page = rootId.createPage({ statusText: "Modified" })
            waitForRendering(page)

            let divider = findChild(page, "dividerHandle")
            verify(divider !== null, "Divider handle should exist")
            verify(divider.visible, "Divider should be visible for Modified")
        }

        function test_modifiedState_afterClipHidden() {
            let page = rootId.createPage({ statusText: "Modified" })
            waitForRendering(page)

            let afterClip = findChild(page, "afterClip")
            verify(!afterClip.visible, "After clip placeholder should be hidden for Modified")
        }

        function test_modifiedState_isAddedFalse() {
            let page = rootId.createPage({ statusText: "Modified" })
            verify(!page._isAdded, "_isAdded should be false for Modified")
            verify(!page._isDeleted, "_isDeleted should be false for Modified")
        }

        function test_addedState_isAddedTrue() {
            let page = rootId.createPage({ statusText: "Added" })
            verify(page._isAdded, "_isAdded should be true for Added")
            verify(!page._isDeleted, "_isDeleted should be false for Added")
        }

        function test_addedState_legendVisibility() {
            let page = rootId.createPage({ statusText: "Added" })
            waitForRendering(page)

            let beforeLabel = findChild(page, "beforeLabel")
            let afterLabel = findChild(page, "afterLabel")
            verify(!beforeLabel.visible, "Before label should be hidden for Added")
            verify(afterLabel.visible, "After label should be visible for Added")
        }

        function test_addedState_dividerVisible() {
            let page = rootId.createPage({ statusText: "Added" })
            waitForRendering(page)

            let divider = findChild(page, "dividerHandle")
            verify(divider.visible, "Divider should be visible for Added")
        }

        function test_addedState_beforeSourceEmpty() {
            let page = rootId.createPage({ statusText: "Added" })
            compare(page._beforeSource, "", "Before source should be empty for Added")
        }

        function test_addedState_afterSourceNotEmpty() {
            let page = rootId.createPage({ statusText: "Added" })
            verify(page._afterSource !== "", "After source should not be empty for Added")
        }

        function test_addedState_afterClipHidden() {
            let page = rootId.createPage({ statusText: "Added" })
            waitForRendering(page)

            let afterClip = findChild(page, "afterClip")
            verify(!afterClip.visible, "After clip should be hidden for Added")
        }

        function test_deletedState_isDeletedTrue() {
            let page = rootId.createPage({ statusText: "Deleted" })
            verify(!page._isAdded, "_isAdded should be false for Deleted")
            verify(page._isDeleted, "_isDeleted should be true for Deleted")
        }

        function test_deletedState_legendVisibility() {
            let page = rootId.createPage({ statusText: "Deleted" })
            waitForRendering(page)

            let beforeLabel = findChild(page, "beforeLabel")
            let afterLabel = findChild(page, "afterLabel")
            verify(beforeLabel.visible, "Before label should be visible for Deleted")
            verify(!afterLabel.visible, "After label should be hidden for Deleted")
        }

        function test_deletedState_dividerVisible() {
            let page = rootId.createPage({ statusText: "Deleted" })
            waitForRendering(page)

            let divider = findChild(page, "dividerHandle")
            verify(divider.visible, "Divider should be visible for Deleted")
        }

        function test_deletedState_afterSourceEmpty() {
            let page = rootId.createPage({ statusText: "Deleted" })
            compare(page._afterSource, "", "After source should be empty for Deleted")
        }

        function test_deletedState_afterClipVisible() {
            let page = rootId.createPage({ statusText: "Deleted" })
            waitForRendering(page)

            let afterClip = findChild(page, "afterClip")
            verify(afterClip.visible, "After clip should be visible for Deleted")
        }

        function test_dividerRatioUpdatesOnDrag() {
            let page = rootId.createPage({ statusText: "Modified" })
            compare(page._dividerRatio, 0.5)

            page._dividerRatio = 0.3
            compare(page._dividerRatio, 0.3)

            page._dividerRatio = 0.8
            compare(page._dividerRatio, 0.8)
        }
    }
}
