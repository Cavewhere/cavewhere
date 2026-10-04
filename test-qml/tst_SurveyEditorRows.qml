import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The survey table's row colors: shot cells alternate in bands, station cells
// take one flat fill, the shot that holds the keyboard is tinted with an arrow
// from its from station to its to station, and a station that holds the
// keyboard is tinted along with its splays.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
        testWindow: rootId
    }

    TestCase {
        name: "SurveyEditorRows"
        when: windowShown

        // The station the a4 splays hang from: A2, between shots 0 and 1
        readonly property int splayStation: 1

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        // A1 to A4 in the first chunk, A2 carrying the a4 splays with its
        // cluster open, and an empty second chunk to hold the keyboard so the
        // first sits at rest.
        function gotoSurveyTable() {
            const context = surveyTableId.openSurveyTable(this, "RowCave", "RowTrip")
            const model = context.editorModel
            const names = ["A1", "A2", "A3", "A4"]

            surveyTableId.setStationName(this, context, 0, names[0])
            surveyTableId.setStationName(this, context, 1, names[1])
            for (let i = 2; i < names.length; i++) {
                // Focusing the chunk brings in the trailing blank station the
                // next name is typed into
                model.setFocusedCell(model.cellIndex(surveyTableId.stationRow(context, i - 1),
                                                     SurveyEditorCellIndex.StationNameCell))
                tryCompare(model, "focusedRow", surveyTableId.stationRow(context, i - 1))
                surveyTableId.setStationName(this, context, i, names[i])
            }
            tryCompare(context.chunk, "stationCount", names.length)

            const splays = TestHelper.a4SplayReadings()
            for (const splay of splays) {
                TestHelper.addStationSplay(context.chunk, splayStation,
                                           splay.distance, splay.compass, splay.clino)
            }
            model.toggleSplaysExpanded(model.rowIndex(context.chunk, splayStation,
                                                      SurveyEditorRowIndex.StationRow))

            context.trip.addNewChunk()
            const otherChunk = context.trip.chunk(1)
            const otherRow = model.toModelRow(model.rowIndex(otherChunk, 0, SurveyEditorRowIndex.StationRow))
            verify(otherRow >= 0, "the second chunk should have a station row")
            model.setFocusedCell(model.cellIndex(otherRow, SurveyEditorCellIndex.StationNameCell))
            tryCompare(model, "focusedRow", otherRow)

            return context
        }

        function shotRow(context, shotIndex) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(context.chunk, shotIndex, SurveyEditorRowIndex.ShotRow))
        }

        function splayRow(context, splayIndex) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(context.chunk, splayStation,
                                                     SurveyEditorRowIndex.StationRow)) + 1 + splayIndex
        }

        function cell(context, row, cellRole) {
            const item = surveyTableId.rowItem(this, context, row)
            const name = cellRole === SurveyEditorCellIndex.StationSplaysCell
                       ? "splaysBox." + row
                       : "dataBox." + row + "." + cellRole
            let found = null
            tryVerify(() => {
                found = findChild(item, name)
                return found !== null
            }, 5000, "row " + row + " should have cell " + cellRole)
            return found
        }

        function fill(cellItem) {
            const rect = findChild(cellItem, "cellFill")
            verify(rect !== null && rect.visible, cellItem.objectName + " should draw its fill")
            return rect.color
        }

        function compareFill(cellItem, expected, message) {
            tryVerify(() => Qt.colorEqual(fill(cellItem), expected), 5000,
                      message + " (" + cellItem.objectName + " is " + fill(cellItem) + ")")
        }

        function selectedHalf(cellItem) {
            const half = findChild(cellItem, "cellSelectedShotHalf")
            verify(half !== null, cellItem.objectName + " should have a selected-shot half")
            return half
        }

        function stationCells(context, stationIndex) {
            const row = surveyTableId.stationRow(context, stationIndex)
            return [SurveyEditorCellIndex.StationNameCell,
                    SurveyEditorCellIndex.StationLeftCell,
                    SurveyEditorCellIndex.StationRightCell,
                    SurveyEditorCellIndex.StationUpCell,
                    SurveyEditorCellIndex.StationDownCell,
                    SurveyEditorCellIndex.StationSplaysCell].map(role => cell(context, row, role))
        }

        function shotCells(context, shotIndex) {
            const row = shotRow(context, shotIndex)
            return [SurveyEditorCellIndex.ShotDistanceCell,
                    SurveyEditorCellIndex.ShotCompassCell,
                    SurveyEditorCellIndex.ShotClinoCell].map(role => cell(context, row, role))
        }

        function splayCells(context) {
            const cells = []
            for (let i = 0; i < TestHelper.a4SplayReadings().length; i++) {
                for (const role of [SurveyEditorCellIndex.SplayDistanceCell,
                                    SurveyEditorCellIndex.SplayCompassCell,
                                    SurveyEditorCellIndex.SplayClinoCell]) {
                    cells.push(cell(context, splayRow(context, i), role))
                }
            }
            return cells
        }

        function shotArrow(context, shotIndex) {
            return findChild(surveyTableId.rowItem(this, context, shotRow(context, shotIndex)), "shotArrow")
        }

        function band(shotIndex) {
            return shotIndex % 2 === 0 ? Theme.background : Theme.rowAlternate
        }

        // The label a station name cell draws its name with
        function nameLabel(nameCell) {
            const input = findChild(nameCell, "coreTextInput")
            verify(input !== null, "the name cell should hold its text")
            for (const child of input.children) {
                if (child.text !== undefined && child.contentHeight !== undefined) {
                    return child
                }
            }
            fail("the name cell should hold a label")
        }

        function test_restingRowsBandOnlyTheShots() {
            const context = gotoSurveyTable()

            for (let station = 0; station < context.chunk.stationCount; station++) {
                for (const stationCell of stationCells(context, station)) {
                    compareFill(stationCell, Theme.background, "a station cell rests on the background")
                    verify(!selectedHalf(stationCell).visible,
                           stationCell.objectName + " should draw no split half at rest")
                }
            }

            for (let shot = 0; shot < context.chunk.shotCount; shot++) {
                for (const shotCell of shotCells(context, shot)) {
                    compareFill(shotCell, band(shot), "shot " + shot + " takes its band")
                }
            }
            verify(!Qt.colorEqual(band(0), band(1)), "neighboring shots should differ in fill")

            for (const splayCell of splayCells(context)) {
                compareFill(splayCell, Theme.splaySurface, "a splay keeps its accent at rest")
            }

            for (let shot = 0; shot < context.chunk.shotCount; shot++) {
                tryVerify(() => shotArrow(context, shot) === null, 5000, "no shot shows an arrow at rest")
            }
        }

        function test_selectedShotTintsItsCellsAndShowsTheArrow() {
            const context = gotoSurveyTable()
            const model = context.editorModel
            const selectedShot = 0

            model.setFocusedCell(model.cellIndex(shotRow(context, selectedShot),
                                                 SurveyEditorCellIndex.ShotCompassCell))
            tryCompare(model, "focusedRow", shotRow(context, selectedShot))

            for (const shotCell of shotCells(context, selectedShot)) {
                compareFill(shotCell, Theme.rowCurrent, "the selected shot is tinted")
            }
            for (let shot = 1; shot < context.chunk.shotCount; shot++) {
                for (const shotCell of shotCells(context, shot)) {
                    compareFill(shotCell, band(shot), "shot " + shot + " stays in its band")
                }
            }

            // The from station's lower half and the to station's upper half
            for (const fromCell of stationCells(context, selectedShot)) {
                compareFill(fromCell, Theme.background, "the from station keeps its flat fill")
                const half = selectedHalf(fromCell)
                tryVerify(() => half.visible, 5000, fromCell.objectName + " should tint a half")
                compare(half.y, fromCell.height / 2, "the from station tints its lower half")
                verify(Qt.colorEqual(half.color, Theme.rowCurrent))
            }
            for (const toCell of stationCells(context, selectedShot + 1)) {
                const half = selectedHalf(toCell)
                tryVerify(() => half.visible, 5000, toCell.objectName + " should tint a half")
                compare(half.y, 0, "the to station tints its upper half")
            }
            for (const otherCell of stationCells(context, selectedShot + 2)) {
                verify(!selectedHalf(otherCell).visible, "a station off the shot draws no half")
            }

            let arrow = null
            tryVerify(() => {
                arrow = shotArrow(context, selectedShot)
                return arrow !== null && arrow.visible && arrow.width > 0
            }, 5000, "the selected shot should show its arrow")
            for (let shot = 1; shot < context.chunk.shotCount; shot++) {
                tryVerify(() => shotArrow(context, shot) === null, 5000,
                          "only the selected shot shows an arrow")
            }

            // Centered across the name column and on the seam between the two
            // name cells, clear of both names' text
            const fromName = cell(context, surveyTableId.stationRow(context, selectedShot),
                                  SurveyEditorCellIndex.StationNameCell)
            const toName = cell(context, surveyTableId.stationRow(context, selectedShot + 1),
                                SurveyEditorCellIndex.StationNameCell)
            const content = context.view.contentItem
            const arrowCenter = arrow.mapToItem(content, arrow.width / 2, arrow.height / 2)
            const fromCenter = fromName.mapToItem(content, fromName.width / 2, 0)
            const fromBottom = fromName.mapToItem(content, 0, fromName.height).y
            const toTop = toName.mapToItem(content, 0, 0).y
            fuzzyCompare(arrowCenter.x, fromCenter.x, 0.5, "the arrow centers on the name column")
            fuzzyCompare(arrowCenter.y, (fromBottom + toTop) / 2, 1, "the arrow centers on the seam")
            compare(arrow.width, Theme.shotArrowSize, "the default font leaves room for the full arrow")

            const fromLabel = nameLabel(fromName)
            const toLabel = nameLabel(toName)
            const arrowTop = arrow.mapToItem(content, 0, 0).y
            const arrowBottom = arrow.mapToItem(content, 0, arrow.height).y
            verify(arrowTop > fromLabel.mapToItem(content, 0, fromLabel.height).y,
                   "the arrow should clear the from station's name")
            verify(arrowBottom < toLabel.mapToItem(content, 0, 0).y,
                   "the arrow should clear the to station's name")
            verify(!arrow.enabled, "the arrow takes no pointer input")
        }

        function checkStationSelected(context, message) {
            const model = context.editorModel
            for (const stationCell of stationCells(context, splayStation)) {
                compareFill(stationCell, Theme.rowCurrent, message + ": the station is tinted")
                verify(!selectedHalf(stationCell).visible, "a selected station draws no split half")
            }
            for (const splayCell of splayCells(context)) {
                compareFill(splayCell, Theme.rowCurrent, message + ": its splays are tinted")
            }
            for (const otherCell of stationCells(context, splayStation + 1)) {
                compareFill(otherCell, Theme.background, message + ": other stations rest")
            }
            for (let shot = 0; shot < context.chunk.shotCount; shot++) {
                for (const shotCell of shotCells(context, shot)) {
                    compareFill(shotCell, band(shot), message + ": shot " + shot + " stays in its band")
                }
                tryVerify(() => shotArrow(context, shot) === null, 5000, message + ": no arrow shows")
            }
        }

        function test_focusedLrudSelectsItsStation() {
            const context = gotoSurveyTable()
            const model = context.editorModel

            // A shot first, so the arrow has something to hide
            model.setFocusedCell(model.cellIndex(shotRow(context, 0), SurveyEditorCellIndex.ShotDistanceCell))
            tryVerify(() => shotArrow(context, 0) !== null, 5000, "the shot should show its arrow")

            const row = surveyTableId.stationRow(context, splayStation)
            model.setFocusedCell(model.cellIndex(row, SurveyEditorCellIndex.StationLeftCell))
            tryCompare(model, "focusedRow", row)
            checkStationSelected(context, "an LRUD cell")
        }

        function test_focusedSplaySelectsItsStation() {
            const context = gotoSurveyTable()
            const model = context.editorModel

            const row = splayRow(context, 1)
            model.setFocusedCell(model.cellIndex(row, SurveyEditorCellIndex.SplayCompassCell))
            tryCompare(model, "focusedRow", row)
            checkStationSelected(context, "a splay cell")
        }
    }
}
