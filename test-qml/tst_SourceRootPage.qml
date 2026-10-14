import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib

// The page of a source root (SURVEY_TREE_PLAN C7.4b): the source card names
// the copied file, its format and provenance, and the solve status; Reload and
// Replace… live in the header's right-click menus (MP.9).
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
            compare(findChild(card, "solveWarningPill"), null,
                    "XC.4: the card shows no warning count; the row and the warnings banner carry warnings")
            compare(findChild(card, "solveStatusDot"), null,
                    "XC.4: the card shows no colored solve dot")

            compare(findChild(card, "sourceRootHiddenLabel"), null,
                    "XC.11: sourced data is all or nothing, so the card lists nothing hidden")
            compare(findChild(card, "sourceRootRestoreLink"), null,
                    "XC.11: the card offers no Restore…")
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
    }
}
