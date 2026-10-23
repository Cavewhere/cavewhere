import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// Accepting warnings from the trip page (#174): the warning badge's menu
// accepts or un-accepts every survey warning in the trip, the per-cell popup
// accepts every warning of one kind, and a cell whose warnings are all
// accepted keeps a muted marker so it can be reopened.
//
// Phase 3 names the pieces this test reaches for: the badge
// (SurveyErrorOverview) is surveyErrorOverview with its text in overviewText;
// tripWarningsMenuButton opens a menu with acceptAllWarnings and
// unacceptAllWarnings; ErrorListQuoteBox offers an acceptSimilar link; the
// errorIcon's image is errorIconImage.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "AcceptWarnings"
        when: windowShown

        readonly property int missingLeftId: 1740

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        // A1 to A2. A1's four LRUD boxes are empty; A2's are filled when
        // fillA2 is true, which leaves exactly four warnings.
        function gotoSurveyTable(fillA2) {
            const context = surveyTableId.openSurveyTable(this, "AcceptCave", "AcceptTrip")
            surveyTableId.setStationName(this, context, 0, "A1")
            surveyTableId.setStationName(this, context, 1, "A2")

            if (fillA2) {
                const row = surveyTableId.stationRow(context, 1)
                for (const cellRole of [SurveyEditorCellIndex.StationLeftCell,
                                        SurveyEditorCellIndex.StationRightCell,
                                        SurveyEditorCellIndex.StationUpCell,
                                        SurveyEditorCellIndex.StationDownCell]) {
                    context.editorModel.setDataAt(context.editorModel.cellIndex(row, cellRole), "1")
                }
            }

            tryCompare(context.trip.errorModel, "warningCount", fillA2 ? 4 : 8)
            return context
        }

        function findInEditor(name) {
            let found = null
            tryVerify(() => {
                const editor = surveyTableId.surveyEditor()
                found = editor === null ? null : findChild(editor, name)
                return found !== null
            }, 5000, name + " should be on the trip page")
            return found
        }

        function menuItem(menuButton, itemName) {
            let item = null
            tryVerify(() => {
                item = findChild(menuButton, itemName)
                return item !== null
            }, 5000, "the trip warnings menu should offer " + itemName)
            return item
        }

        function leftErrorIconChain(context) {
            const row = surveyTableId.stationRow(context, 0)
            return "rootId->tripPage->surveyEditor->view->dataBox." + row + "."
                    + SurveyEditorCellIndex.StationLeftCell + "->coreTextInput->errorIcon"
        }

        function leftErrorIcon(context) {
            let icon = null
            tryVerify(() => {
                surveyTableId.rowItem(this, context, surveyTableId.stationRow(context, 0))
                icon = ObjectFinder.findObjectByChain(rootId.mainWindow, leftErrorIconChain(context))
                return icon !== null
            }, 5000, "A1's Left cell should show its error icon")
            return icon
        }

        function test_menuAcceptsAndUnacceptsEveryWarning() {
            const context = gotoSurveyTable(true)

            const menuButton = findInEditor("tripWarningsMenuButton")
            tryVerify(() => menuButton.visible, 5000, "the badge should offer its menu")

            mouseClick(menuButton)
            const acceptAll = menuItem(menuButton, "acceptAllWarnings")
            verify(acceptAll.enabled, "Accept all should be on while warnings show")
            acceptAll.triggered()

            tryCompare(context.trip.errorModel, "warningCount", 0)
            tryCompare(context.trip.errorModel, "suppressedWarningCount", 4)

            const badge = findInEditor("surveyErrorOverview")
            const badgeText = findChild(badge, "overviewText")
            verify(badgeText !== null, "the badge should carry its text")
            tryVerify(() => badge.visible && badgeText.text.indexOf("accepted") >= 0, 5000,
                      "the badge should stay up and say what was accepted: " + badgeText.text)
            verify(menuButton.visible, "the menu should stay reachable with only accepted warnings")

            mouseClick(menuButton)
            const unacceptAll = menuItem(menuButton, "unacceptAllWarnings")
            verify(unacceptAll.enabled, "Un-accept all should be on while warnings are accepted")
            unacceptAll.triggered()

            tryCompare(context.trip.errorModel, "warningCount", 4)
            tryCompare(context.trip.errorModel, "suppressedWarningCount", 0)
        }

        function test_acceptSimilarAcceptsOnlyThatKind() {
            const context = gotoSurveyTable(false)
            compare(context.trip.warningCountForTypeId(missingLeftId), 2)

            mouseClick(leftErrorIcon(context))

            const row = surveyTableId.stationRow(context, 0)
            const quoteBoxName = "errorBoxdataBox." + row + "." + SurveyEditorCellIndex.StationLeftCell
            let acceptSimilar = null
            tryVerify(() => {
                acceptSimilar = ObjectFinder.findObjectByChain(rootId.mainWindow,
                                                               "rootId->" + quoteBoxName + "->acceptSimilar")
                return acceptSimilar !== null && acceptSimilar.visible
            }, 5000, "the Missing Left popup should offer to accept every similar warning")
            verify(acceptSimilar.text.indexOf("(2)") >= 0, "the link should count both Left warnings: " + acceptSimilar.text)

            mouseClick(acceptSimilar)

            tryCompare(context.trip.errorModel, "warningCount", 6)
            compare(context.trip.warningCountForTypeId(missingLeftId), 0)
            compare(context.trip.errorModel.suppressedWarningCount, 2)
        }

        function test_acceptedCellKeepsAMutedMarker() {
            const context = gotoSurveyTable(false)
            context.chunk.setWarningAccepted(0, SurveyChunk.StationLeftRole, missingLeftId, true)
            tryCompare(context.trip.errorModel, "warningCount", 7)

            const icon = leftErrorIcon(context)
            tryVerify(() => icon.visible, 5000, "a cell with only accepted warnings should keep its marker")

            const image = findChild(icon, "errorIconImage")
            verify(image !== null, "the error icon should name its image")
            tryVerify(() => String(image.source).indexOf("warningAccepted") >= 0, 5000,
                      "the marker should use the accepted look: " + image.source)
        }
    }
}
