import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Ignoring warnings from the trip page (#174): the warning badge's menu
// ignores or shows every survey warning in the trip, the per-cell popup
// ignores every warning of one kind, and a cell whose warnings are all
// ignored keeps a muted marker so it can be reopened.
//
// Phase 3 names the pieces this test reaches for: the badge
// (SurveyErrorOverview) is surveyErrorOverview with its text in overviewText;
// tripWarningsMenuButton opens a menu with ignoreAllWarnings and
// showAllWarnings; ErrorListQuoteBox offers an ignoreSimilar link; the
// errorIcon's image is errorIconImage. The cell's warnings are a Popup, so they
// are found under the window's content item (rootId.parent), not under rootId.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "IgnoreWarnings"
        when: windowShown

        // cwErrorTypeId::SurveyMissingLeft; the enum is C++-only.
        readonly property int missingLeftId: 4

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        // A1 to A2. A1's four LRUD boxes are empty; A2's are filled when
        // fillA2 is true, which leaves exactly four warnings.
        function gotoSurveyTable(fillA2) {
            const context = surveyTableId.openSurveyTable(this, "IgnoreCave", "IgnoreTrip")
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

        function test_menuIgnoresAndShowsEveryWarning() {
            const context = gotoSurveyTable(true)

            // A complete shot leaves the trip with warnings and no errors
            const shotRow = context.editorModel.toModelRow(
                              context.editorModel.rowIndex(context.chunk, 0, SurveyEditorRowIndex.ShotRow))
            verify(shotRow >= 0, "the chunk should have a shot row")
            for (const [cellRole, value] of [[SurveyEditorCellIndex.ShotDistanceCell, "10"],
                                             [SurveyEditorCellIndex.ShotCompassCell, "10"],
                                             [SurveyEditorCellIndex.ShotBackCompassCell, "190"],
                                             [SurveyEditorCellIndex.ShotClinoCell, "0"],
                                             [SurveyEditorCellIndex.ShotBackClinoCell, "0"]]) {
                context.editorModel.setDataAt(context.editorModel.cellIndex(shotRow, cellRole), value)
            }
            tryCompare(context.trip.errorModel, "fatalCount", 0)
            tryCompare(context.trip.errorModel, "warningCount", 4)

            const menuButton = findInEditor("tripWarningsMenuButton")
            tryVerify(() => menuButton.visible, 5000, "the badge should offer its menu")

            mouseClick(menuButton)
            const ignoreAll = menuItem(menuButton, "ignoreAllWarnings")
            verify(ignoreAll.enabled, "Ignore all should be on while warnings show")
            ignoreAll.triggered()

            tryCompare(context.trip.errorModel, "warningCount", 0)
            tryCompare(context.trip.errorModel, "ignoredWarningCount", 4)

            // With every warning ignored the badge steps aside; the section
            // under it says what was ignored and can put them back
            const badge = findInEditor("surveyErrorOverview")
            const section = findInEditor("ignoredWarningsSection")
            const sectionText = findChild(section, "ignoredWarningsHeaderText")
            verify(sectionText !== null, "the ignored-warnings section should carry its text")
            tryVerify(() => !badge.visible && section.visible && sectionText.text.indexOf("4 ignored") >= 0, 5000,
                      "the badge should hide and the section say what was ignored: " + sectionText.text)
            verify(!menuButton.visible, "the menu button should hide with the badge")

            const showAll = findChild(section, "showAllIgnored")
            verify(showAll !== null, "the section should offer Show all")
            mouseClick(showAll)

            tryCompare(context.trip.errorModel, "warningCount", 4)
            tryCompare(context.trip.errorModel, "ignoredWarningCount", 0)
            tryVerify(() => badge.visible && menuButton.visible, 5000,
                      "the badge and its menu should return with the warnings")
        }

        function test_ignoreSimilarIgnoresOnlyThatKind() {
            const context = gotoSurveyTable(false)
            compare(context.trip.warningCountForTypeId(missingLeftId), 2)

            mouseClick(leftErrorIcon(context))

            const row = surveyTableId.stationRow(context, 0)
            const quoteBoxName = "errorBoxdataBox." + row + "." + SurveyEditorCellIndex.StationLeftCell
            let ignoreSimilar = null
            tryVerify(() => {
                const quoteBox = findChild(rootId.parent, quoteBoxName)
                ignoreSimilar = quoteBox === null ? null : findChild(quoteBox, "ignoreSimilar")
                return ignoreSimilar !== null && ignoreSimilar.visible
            }, 5000, "the Missing Left popup should offer to ignore every similar warning")
            verify(ignoreSimilar.text.indexOf("(2)") >= 0, "the link should count both Left warnings: " + ignoreSimilar.text)

            mouseClick(ignoreSimilar)

            tryCompare(context.trip.errorModel, "warningCount", 6)
            compare(context.trip.warningCountForTypeId(missingLeftId), 0)
            compare(context.trip.errorModel.ignoredWarningCount, 2)
        }

        function test_cellPopupClosesOnPressOutside() {
            const context = gotoSurveyTable(false)

            const icon = leftErrorIcon(context)
            mouseClick(icon)

            const row = surveyTableId.stationRow(context, 0)
            const quoteBoxName = "errorBoxdataBox." + row + "." + SurveyEditorCellIndex.StationLeftCell
            tryVerify(() => {
                const quoteBox = findChild(rootId.parent, quoteBoxName)
                return quoteBox !== null && quoteBox.visible
            }, 5000, "clicking the icon should open the cell's warnings")
            verify(icon.checked, "the icon should read as open")

            const otherRow = surveyTableId.stationRow(context, 1)
            surveyTableId.rowItem(this, context, otherRow)
            const otherStation = ObjectFinder.findObjectByChain(rootId.mainWindow,
                                                                "rootId->tripPage->surveyEditor->view->dataBox."
                                                                + otherRow + "." + SurveyEditorCellIndex.StationNameCell)
            verify(otherStation !== null, "A2's station cell should be on screen")
            mouseClick(otherStation)

            tryVerify(() => findChild(rootId.parent, quoteBoxName) === null, 5000,
                      "a press outside should close the cell's warnings")
            verify(!icon.checked, "the icon should uncheck so it can reopen")

            mouseClick(icon)
            tryVerify(() => findChild(rootId.parent, quoteBoxName) !== null, 5000,
                      "the icon should reopen the cell's warnings")
            mouseClick(icon)
            tryVerify(() => findChild(rootId.parent, quoteBoxName) === null, 5000,
                      "clicking the icon again should close them")
            verify(!icon.checked, "the icon should read as closed")
        }

        function test_ignoredCellKeepsAMutedMarker() {
            const context = gotoSurveyTable(false)
            context.chunk.setWarningIgnored(0, SurveyChunk.StationLeftRole, missingLeftId, true)
            tryCompare(context.trip.errorModel, "warningCount", 7)

            const icon = leftErrorIcon(context)
            tryVerify(() => icon.visible, 5000, "a cell with only ignored warnings should keep its marker")

            const image = findChild(icon, "errorIconImage")
            verify(image !== null, "the error icon should name its image")
            tryVerify(() => String(image.source).indexOf("warningIgnored") >= 0, 5000,
                      "the marker should use the ignored look: " + image.source)
        }
    }
}
