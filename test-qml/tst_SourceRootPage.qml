import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib

// The page of a source root (SURVEY_TREE_PLAN C7.4b): the source card names
// the copied file, its format and provenance, and the solve status; Reload and
// Replace… live in the header's right-click menus (MP.9); the derived items the
// user hid are listed with Restore….
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "SourceRootPage"
        when: windowShown

        SignalSpy {
            id: attachCompletedSpyId
            target: RootData.externalCenterlineManager
            signalName: "attachCompleted"
        }

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
            attachCompletedSpyId.clear()
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function gotoNodePage(address, node) {
            RootData.pageSelectionModel.currentPageAddress = address
            tryVerify(() => {
                          const page = RootData.pageView.currentPageItem
                          return page !== null && page.objectName === "cavePage"
                              && page.currentNode === node
                      }, 10000, "the page for " + address + " should be showing")
            const page = RootData.pageView.currentPageItem
            waitForRendering(page)
            return page
        }

        function sourceCard(page) {
            const card = findChild(page, "sourceRootCard")
            verify(card !== null, "the root's page carries the source card")
            tryVerify(() => card.visible, 5000, "the source card shows on the root's page")
            return card
        }

        function test_cardShowsTheSource() {
            const sourceRoot = attachCompassProject("source-root-card")
            const page = gotoNodePage("Source/Data/Node=PROJECT", sourceRoot)
            const card = sourceCard(page)

            const header = findChild(card, "attachedHeader")
            compare(header.fileName, "PROJECT.MAK", "the card names the copied file")
            compare(findChild(card, "attachedFormatLabel").text, sourceRoot.externalCenterline.format)
            verify(sourceRoot.externalCenterline.format.length > 0, "the card names the format")
            tryVerify(() => findChild(card, "sourceModeLabel").text.indexOf("compass_project/PROJECT.MAK") >= 0,
                      5000, "the card says where the copy came from")

            compare(findChild(card, "sourceRootUnitsLabel"), null,
                    "MP.9: the card carries no unit count")

            const solveStatusLabel = findChild(card, "solveStatusLabel")
            tryVerify(() => /^[1-9]\d* stations$/.test(solveStatusLabel.text), 20000,
                      "the solve status counts the root's solved stations; got: "
                      + solveStatusLabel.text)

            verify(!findChild(card, "sourceRootHiddenLabel").visible, "nothing is hidden yet")
            compare(findChild(card, "replaceButton"), null, "Replace… is no button on the card")
            const menuArea = openAttachedFileMenu(header)
            const replaceItem = findChild(menuArea, "replaceAction")
            verify(replaceItem !== null, "Replace… lives in the root's header menu")
            verify(replaceItem.enabled, "an idle root offers Replace…")
            closeMenu(menuArea)
            verify(findChild(page, "fixStationsLink").visible, "the root keeps its fix-station row")
            verify(!findChild(page, "derivedNodeCard").visible, "a root shows no derived-node card")
        }

        // The cavern output covers the whole project, so its link sits on the
        // source root's card beside the solve status, and nowhere on a trip.
        function test_cardLinksToTheCavernOutput() {
            const sourceRoot = attachCompassProject("source-root-cavern-link")
            const page = gotoNodePage("Source/Data/Node=PROJECT", sourceRoot)
            const card = sourceCard(page)

            const link = findChild(card, "viewCavernOutputLink")
            verify(link !== null, "the source root card carries View cavern output")
            compare(link.text, "View cavern output")
            tryVerify(() => link.visible, 5000, "the link shows on the card")
            waitForRendering(card)
            mouseClick(link)
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", "Cavern")
        }

        function test_reloadFromTheHeaderMenuReloadsTheRoot() {
            const sourceRoot = attachCompassProject("source-root-reload")
            attachCompletedSpyId.clear()
            const page = gotoNodePage("Source/Data/Node=PROJECT", sourceRoot)
            const card = sourceCard(page)

            compare(findChild(card, "sourceRootReloadButton"), null,
                    "MP.9: Reload is no button on the card")
            const menuArea = openAttachedFileMenu(findChild(card, "attachedHeader"))
            const reloadItem = findChild(menuArea, "reloadFromSourceAction")
            verify(reloadItem !== null, "Reload lives in the root's header menu")
            tryVerify(() => reloadItem.enabled, 5000,
                      "the file the root was copied from is on disk, so Reload is offered")
            closeMenu(menuArea)
            reloadItem.triggered()

            tryVerify(() => attachCompletedSpyId.count === 1, 20000, "Reload runs once")
            const report = attachCompletedSpyId.signalArguments[0][0]
            verify(report.success, "the reload succeeds; got: " + report.errorMessage)
            compare(report.ownerId.toString(), sourceRoot.id.toString(), "the root is what reloads")
            compare(sourceRoot.childNodeCount, 3, "the subtree is rebuilt as it was")
        }

        function test_hideThenRestore() {
            const sourceRoot = attachCompassProject("source-root-restore")
            attachCompletedSpyId.clear()
            const alpha = childNamed(sourceRoot, "Alpha")
            verify(alpha !== null)
            const alpha2 = alpha.trip(0).name === "ALPHA2" ? alpha.trip(0) : alpha.trip(1)
            compare(alpha2.name, "ALPHA2")

            verify(RootData.hideDerived(alpha2), "a derived trip hides")
            verify(RootData.hideDerived(childNamed(sourceRoot, "Bravo")), "a derived node hides")
            compare(alpha.tripCount, 1, "the hidden trip leaves the tree")
            verify(childNamed(sourceRoot, "Bravo") === null, "the hidden node leaves the tree")

            const page = gotoNodePage("Source/Data/Node=PROJECT", sourceRoot)
            const card = sourceCard(page)

            const hiddenLabel = findChild(card, "sourceRootHiddenLabel")
            tryVerify(() => hiddenLabel.visible, 5000, "the card lists what is hidden")
            compare(findChild(card, "sourceRootHiddenLabel").text, "2 hidden: ALPHA2, BRAVO.DAT ·")

            waitForRendering(card)
            mouseClick(findChild(card, "sourceRootRestoreLink"))
            const dialog = findChild(card, "sourceRootRestoreDialog")
            tryVerify(() => dialog.opened, 5000, "Restore… asks first")
            compare(findChild(dialog, "sourceRootRestoreMessage").text,
                    "Restore 2 hidden item(s) from PROJECT.MAK? They come back as the file describes them.")
            waitForRendering(dialog.contentItem)
            mouseClick(findChild(dialog, "sourceRootRestoreConfirmButton"))

            tryVerify(() => attachCompletedSpyId.count === 1, 20000, "Restore reconciles the subtree")
            tryVerify(() => childNamed(sourceRoot, "Bravo") !== null, 10000, "the hidden node comes back")
            tryCompare(alpha, "tripCount", 2, 10000, "the hidden trip comes back")
            tryVerify(() => !hiddenLabel.visible, 5000, "nothing is hidden any more")
        }
    }
}
