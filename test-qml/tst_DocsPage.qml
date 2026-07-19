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
            docsPageId.slug = ""
            waitForRendering(rootId)
        }

        // The render path: with an empty slug the page renders the manual index
        // and Qt's MarkdownText renderer produces a non-empty laid-out block.
        function test_rendersLandingByDefault() {
            let body = findBody()
            verify(body !== null, "docs body label not found")
            compare(docsPageId.slug, "", "default slug should be the landing page")
            verify(body.text.indexOf("CaveWhere User Manual") >= 0, "landing did not render the manual index")
            verify(body.textFormat === Text.MarkdownText, "body is not rendering MarkdownText")
            tryVerify(function() { return body.implicitHeight > 0 }, 2000, "body did not lay out")
        }

        // slug is the seam that selects a per-article body through cwManualIndex.
        function test_slugDrivesTheBody() {
            let body = findBody()
            docsPageId.slug = "scraps-carpeting"
            tryVerify(function() { return body.text.indexOf("Scraps and Carpeting") >= 0 },
                      2000, "slug did not drive the article body")
            verify(body.text.indexOf("qrc:/manual/images/") >= 0, "image links were not rewritten to qrc:")
            verify(body.text.indexOf("../images/") < 0, "a relative image link survived rewriting")
            verify(body.implicitHeight > 0)
        }

        // Each manual article is registered as a child page of Docs, reachable by
        // its title through the page model — proving the MainContent loop wired
        // cwManualIndex.articles into real, navigable pages.
        function test_manualArticlesRegistered() {
            let model = RootData.pageSelectionModel
            let articles = RootData.manualIndex.articles
            verify(articles.length > 10, "manual index is empty — is the manual resource built?")

            model.gotoPageByName(null, "Docs")
            let docsPage = model.currentPage
            verify(docsPage !== null, "Docs landing page is not registered")

            let article = articles[0]
            model.gotoPageByName(docsPage, article.title)
            tryVerify(function() { return model.currentPageAddress.endsWith(article.title) },
                      2000, "manual article '" + article.title + "' is not a reachable child page")
        }

        // A relative .md link in the body navigates in-app through the page
        // model instead of leaving to the browser, and the hop is an ordinary
        // history navigation so back() returns to where it came from. Starting
        // from a known non-target page (the Docs landing) keeps the forward
        // assertion from passing vacuously if the manual is ever reordered.
        // Emitting linkActivated drives the same handler a real click would.
        function test_relativeLinkNavigatesInApp() {
            let model = RootData.pageSelectionModel
            model.gotoPageByName(null, "Docs")
            verify(model.currentPageAddress.endsWith("Docs"),
                   "pre-state should be the Docs landing, not the link target")

            let body = findBody()
            docsPageId.slug = "settings-change-settings"

            body.linkActivated("../scraps/carpeting.md")
            tryVerify(function() {
                return model.currentPageAddress.endsWith("Scraps and Carpeting")
            }, 2000, "a relative .md link did not navigate to the target article page")

            model.back()
            verify(model.currentPageAddress.endsWith("Docs"),
                   "the in-app hop did not enter history — back() should return to the landing")
        }

        // A same-page anchor is a no-op for now (intra-page scroll is a
        // fast-follow) — it must not navigate away or reach the browser.
        function test_anchorLinkDoesNotNavigate() {
            RootData.pageSelectionModel.gotoPageByName(null, "Docs")
            let body = findBody()
            body.linkActivated("#some-section")
            verify(RootData.pageSelectionModel.currentPageAddress.endsWith("Docs"),
                   "a same-page anchor should not change the current page")
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
