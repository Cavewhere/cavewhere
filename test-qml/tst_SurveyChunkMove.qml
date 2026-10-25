import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// "Move chunk up" and "Move chunk down" on the shot and station menus: A1 to A4
// split at the shot A2 to A3 gives A1 to A2 and then A2 to A4.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "SurveyChunkMove"
        when: windowShown

        readonly property int splitShot: 1

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        // Returns the context with `lower` holding the second chunk
        function gotoSurveyTable() {
            const context = surveyTableId.openSurveyTable(this, "MoveCave", "MoveTrip")
            const model = context.editorModel
            const names = ["A1", "A2", "A3", "A4"]

            surveyTableId.setStationName(this, context, 0, names[0])
            surveyTableId.setStationName(this, context, 1, names[1])
            for (let i = 2; i < names.length; i++) {
                model.setFocusedCell(model.cellIndex(surveyTableId.stationRow(context, i - 1),
                                                     SurveyEditorCellIndex.StationNameCell))
                tryCompare(model, "focusedRow", surveyTableId.stationRow(context, i - 1))
                surveyTableId.setStationName(this, context, i, names[i])
            }
            tryCompare(context.chunk, "stationCount", names.length)

            const splitCell = model.cellIndex(shotRow(context, context.chunk, splitShot),
                                              SurveyEditorCellIndex.ShotDistanceCell)
            verify(model.setDataAt(splitCell, "5"))
            tryVerify(() => context.chunk.data(SurveyChunk.ShotDistanceRole, splitShot).value === "5",
                      5000, "the split shot should carry a distance")
            model.splitChunkAt(splitCell)
            tryCompare(context.trip, "chunkCount", 2)

            context.lower = context.trip.chunk(1)
            compare(context.lower.data(SurveyChunk.StationNameRole, 0), "A2")
            return context
        }

        function shotRow(context, chunk, shotIndex) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(chunk, shotIndex, SurveyEditorRowIndex.ShotRow))
        }

        function stationRow(context, chunk, stationIndex) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(chunk, stationIndex, SurveyEditorRowIndex.StationRow))
        }

        function test_shotMenuOffersBothMoveItems() {
            const context = gotoSurveyTable()

            const menu = surveyTableId.openMenu(this, context, shotRow(context, context.chunk, 0),
                                                SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            const up = surveyTableId.menuItem(this, menu, "shotMenuMoveChunkUp", "Move chunk up")
            const down = surveyTableId.menuItem(this, menu, "shotMenuMoveChunkDown", "Move chunk down")

            verify(!up.enabled, "the first chunk has no chunk above it")
            verify(down.enabled, "the first chunk has a chunk below it")
            surveyTableId.closeMenu(this, menu)
        }

        function test_stationMenuOnTheLastChunkMovesOnlyUp() {
            const context = gotoSurveyTable()

            const menu = surveyTableId.openMenu(this, context, stationRow(context, context.lower, 1),
                                                SurveyEditorCellIndex.StationNameCell, "stationMenuLoader")
            const up = surveyTableId.menuItem(this, menu, "stationMenuMoveChunkUp", "Move chunk up")
            const down = surveyTableId.menuItem(this, menu, "stationMenuMoveChunkDown", "Move chunk down")

            verify(up.enabled, "the last chunk has a chunk above it")
            verify(!down.enabled, "the last chunk has no chunk below it")
            surveyTableId.closeMenu(this, menu)
        }

        function test_shotMenuMoveDownPutsTheFirstChunkLast() {
            const context = gotoSurveyTable()

            const menu = surveyTableId.openMenu(this, context, shotRow(context, context.chunk, 0),
                                                SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            const down = surveyTableId.menuItem(this, menu, "shotMenuMoveChunkDown", "Move chunk down")
            verify(down.enabled, "the first chunk has a chunk below it")

            surveyTableId.closeMenu(this, menu)
            down.triggered()

            tryVerify(() => context.trip.chunk(1) === context.chunk, 5000,
                      "A1 to A2 should be the second chunk")
            compare(context.trip.chunk(0), context.lower)
            compare(context.trip.chunkCount, 2)
            compare(context.chunk.data(SurveyChunk.StationNameRole, 0), "A1")
            compare(context.lower.data(SurveyChunk.StationNameRole, 2), "A4")
        }

        function test_moveUpKeepsTheFocusedCell() {
            const context = gotoSurveyTable()
            const model = context.editorModel
            const a4 = 2

            // Above the move: the first chunk's title, A1, a shot and A2, then
            // the lower chunk's title, A2, a shot, A3, a shot and A4
            const a4RowBefore = 9
            model.setFocusedCell(model.cellIndex(stationRow(context, context.lower, a4),
                                                 SurveyEditorCellIndex.StationNameCell))
            tryCompare(model, "focusedRow", a4RowBefore)

            // A right click focuses its cell, so open the menu on A4 itself
            const menu = surveyTableId.openMenu(this, context, a4RowBefore,
                                                SurveyEditorCellIndex.StationNameCell, "stationMenuLoader")
            const up = surveyTableId.menuItem(this, menu, "stationMenuMoveChunkUp", "Move chunk up")
            verify(up.enabled, "the last chunk has a chunk above it")

            surveyTableId.closeMenu(this, menu)
            up.triggered()

            tryVerify(() => context.trip.chunk(0) === context.lower, 5000,
                      "A2 to A4 should be the first chunk")

            // The lower chunk's six rows now come first
            const a4RowAfter = 5
            tryCompare(model, "focusedRow", a4RowAfter)
            compare(model.focusedRowIndex.chunk, context.lower)
            compare(model.focusedRowIndex.indexInChunk, a4)
            compare(model.focusedRole, SurveyEditorCellIndex.StationNameCell)

            const focusedCell = surveyTableId.cell(this, context, a4RowAfter, SurveyEditorCellIndex.StationNameCell)
            tryVerify(() => focusedCell.activeFocus, 5000, "A4's name cell should keep the keyboard")
        }
    }
}
