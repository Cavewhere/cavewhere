import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// "Merge with chunk above" and "Merge with chunk below" on the shot and station
// menus: A1 to A4 split at the shot A2 to A3 gives A1 to A2 and A2 to A4, which
// tie at A2.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "SurveyChunkMerge"
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
            const context = surveyTableId.openSurveyTable(this, "MergeCave", "MergeTrip")
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

        function titleRow(context, chunk) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(chunk, -1, SurveyEditorRowIndex.TitleRow))
        }

        function test_shotMenuOffersBothMergeItems() {
            const context = gotoSurveyTable()

            const menu = surveyTableId.openMenu(this, context, shotRow(context, context.chunk, 0),
                                                SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            const above = surveyTableId.menuItem(this, menu, "shotMenuMergeChunkAbove", "Merge with chunk above")
            const below = surveyTableId.menuItem(this, menu, "shotMenuMergeChunkBelow", "Merge with chunk below")

            verify(!above.enabled, "the first chunk has no chunk above it")
            verify(below.enabled, "the chunk below starts with the tie station A2")
            surveyTableId.closeMenu(this, menu)
        }

        function test_shotMenuMergeBelowRemovesTheLowerTitleRow() {
            const context = gotoSurveyTable()
            const lower = context.lower
            verify(titleRow(context, lower) > titleRow(context, context.chunk))

            const menu = surveyTableId.openMenu(this, context, shotRow(context, context.chunk, 0),
                                                SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            const below = surveyTableId.menuItem(this, menu, "shotMenuMergeChunkBelow", "Merge with chunk below")
            verify(below.enabled, "the tie matches")

            surveyTableId.closeMenu(this, menu)
            below.triggered()

            tryCompare(context.trip, "chunkCount", 1)
            compare(context.trip.chunk(0), context.chunk)
            compare(context.chunk.stationCount, 4)
            compare(context.chunk.data(SurveyChunk.StationNameRole, 3), "A4")
            compare(context.chunk.data(SurveyChunk.ShotDistanceRole, splitShot).value, "5")
            const model = context.editorModel
            tryVerify(() => {
                for (let row = 0; row < model.rowCount(); row++) {
                    if (model.chunkForRow(row) !== context.chunk) {
                        return false
                    }
                }
                return true
            }, 5000, "every row, including the lower chunk's old title row, should address the surviving chunk")
        }

        function test_stationMenuMergesWithTheChunkAbove() {
            const context = gotoSurveyTable()
            const lower = context.lower

            const menu = surveyTableId.openMenu(this, context, stationRow(context, lower, 1),
                                                SurveyEditorCellIndex.StationNameCell, "stationMenuLoader")
            const above = surveyTableId.menuItem(this, menu, "stationMenuMergeChunkAbove", "Merge with chunk above")
            const below = surveyTableId.menuItem(this, menu, "stationMenuMergeChunkBelow", "Merge with chunk below")
            verify(above.enabled, "the chunk above ends with the tie station A2")
            verify(!below.enabled, "the last chunk has no chunk below it")

            surveyTableId.closeMenu(this, menu)
            above.triggered()

            tryCompare(context.trip, "chunkCount", 1)
            compare(context.chunk.stationCount, 4)
        }

        function test_mismatchedTieDisablesMergeWithTheSameText() {
            const context = gotoSurveyTable()
            const model = context.editorModel
            const row = shotRow(context, context.chunk, 0)

            let menu = surveyTableId.openMenu(this, context, row, SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            let below = surveyTableId.menuItem(this, menu, "shotMenuMergeChunkBelow", "Merge with chunk below")
            verify(below.enabled, "the tie matches before the rename")
            surveyTableId.closeMenu(this, menu)

            // The menu's loader stays built, so reopening the Chunk submenu must ask again
            const lowerTieRow = stationRow(context, context.lower, 0)
            verify(model.setDataAt(model.cellIndex(lowerTieRow, SurveyEditorCellIndex.StationNameCell), "B2"))
            tryVerify(() => context.lower.data(SurveyChunk.StationNameRole, 0) === "B2",
                      5000, "the lower chunk should start at B2")

            menu = surveyTableId.openMenu(this, context, row, SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            const chunkMenu = surveyTableId.openSubMenu(this, menu, "shotMenuChunkMenu")
            below = surveyTableId.menuItem(this, chunkMenu, "shotMenuMergeChunkBelow", "Merge with chunk below")
            verify(!below.enabled, "A2 and B2 are different names")
            surveyTableId.closeMenu(this, menu)

            compare(context.trip.chunkCount, 2)
        }
    }
}
