import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// "Split chunk here" on the shot and station menus: a chunk A1 to A4 with a
// distance on the shot A2 to A3 splits there into A1 to A2 and A2 to A4.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "SurveyChunkSplit"
        when: windowShown

        readonly property int splitShot: 1

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        function gotoSurveyTable() {
            const context = surveyTableId.openSurveyTable(this, "SplitCave", "SplitTrip")
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

            verify(model.setDataAt(model.cellIndex(shotRow(context, splitShot),
                                                   SurveyEditorCellIndex.ShotDistanceCell), "5"))
            tryVerify(() => context.chunk.data(SurveyChunk.ShotDistanceRole, splitShot).value === "5",
                      5000, "the split shot should carry a distance")
            return context
        }

        function shotRow(context, shotIndex) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(context.chunk, shotIndex, SurveyEditorRowIndex.ShotRow))
        }

        function titleRow(context, chunk) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(chunk, -1, SurveyEditorRowIndex.TitleRow))
        }

        function cell(context, row, cellRole) {
            const item = surveyTableId.rowItem(this, context, row)
            let found = null
            tryVerify(() => {
                found = findChild(item, "dataBox." + row + "." + cellRole)
                return found !== null
            }, 5000, "row " + row + " should have cell " + cellRole)
            return found
        }

        // Right-clicks the cell and returns its menu once the loader has built it
        function openMenu(context, row, cellRole, loaderName) {
            const cellItem = cell(context, row, cellRole)
            mouseClick(cellItem, cellItem.width / 2, cellItem.height / 2, Qt.RightButton)

            const loaderPath = "rootId->tripPage->surveyEditor->view->dataBox." + row + "." + cellRole
                    + "->" + loaderName
            let loader = null
            tryVerify(() => {
                loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                return loader !== null && loader.item !== null
            }, 5000, loaderName + " should build its menu")
            tryVerify(() => loader.item.opened, 5000, loaderName + " should be open")
            return loader.item
        }

        function splitItem(menu, objectName) {
            const item = findChild(menu, objectName)
            verify(item !== null, "the menu should offer " + objectName)
            compare(item.text, "Split chunk here")
            return item
        }

        function test_shotMenuSplitsTheChunk() {
            const context = gotoSurveyTable()
            const model = context.editorModel
            compare(context.trip.chunkCount, 1)

            const menu = openMenu(context, shotRow(context, splitShot),
                                  SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            const item = splitItem(menu, "shotMenuSplitChunk")
            verify(item.enabled, "a shot past the first should be a split point")

            menu.close()
            item.triggered()

            tryCompare(context.trip, "chunkCount", 2)
            const newChunk = context.trip.chunk(1)
            verify(titleRow(context, newChunk) > titleRow(context, context.chunk),
                   "the new chunk should get a title row below the first chunk's")
            compare(context.chunk.stationCount, 2)
            compare(newChunk.stationCount, 3)
            compare(newChunk.data(SurveyChunk.StationNameRole, 0), "A2")
            compare(newChunk.data(SurveyChunk.ShotDistanceRole, 0).value, "5")

            const newFirstStation = model.toModelRow(model.rowIndex(newChunk, 0, SurveyEditorRowIndex.StationRow))
            tryCompare(model, "focusedRow", newFirstStation)
        }

        function test_shotMenuSplitIsDisabledOnTheFirstShot() {
            const context = gotoSurveyTable()

            const menu = openMenu(context, shotRow(context, 0),
                                  SurveyEditorCellIndex.ShotDistanceCell, "shotMenuLoader")
            const item = splitItem(menu, "shotMenuSplitChunk")
            verify(!item.enabled, "splitting at the first shot would leave an empty chunk")

            // A split point exists further down, so the item is disabled for
            // the first shot and not for every shot
            verify(context.editorModel.canSplitChunkAt(
                       context.editorModel.cellIndex(shotRow(context, splitShot),
                                                     SurveyEditorCellIndex.ShotDistanceCell)))
            menu.close()
        }

        function test_stationMenuSplitsAtTheShotBelow() {
            const context = gotoSurveyTable()

            const menu = openMenu(context, surveyTableId.stationRow(context, splitShot),
                                  SurveyEditorCellIndex.StationNameCell, "stationMenuLoader")
            const item = splitItem(menu, "stationMenuSplitChunk")
            verify(item.enabled, "A2 has a shot below it to split at")

            menu.close()
            item.triggered()

            tryCompare(context.trip, "chunkCount", 2)
            compare(context.trip.chunk(1).data(SurveyChunk.StationNameRole, 0), "A2")
            compare(context.chunk.stationCount, 2)
        }

        function test_splitPreviewDrawsADividerUnderTheShot() {
            const context = gotoSurveyTable()
            const distanceCell = cell(context, shotRow(context, splitShot),
                                      SurveyEditorCellIndex.ShotDistanceCell)
            const preview = distanceCell.removePreview
            verify(preview !== null)

            preview.previewSplit(context.chunk, splitShot)

            const splitRowItem = surveyTableId.rowItem(this, context, shotRow(context, splitShot))
            let line = null
            tryVerify(() => {
                line = findChild(splitRowItem, "splitPreviewLine")
                return line !== null && line.visible
            }, 5000, "the split shot should show the split divider")

            const otherLine = findChild(surveyTableId.rowItem(this, context, shotRow(context, 0)),
                                        "splitPreviewLine")
            verify(otherLine === null || !otherLine.visible, "only the split shot draws the divider")
            verify(!distanceCell.removePreviewActive, "a split strikes nothing out")

            preview.clear()
            tryVerify(() => !line.visible, 5000, "clearing the preview should take the divider away")
        }
    }
}
