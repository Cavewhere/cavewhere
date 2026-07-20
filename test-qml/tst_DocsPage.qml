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

        function findByName(name) {
            return ObjectFinder.findObjectByChain(rootId, "rootId->docsPage->" + name)
        }

        // The reading column is a Flickable; its declared children are reparented
        // into its contentItem, so walk up from the body to the ancestor that has
        // a contentHeight.
        function findReadingFlickable() {
            let flick = findBody().parent
            while (flick !== null && flick.contentHeight === undefined) {
                flick = flick.parent
            }
            return flick
        }

        function init() {
            // Every test starts wide (persistent sidebar) unless it resizes.
            rootId.width = 1200
            docsPageId.slug = ""
            docsPageId.closeFind()
            let field = findByName("docsSearchField")
            if (field !== null) {
                field.text = ""
            }
            let findField = findByName("docsFindField")
            if (findField !== null) {
                findField.text = ""
            }
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

        // A same-page anchor scrolls within the page (see
        // test_anchorLinkScrollsToHeading); it must never change the page or reach
        // the browser — including an unmatched anchor, which here is a pure no-op.
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

        // The sidebar TOC lists every manual article when the search box is
        // empty, so the chapter-grouped table of contents is always present.
        function test_sidebarListsAllArticlesByDefault() {
            let toc = findByName("docsTocList")
            verify(toc !== null, "sidebar TOC list not found")
            let articleCount = RootData.manualIndex.articles.length
            verify(articleCount > 10, "manual index is empty — is the manual resource built?")
            tryCompare(toc, "count", articleCount, 2000,
                       "the empty-query TOC should list every article")
        }

        // Typing in the search box filters the TOC to matching articles through
        // the ManualSearchModel; clearing it restores the full list.
        function test_searchFiltersTheToc() {
            let toc = findByName("docsTocList")
            let field = findByName("docsSearchField")
            verify(toc !== null && field !== null, "sidebar search widgets not found")
            let articleCount = RootData.manualIndex.articles.length

            field.text = "declination"
            tryVerify(function() { return toc.count > 0 && toc.count < articleCount },
                      2000, "search did not narrow the TOC")

            field.text = ""
            tryCompare(toc, "count", articleCount, 2000,
                       "clearing the search did not restore the full TOC")
        }

        // Below the collapse breakpoint the pinned sidebar gives way to a
        // drawer: its persistent TOC panel deactivates and a toggle button
        // appears. The drawer reuses the same DocsSidebar, so its search/TOC
        // behaviour is already covered by the wide-mode cases above.
        function test_compactModeCollapsesSidebarIntoADrawer() {
            verify(!docsPageId.isNarrow, "a 1200px window should not be compact")
            verify(findByName("docsTocList") !== null, "wide mode should pin the TOC panel")
            let toggle = findByName("docsSidebarToggle")
            verify(toggle !== null, "the drawer toggle should exist")
            verify(!toggle.visible, "the toggle should be hidden while the panel is pinned")

            rootId.width = Theme.breakpointPanelCollapse - 100
            tryVerify(function() { return docsPageId.isNarrow },
                      2000, "shrinking past the breakpoint should switch to compact mode")
            tryVerify(function() { return toggle.visible },
                      2000, "the drawer toggle should appear in compact mode")
            // The pinned panel's Loader deactivates, so its TOC is gone from the
            // page tree; the live TOC now lives inside the (closed) drawer.
            verify(findByName("docsTocList") === null,
                   "the persistent TOC panel should be inactive in compact mode")

            // Picking an entry in compact mode goes through selectSlug, which
            // closes the drawer (a no-op here) and navigates — exercise it.
            let model = RootData.pageSelectionModel
            model.gotoPageByName(null, "Docs")
            docsPageId.selectSlug("scraps-carpeting")
            tryVerify(function() {
                return model.currentPageAddress.endsWith("Scraps and Carpeting")
            }, 2000, "selecting an article in compact mode did not navigate")

            rootId.width = 1200
            tryVerify(function() { return !docsPageId.isNarrow && findByName("docsTocList") !== null },
                      2000, "restoring the width should re-pin the sidebar panel")
        }

        // A gibberish query matches nothing, so the TOC empties out.
        function test_searchWithNoMatchesEmptiesToc() {
            let toc = findByName("docsTocList")
            let field = findByName("docsSearchField")
            verify(toc !== null && field !== null, "sidebar search widgets not found")
            field.text = "zzzznotarealtopic"
            tryCompare(toc, "count", 0, 2000, "a non-matching search should empty the TOC")
        }

        // Selecting an entry navigates in-app by slug and enters history, the
        // same path the sidebar delegate's TapHandler drives.
        function test_navigateToSlugEntersHistory() {
            let model = RootData.pageSelectionModel
            model.gotoPageByName(null, "Docs")
            verify(model.currentPageAddress.endsWith("Docs"), "pre-state should be the Docs landing")

            docsPageId.navigateToSlug("scraps-carpeting")
            tryVerify(function() {
                return model.currentPageAddress.endsWith("Scraps and Carpeting")
            }, 2000, "navigateToSlug did not open the target article")

            model.back()
            verify(model.currentPageAddress.endsWith("Docs"),
                   "the slug hop did not enter history — back() should return to the landing")
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

        // The find bar is hidden until opened; openFind() reveals it and focuses
        // the query field, and closeFind() hides it again.
        function test_findBarOpensAndCloses() {
            verify(!docsPageId.findVisible, "the find bar should start closed")
            let findField = findByName("docsFindField")
            verify(findField !== null, "the find field should exist even while closed")
            verify(!findField.visible, "the find field should be hidden while the bar is closed")

            docsPageId.openFind()
            tryVerify(function() { return findField.visible }, 2000,
                      "openFind should reveal the find bar")
            verify(findField.activeFocus, "openFind should focus the query field")

            docsPageId.closeFind()
            tryVerify(function() { return !findField.visible }, 2000,
                      "closeFind should hide the find bar")
        }

        // A search button opens the find bar, and hides itself while the bar is
        // showing (the bar is the affordance then).
        function test_findButtonOpensFind() {
            let button = findByName("docsFindButton")
            verify(button !== null, "the find button should exist")
            verify(button.visible, "the find button should show while find is closed")

            button.clicked()
            tryVerify(function() { return docsPageId.findVisible }, 2000,
                      "clicking the find button should open the find bar")
            verify(!button.visible, "the find button should hide while the find bar is open")

            docsPageId.closeFind()
            tryVerify(function() { return button.visible }, 2000,
                      "closing find should restore the find button")
        }

        // Wide mode always reserves a top band so the find UI never covers text:
        // the search button sits in it while find is closed, the bar fills it while
        // open, and opening find does not shift the document (the band is already
        // reserved). Both affordances stay above the document top.
        function test_findBarReservesSpaceAboveDocument() {
            let flick = findReadingFlickable()
            verify(flick !== null, "the reading column should be a Flickable")
            verify(flick.y > 0, "wide mode should reserve a top band above the document")
            let documentTop = flick.y

            let button = findByName("docsFindButton")
            verify(button.y >= 0 && button.y + button.height <= documentTop + 1,
                   "the closed-state search button should sit within the reserved band")

            docsPageId.openFind()
            let findBar = findByName("docsFindField").parent   // field -> bar
            verify(findBar.y + findBar.height <= documentTop + 1,
                   "the find bar should sit entirely above the document")
            tryCompare(flick, "y", documentTop, 2000,
                       "opening find should not shift the document — the band is already reserved")

            docsPageId.closeFind()
        }

        // In compact mode the find bar shares the drawer toggle's row (starting
        // right of the hamburger) instead of dropping to its own band, so opening
        // find does not push the document further down.
        function test_compactFindBarSharesHamburgerRow() {
            rootId.width = Theme.breakpointPanelCollapse - 100
            tryVerify(function() { return docsPageId.isNarrow }, 2000,
                      "shrinking past the breakpoint should switch to compact mode")

            let toggle = findByName("docsSidebarToggle")
            let flick = findReadingFlickable()
            verify(flick !== null, "the reading column should be a Flickable")
            let documentTopClosed = flick.y

            docsPageId.openFind()
            let bar = findByName("docsFindField").parent

            verify(Math.abs(bar.y - toggle.y) <= toggle.height,
                   "the find bar should sit on the hamburger's row, not below it")
            verify(bar.x >= toggle.x + toggle.width,
                   "the find bar should start to the right of the hamburger")
            tryCompare(flick, "y", documentTopClosed, 2000,
                       "opening find in compact mode should not push the document down")

            docsPageId.closeFind()
            rootId.width = 1200
            tryVerify(function() { return !docsPageId.isNarrow }, 2000)
        }

        // Typing a term that occurs several times highlights the first match and
        // the status reads "1/N"; find is case-insensitive over the rendered text.
        function test_findHighlightsAndCountsMatches() {
            let body = findBody()
            docsPageId.slug = "scraps-carpeting"
            tryVerify(function() { return body.text.indexOf("Scraps and Carpeting") >= 0 },
                      2000, "the article body did not load")

            docsPageId.openFind()
            let findField = findByName("docsFindField")
            findField.text = "scrap"

            let status = findByName("docsFindStatus")
            verify(status !== null, "the match-count status label should exist")
            tryVerify(function() { return status.text.indexOf("/") >= 0 }, 2000,
                      "a common term should report a running match count")
            verify(status.text.startsWith("1/"), "the first match should be current: " + status.text)

            verify(body.selectionStart >= 0, "the first match should be selected")
            compare(body.selectedText.toLowerCase(), "scrap",
                    "the highlighted selection should be the matched term")
        }

        // Next advances the selection to a later match (wrapping at the end), and
        // Previous walks it back — driven through the bar's buttons.
        function test_findNextAndPreviousWalkMatches() {
            let body = findBody()
            docsPageId.slug = "scraps-carpeting"
            tryVerify(function() { return body.text.indexOf("Scraps and Carpeting") >= 0 }, 2000)

            docsPageId.openFind()
            findByName("docsFindField").text = "scrap"
            tryVerify(function() { return body.selectionStart >= 0 }, 2000,
                      "the first match should be selected")

            let firstStart = body.selectionStart
            findByName("docsFindNext").clicked()
            tryVerify(function() { return body.selectionStart !== firstStart }, 2000,
                      "Next should move the selection to a different match")
            let secondStart = body.selectionStart
            verify(secondStart > firstStart, "Next should move forward through the document")

            findByName("docsFindPrevious").clicked()
            tryVerify(function() { return body.selectionStart === firstStart }, 2000,
                      "Previous should return to the earlier match")
        }

        // In the query field, Enter steps to the next match and Shift+Enter steps
        // back — the in-field counterpart to the FindNext/FindPrevious shortcuts.
        function test_findFieldEnterKeysWalkMatches() {
            let body = findBody()
            docsPageId.slug = "scraps-carpeting"
            tryVerify(function() { return body.text.indexOf("Scraps and Carpeting") >= 0 }, 2000)

            docsPageId.openFind()
            let field = findByName("docsFindField")
            field.forceActiveFocus()
            field.text = "scrap"
            tryVerify(function() { return body.selectionStart >= 0 }, 2000,
                      "the first match should be selected")
            let first = body.selectionStart

            keyClick(Qt.Key_Return)
            tryVerify(function() { return body.selectionStart > first }, 2000,
                      "Enter should advance to the next match")

            keyClick(Qt.Key_Return, Qt.ShiftModifier)
            tryVerify(function() { return body.selectionStart === first }, 2000,
                      "Shift+Enter should step back to the previous match")
        }

        // A term with no occurrences reports "None", leaves nothing selected.
        function test_findWithNoMatchesReportsNone() {
            let body = findBody()
            docsPageId.slug = "scraps-carpeting"
            tryVerify(function() { return body.text.indexOf("Scraps and Carpeting") >= 0 }, 2000)

            docsPageId.openFind()
            findByName("docsFindField").text = "zzzznotarealword"

            let status = findByName("docsFindStatus")
            tryCompare(status, "text", "None", 2000,
                       "a non-matching term should report None")
            compare(body.selectedText, "", "nothing should be selected when there are no matches")
        }

        // Closing the bar clears the highlighted match.
        function test_closingFindClearsSelection() {
            let body = findBody()
            docsPageId.slug = "scraps-carpeting"
            tryVerify(function() { return body.text.indexOf("Scraps and Carpeting") >= 0 }, 2000)

            docsPageId.openFind()
            findByName("docsFindField").text = "scrap"
            tryVerify(function() { return body.selectedText.length > 0 }, 2000,
                      "a match should be selected")

            docsPageId.closeFind()
            tryVerify(function() { return body.selectedText === "" }, 2000,
                      "closing the find bar should clear the selection")
            verify(!docsPageId.findVisible, "the find bar should be closed")
        }

        // A same-page "#heading" link scrolls the reading column to that specific
        // heading (an earlier heading scrolls less than a later one, proving the
        // anchor resolves to the right position, not merely "down the page"); a
        // bogus anchor neither scrolls nor navigates away.
        function test_anchorLinkScrollsToHeading() {
            let body = findBody()
            let flick = findReadingFlickable()
            verify(flick !== null, "the reading column should be a Flickable")

            docsPageId.slug = "concepts-glossary"
            tryVerify(function() { return body.text.indexOf("Glossary") >= 0 }, 2000,
                      "the glossary article did not load")
            tryVerify(function() { return flick.contentHeight > flick.height }, 2000,
                      "the glossary should overflow the viewport so anchors can scroll")

            flick.contentY = 0
            body.linkActivated("#the-data-model")
            let earlyY = flick.contentY

            flick.contentY = 0
            body.linkActivated("#scrap")
            tryVerify(function() { return flick.contentY > earlyY }, 2000,
                      "a later heading should scroll further than an earlier one")
            let lateY = flick.contentY

            let model = RootData.pageSelectionModel
            let addressBefore = model.currentPageAddress
            body.linkActivated("#not-a-real-heading")
            compare(flick.contentY, lateY, "an unknown anchor should not scroll")
            compare(model.currentPageAddress, addressBefore,
                    "an unknown anchor should not navigate away")
        }

        // Navigating to another article resets the find bar so its matches don't
        // linger against the new page's text.
        function test_navigatingResetsFind() {
            let body = findBody()
            docsPageId.slug = "scraps-carpeting"
            tryVerify(function() { return body.text.indexOf("Scraps and Carpeting") >= 0 }, 2000)

            docsPageId.openFind()
            findByName("docsFindField").text = "scrap"
            tryVerify(function() { return docsPageId.findVisible }, 2000)

            docsPageId.slug = "notes-add-a-note"
            verify(!docsPageId.findVisible,
                   "changing the article should close the find bar")
        }
    }
}
