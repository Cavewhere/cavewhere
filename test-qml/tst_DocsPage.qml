import QtQuick
import QtTest
import cavewherelib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    DocsPage {
        id: docsPageId
        objectName: "docsPage"
        anchors.fill: parent
    }

    TestCase {
        name: "DocsPage"
        when: windowShown

        function findBody() {
            return ObjectFinder.findObjectByChain(rootId, "rootId->docsPage->docsBody")
        }

        function init() {
            waitForRendering(rootId)
        }

        // The render path: the placeholder Markdown reaches the body label and
        // Qt's MarkdownText renderer produces a non-empty laid-out block.
        function test_rendersPlaceholderMarkdown() {
            let body = findBody()
            verify(body !== null, "docs body label not found")
            verify(body.text.length > 0, "markdown text is empty")
            verify(body.textFormat === Text.MarkdownText, "body is not rendering MarkdownText")
            tryVerify(function() { return body.implicitHeight > 0 }, 2000, "body did not lay out")
        }

        // markdownText is the public seam CP2 uses to swap in per-article content.
        function test_markdownTextDrivesTheBody() {
            let body = findBody()
            docsPageId.markdownText = "# Swapped\n\nNew body copy."
            tryCompare(body, "text", "# Swapped\n\nNew body copy.")
            verify(body.implicitHeight > 0)
            docsPageId.markdownText = Qt.binding(function() { return docsPageId.placeholderMarkdown })
        }

        // File -> Docs is a real registered page reachable through the model.
        function test_docsPageRegisteredAndReachable() {
            RootData.pageSelectionModel.gotoPageByName(null, "Docs")
            tryVerify(function() {
                return RootData.pageSelectionModel.currentPageAddress.endsWith("Docs")
            }, 2000, "File->Docs did not navigate to the Docs page")
        }

        // Navigating to Docs pushes it as its own history entry on top of the
        // previous page, so CaveWhere's existing back/forward carries the viewer.
        // Asserting back() returns to the prior page proves Docs was a distinct
        // entry, not just that some earlier navigation left a back-stack.
        function test_docsPageEntersHistory() {
            let model = RootData.pageSelectionModel
            model.gotoPageByName(null, "About")
            model.gotoPageByName(null, "Docs")
            verify(model.currentPageAddress.endsWith("Docs"), "did not navigate to Docs")
            verify(model.hasBackward, "Docs did not enter navigation history")

            model.back()
            verify(model.currentPageAddress.endsWith("About"),
                   "back() did not return to the page before Docs — Docs was not a distinct history entry")
        }
    }
}
