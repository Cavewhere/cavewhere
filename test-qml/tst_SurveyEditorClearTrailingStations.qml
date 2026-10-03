import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Stations 1 -> 2 -> 3 with readings on the first shot only, then the names of
// 2 and 3 deleted. That leaves a nameless station holding a shot with data, and
// the table still offers a blank shot and station below it. Tabbing off that
// blank station used to announce rows the chunk never grew.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
        testWindow: rootId
    }

    TestCase {
        name: "SurveyEditorClearTrailingStations"
        when: windowShown

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        function dataBoxAt(context, row, column) {
            const item = surveyTableId.rowItem(this, context, row)

            let cell = null
            tryVerify(() => {
                cell = findChild(item, "dataBox." + row + "." + column)
                return cell !== null
            }, 5000, "row " + row + " should have a cell for role " + column)
            return cell
        }

        function setCell(context, row, column, text) {
            mouseClick(dataBoxAt(context, row, column))
            for (let i = 0; i < text.length; i++) {
                keyClick(text.charCodeAt(i))
            }
            keyClick(Qt.Key_Return)
            waitForRendering(rootId)
        }

        function clearCell(context, row, column) {
            mouseClick(dataBoxAt(context, row, column))
            keyClick(Qt.Key_Return)
            keyClick(Qt.Key_Delete)
            keyClick(Qt.Key_Return)
            waitForRendering(rootId)
        }

        function stationRow(context, chunk, indexInChunk) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(chunk, indexInChunk,
                                                     SurveyEditorRowIndex.StationRow))
        }

        function shotRow(context, chunk, indexInChunk) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(chunk, indexInChunk,
                                                     SurveyEditorRowIndex.ShotRow))
        }

        // The table cell holding keyboard focus, or null when focus sits
        // outside the table.
        function focusedDataBox() {
            let item = rootId.Window.window.activeFocusItem
            while (item !== null && item !== undefined) {
                if (item.objectName !== undefined && item.objectName.indexOf("dataBox.") === 0) {
                    return item
                }
                item = item.parent
            }
            return null
        }

        // Every chunk draws a title, then station, shot, station ... in order,
        // and at most one chunk draws one blank shot and station past its own.
        // The view has to agree with the model on how many rows that makes.
        function verifyTableMatchesChunks(context, when) {
            const model = context.editorModel
            let row = 0
            let chunksWithBlankRows = 0

            for (let c = 0; c < context.trip.chunkCount; c++) {
                const chunk = context.trip.chunk(c)
                const label = when + ", chunk " + c
                compare(chunk.shotCount, chunk.stationCount - 1, label + ": one shot between each station")

                verify(row < model.rowCount(), label + ": should have a title row")
                const title = model.data(model.index(row, 0), SurveyEditorModel.RowIndexRole)
                verify(title.chunk === chunk, label + ": title row belongs to the chunk")
                compare(title.rowType, SurveyEditorRowIndex.TitleRow, label + ": starts with a title row")
                row++

                let dataRows = 0
                while (row < model.rowCount()) {
                    const rowIndex = model.data(model.index(row, 0), SurveyEditorModel.RowIndexRole)
                    if (rowIndex.chunk !== chunk || rowIndex.rowType === SurveyEditorRowIndex.TitleRow) {
                        break
                    }

                    const expectStation = dataRows % 2 === 0
                    compare(rowIndex.rowType,
                            expectStation ? SurveyEditorRowIndex.StationRow : SurveyEditorRowIndex.ShotRow,
                            label + ": row " + row + " type")
                    compare(rowIndex.indexInChunk, Math.floor(dataRows / 2),
                            label + ": row " + row + " index in chunk")
                    dataRows++
                    row++
                }

                const chunkRows = chunk.stationCount + chunk.shotCount
                verify(dataRows === chunkRows || dataRows === chunkRows + 2,
                       label + ": draws " + dataRows + " rows for " + chunkRows + " stations and shots")
                if (dataRows > chunkRows) {
                    chunksWithBlankRows++
                }
            }

            compare(row, model.rowCount(), when + ": every row belongs to a chunk")
            verify(chunksWithBlankRows <= 1, when + ": only the focused chunk draws blank rows")
            compare(context.view.count, model.rowCount(), when + ": view and model agree on the row count")
        }

        function clearTrailingStationsThenTab(context) {
            failOnWarning(/QAbstractItemModel/)

            const chunk = context.chunk

            setCell(context, stationRow(context, chunk, 0), SurveyChunk.StationNameRole, "1")
            setCell(context, stationRow(context, chunk, 1), SurveyChunk.StationNameRole, "2")
            setCell(context, stationRow(context, chunk, 2), SurveyChunk.StationNameRole, "3")
            setCell(context, shotRow(context, chunk, 0), SurveyChunk.ShotDistanceRole, "10")
            setCell(context, shotRow(context, chunk, 0), SurveyChunk.ShotCompassRole, "20")
            setCell(context, shotRow(context, chunk, 0), SurveyChunk.ShotClinoRole, "5")

            compare(chunk.stationCount, 3)
            compare(chunk.shotCount, 2)
            compare(chunk.data(SurveyChunk.StationNameRole, 2), "3")
            compare(chunk.data(SurveyChunk.ShotDistanceRole, 1).value, "")
            verifyTableMatchesChunks(context, "after entering 1 -> 2 -> 3")

            clearCell(context, stationRow(context, chunk, 1), SurveyChunk.StationNameRole)
            verifyTableMatchesChunks(context, "after deleting 2")

            const rowWhere3Was = stationRow(context, chunk, 2)
            clearCell(context, rowWhere3Was, SurveyChunk.StationNameRole)
            verifyTableMatchesChunks(context, "after deleting 3")

            // A table that stops offering the row leaves nothing to tab off.
            if (stationRow(context, chunk, 2) === rowWhere3Was) {
                const cellWhere3Was = dataBoxAt(context, rowWhere3Was, SurveyChunk.StationNameRole)
                mouseClick(cellWhere3Was)
                tryVerify(() => cellWhere3Was.focus, 5000, "the cell where 3 was should take focus")

                keyClick(Qt.Key_Tab)
                waitForRendering(rootId)

                verifyTableMatchesChunks(context, "after Tab")

                let focused = null
                tryVerify(() => {
                    focused = focusedDataBox()
                    return focused !== null && focused.dataValue.rowIndex.chunk === chunk
                }, 5000, "Tab should keep focus on a cell of the chunk being edited, focus is on "
                         + rootId.Window.window.activeFocusItem)
            }

            // The first shot is the data the user kept
            compare(chunk.data(SurveyChunk.StationNameRole, 0), "1")
            compare(chunk.data(SurveyChunk.ShotDistanceRole, 0).value, "10")
            compare(chunk.data(SurveyChunk.ShotCompassRole, 0).value, "20")
            compare(chunk.data(SurveyChunk.ShotClinoRole, 0).value, "5")
        }

        function test_tabAfterClearingTrailingStations() {
            const context = surveyTableId.openSurveyTable(this, "TrailingCave", "TrailingTrip")

            clearTrailingStationsThenTab(context)
        }

        function test_tabAfterClearingTrailingStations_chunkBelow() {
            const context = surveyTableId.openSurveyTable(this, "TrailingCave", "TrailingTrip")
            context.trip.addNewChunk()
            const chunkBelow = context.trip.chunk(1)
            verifyTableMatchesChunks(context, "after adding the chunk below")

            clearTrailingStationsThenTab(context)

            // The chunk below was never touched
            compare(context.trip.chunkCount, 2)
            verify(context.trip.chunk(1) === chunkBelow)
            compare(chunkBelow.stationCount, 2)
            compare(chunkBelow.shotCount, 1)
            compare(chunkBelow.data(SurveyChunk.StationNameRole, 0), "")
            compare(chunkBelow.data(SurveyChunk.StationNameRole, 1), "")
        }
    }
}
