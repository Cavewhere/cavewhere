import QtQuick
import QtTest
import cavewherelib
import QQuickGit as QG

Item {
    id: rootId
    width: 800
    height: 600

    // A GitRepository pointed at a non-existent path so GitFilePatch stays empty
    QG.GitRepository {
        id: testRepo
    }

    // Component under test — instantiated with required properties via createObject
    Component {
        id: diffPageComponent
        GitFileDiffPage {
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
            filePath: "test.txt",
            statusText: "Modified"
        }

        let merged = Object.assign(defaults, props || {})
        currentPage = diffPageComponent.createObject(rootId, merged)
        return currentPage
    }

    TestCase {
        name: "GitFileDiffPage"
        when: windowShown

        function cleanup() {
            if (rootId.currentPage) {
                rootId.currentPage.destroy()
                rootId.currentPage = null
            }
        }

        function test_requiredPropertiesBound() {
            let page = rootId.createPage({
                filePath: "src/main.cpp",
                statusText: "Added",
                commitSha: "abc123"
            })
            compare(page.filePath, "src/main.cpp")
            compare(page.statusText, "Added")
            compare(page.commitSha, "abc123")
            compare(page.parentIndex, 0)
            compare(page.workingTree, false)
        }

        function test_workingTreePropertyDefaults() {
            let page = rootId.createPage()
            compare(page.workingTree, false)

            let page2 = rootId.createPage({ workingTree: true })
            compare(page2.workingTree, true)
        }

        function test_errorLabelVisible() {
            let page = rootId.createPage({ commitSha: "" })
            waitForRendering(page)

            let errorLabel = findChild(page, "errorLabel")
            let binaryLabel = findChild(page, "binaryLabel")
            let lfsLabel = findChild(page, "lfsLabel")
            let tooLargeLabel = findChild(page, "tooLargeLabel")
            let diffListView = findChild(page, "diffListView")

            // With empty commitSha and non-workingTree, GitFilePatch clears —
            // no error, no data, so the diffListView should be visible but empty
            verify(diffListView !== null)
            verify(!errorLabel.visible)
            verify(!binaryLabel.visible)
            verify(!lfsLabel.visible)
            verify(!tooLargeLabel.visible)
        }

        function test_diffListViewExists() {
            let page = rootId.createPage()
            waitForRendering(page)

            let diffListView = findChild(page, "diffListView")
            verify(diffListView !== null)
        }

        function test_stateLabelsExist() {
            let page = rootId.createPage()
            waitForRendering(page)

            verify(findChild(page, "errorLabel") !== null)
            verify(findChild(page, "binaryLabel") !== null)
            verify(findChild(page, "lfsLabel") !== null)
            verify(findChild(page, "tooLargeLabel") !== null)
            verify(findChild(page, "diffListView") !== null)
        }
    }
}
