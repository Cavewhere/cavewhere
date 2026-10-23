import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The accepted-warnings section under the trip's warning badge (#174, Phase 4):
// one collapsed line while anything is accepted, opening to the accepted
// warnings grouped by kind, rare kinds first. Unticking a row un-accepts it,
// a group's link un-accepts the whole group, and tapping a row's text focuses
// its cell in the survey table.
//
// Phase 4 names the pieces this test reaches for: acceptedWarningsSection has
// an expanded property, a tappable acceptedWarningsHeader and its text in
// acceptedWarningsHeaderText. Each group is acceptedWarningsGroup.<i> (in
// display order) with kind, count and expanded properties, a tappable
// groupHeader, an unacceptGroup link, and rows acceptedWarningRow.<j> that
// hold the "suppress" checkbox and the "errorText" label.
//
// Written in Phase 0 and red until Phase 4 lands.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "ReviewAcceptedWarnings"
        when: windowShown

        readonly property string disagreementKind: "Backsight disagreement"
        readonly property string missingLrudKind: "Missing LRUD"

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        // A1 to A2: A1's four LRUD boxes are empty and the shot's compass (10)
        // disagrees with its backsight (195) by 5 degrees, five warnings in all.
        function gotoSurveyTable() {
            const context = surveyTableId.openSurveyTable(this, "ReviewCave", "ReviewTrip")
            const model = context.editorModel
            surveyTableId.setStationName(this, context, 0, "A1")
            surveyTableId.setStationName(this, context, 1, "A2")

            const stationRow = surveyTableId.stationRow(context, 1)
            for (const cellRole of [SurveyEditorCellIndex.StationLeftCell,
                                    SurveyEditorCellIndex.StationRightCell,
                                    SurveyEditorCellIndex.StationUpCell,
                                    SurveyEditorCellIndex.StationDownCell]) {
                model.setDataAt(model.cellIndex(stationRow, cellRole), "1")
            }

            const shotRow = model.toModelRow(model.rowIndex(context.chunk, 0, SurveyEditorRowIndex.ShotRow))
            verify(shotRow >= 0, "the chunk should have a shot row")
            const shot = [[SurveyEditorCellIndex.ShotDistanceCell, "10"],
                          [SurveyEditorCellIndex.ShotCompassCell, "10"],
                          [SurveyEditorCellIndex.ShotBackCompassCell, "195"],
                          [SurveyEditorCellIndex.ShotClinoCell, "0"],
                          [SurveyEditorCellIndex.ShotBackClinoCell, "0"]]
            for (const [cellRole, value] of shot) {
                model.setDataAt(model.cellIndex(shotRow, cellRole), value)
            }

            tryCompare(context.trip.errorModel, "warningCount", 5)
            return context
        }

        function acceptEverything(context) {
            context.trip.acceptAllWarnings()
            tryCompare(context.trip.errorModel, "suppressedWarningCount", 5)
        }

        function section() {
            const editor = surveyTableId.surveyEditor()
            return editor === null ? null : findChild(editor, "acceptedWarningsSection")
        }

        function visibleSection() {
            let found = null
            tryVerify(() => {
                found = section()
                return found !== null && found.visible
            }, 5000, "the accepted-warnings section should show")
            return found
        }

        // Opening the section or a group grows the table's header layouts,
        // which place their children on the next polish; clicking before then
        // would land on wherever the target sat while collapsed
        function settledClick(target) {
            verify(waitForPolish(target.Window.window), "the window should finish laying out")
            mouseClick(target)
        }

        function tapChild(parentItem, name) {
            const target = findChild(parentItem, name)
            verify(target !== null, parentItem.objectName + " should have " + name)
            settledClick(target)
        }

        function groups(sectionItem) {
            const found = []
            for (let i = 0; ; i++) {
                const group = findChild(sectionItem, "acceptedWarningsGroup." + i)
                if (group === null || !group.visible) {
                    return found
                }
                found.push(group)
            }
        }

        function expandedSection() {
            const sectionItem = visibleSection()
            // Typing the shot left the table scrolled to that row, with the
            // header that holds the section above the top of the view
            const view = findChild(surveyTableId.surveyEditor(), "view")
            verify(view !== null, "the survey editor should have its table view")
            verify(waitForPolish(view.Window.window), "the header should finish growing first")
            view.contentY = view.originY
            tapChild(sectionItem, "acceptedWarningsHeader")
            tryVerify(() => sectionItem.expanded, 5000, "tapping the header should open the section")
            tryVerify(() => groups(sectionItem).length === 2, 5000, "the section should list two kinds")
            return sectionItem
        }

        function groupOfKind(sectionItem, kind) {
            const match = groups(sectionItem).find(group => group.kind === kind)
            verify(match !== undefined, "the section should have a " + kind + " group")
            return match
        }

        function test_sectionStartsCollapsedWithTheCount() {
            const context = gotoSurveyTable()
            const before = section()
            verify(before === null || !before.visible,
                   "nothing accepted, so the section should be absent")

            acceptEverything(context)

            const sectionItem = visibleSection()
            verify(!sectionItem.expanded, "the section should start collapsed")
            const headerText = findChild(sectionItem, "acceptedWarningsHeaderText")
            verify(headerText !== null, "the section should carry its header text")
            verify(headerText.text.indexOf("5") >= 0, "the header should count five: " + headerText.text)
        }

        function test_groupsListRareKindsFirstAndUnaccept() {
            const context = gotoSurveyTable()
            acceptEverything(context)
            const sectionItem = expandedSection()

            const shown = groups(sectionItem)
            compare(shown[0].kind, disagreementKind, "the rare kind should sort first")
            compare(shown[1].kind, missingLrudKind)
            verify(!shown[0].expanded && !shown[1].expanded, "groups should start collapsed")

            const disagreement = groupOfKind(sectionItem, disagreementKind)
            tapChild(disagreement, "groupHeader")
            tryVerify(() => disagreement.expanded, 5000, "tapping a group's header should open it")

            let row = null
            tryVerify(() => {
                row = findChild(disagreement, "acceptedWarningRow.0")
                return row !== null
            }, 5000, "the disagreement group should list its warning")
            compare(findChild(disagreement, "acceptedWarningRow.1"), null, "the disagreement group holds one row")

            const checkbox = findChild(row, "suppress")
            verify(checkbox !== null, "a row should carry its checkbox")
            verify(checkbox.checked)
            settledClick(checkbox)

            tryCompare(context.trip.errorModel, "warningCount", 1)
            tryVerify(() => groups(sectionItem).length === 1, 5000,
                      "an emptied group should leave the section")

            const lrud = groupOfKind(sectionItem, missingLrudKind)
            tapChild(lrud, "unacceptGroup")

            tryCompare(context.trip.errorModel, "warningCount", 5)
            tryCompare(context.trip.errorModel, "suppressedWarningCount", 0)
            tryVerify(() => section() === null || !section().visible, 5000,
                      "with nothing accepted the section should go")
        }

        function test_tappingARowFocusesItsCell() {
            const context = gotoSurveyTable()
            acceptEverything(context)
            const sectionItem = expandedSection()

            const lrud = groupOfKind(sectionItem, missingLrudKind)
            tapChild(lrud, "groupHeader")
            tryVerify(() => lrud.expanded, 5000)

            let row = null
            tryVerify(() => {
                row = findChild(lrud, "acceptedWarningRow.0")
                return row !== null
            }, 5000, "the LRUD group should list its warnings")
            // Opening the group grows the header past the table's top edge;
            // scroll back to the top so the row is on screen
            const view = findChild(surveyTableId.surveyEditor(), "view")
            verify(waitForPolish(view.Window.window), "the group should finish growing first")
            view.contentY = view.originY
            tapChild(row, "errorText")

            const a1Row = surveyTableId.stationRow(context, 0)
            tryCompare(context.editorModel, "focusedRow", a1Row)
            verify([SurveyEditorCellIndex.StationLeftCell,
                    SurveyEditorCellIndex.StationRightCell,
                    SurveyEditorCellIndex.StationUpCell,
                    SurveyEditorCellIndex.StationDownCell].indexOf(context.editorModel.focusedRole) >= 0,
                   "the focus should land on one of A1's LRUD cells, not " + context.editorModel.focusedRole)
        }
    }
}
