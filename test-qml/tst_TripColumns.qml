import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The trip page's Settings section (#174): two checkboxes, LRUD and Splays,
// that hide a column of the survey table. A column can be hidden only while it
// is empty, and a disabled checkbox says why on hover. The title row keeps the
// cells' widths whichever columns show.
//
// Phase 2 names the pieces this test reaches for: tripSettingsEditor holds
// lrudCheckBox and splaysCheckBox, each inside an enabled wrapper
// (lrudCheckBoxWrapper, splaysCheckBoxWrapper) that carries the hover tooltip
// (lrudCheckBoxToolTip, splaysCheckBoxToolTip). The column titles are
// stationTitle, distanceTitle, compassTitle, clinoTitle, lTitle, rTitle, uTitle,
// dTitle and splaysTitle.
MainWindowTest {
    id: rootId

    SurveyTableTestHelper {
        id: surveyTableId
        mainWindow: rootId.mainWindow
    }

    TestCase {
        name: "TripColumns"
        when: windowShown

        readonly property var lrudCells: [SurveyEditorCellIndex.StationLeftCell,
                                          SurveyEditorCellIndex.StationRightCell,
                                          SurveyEditorCellIndex.StationUpCell,
                                          SurveyEditorCellIndex.StationDownCell]

        function init() {
            surveyTableId.resetToViewPage(this)
        }

        function cleanup() {
            surveyTableId.leaveSurveyTable()
        }

        // A1 to A2 with every LRUD box empty: eight missing-LRUD warnings
        function gotoSurveyTable() {
            const context = surveyTableId.openSurveyTable(this, "ColumnsCave", "ColumnsTrip")
            surveyTableId.setStationName(this, context, 0, "A1")
            surveyTableId.setStationName(this, context, 1, "A2")
            tryVerify(() => context.trip.errorModel.warningCount > 0, 5000,
                      "empty LRUD boxes should warn")
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

        function titleLabel(context, name) {
            const titleRow = context.editorModel.toModelRow(
                               context.editorModel.rowIndex(context.chunk, -1, SurveyEditorRowIndex.TitleRow))
            verify(titleRow >= 0, "the chunk should have a title row")
            const item = surveyTableId.rowItem(this, context, titleRow)
            let found = null
            tryVerify(() => {
                found = findChild(item, name)
                return found !== null
            }, 5000, "the title row should have " + name)
            return found
        }

        function stationBox(context, indexInChunk, cellRole) {
            const row = surveyTableId.stationRow(context, indexInChunk)
            const item = surveyTableId.rowItem(this, context, row)
            return findChild(item, "dataBox." + row + "." + cellRole)
        }

        function setLeft(context, text) {
            const row = surveyTableId.stationRow(context, 0)
            context.editorModel.setDataAt(
                        context.editorModel.cellIndex(row, SurveyEditorCellIndex.StationLeftCell), text)
        }

        function hover(item) {
            mouseMove(item, item.width * 0.5, item.height * 0.5)
        }

        function shotBox(context, cellRole) {
            const row = context.editorModel.toModelRow(
                          context.editorModel.rowIndex(context.chunk, 0, SurveyEditorRowIndex.ShotRow))
            verify(row >= 0, "the chunk should have a shot row")
            const item = surveyTableId.rowItem(this, context, row)
            let found = null
            tryVerify(() => {
                found = findChild(item, "dataBox." + row + "." + cellRole)
                return found !== null
            }, 5000, "the shot row should have cell " + cellRole)
            return found
        }

        function splaysCell(context) {
            const row = surveyTableId.stationRow(context, 0)
            const item = surveyTableId.rowItem(this, context, row)
            let found = null
            tryVerify(() => {
                found = findChild(item, "splaysBox." + row)
                return found !== null
            }, 5000, "A1 should have a splays cell")
            return found
        }

        // Each visible title spans exactly the cells under it
        function verifyTitlesMatchCells(context, includeLrud) {
            const pairs = [["stationTitle", () => stationBox(context, 0, SurveyEditorCellIndex.StationNameCell)],
                           ["distanceTitle", () => shotBox(context, SurveyEditorCellIndex.ShotDistanceCell)],
                           ["compassTitle", () => shotBox(context, SurveyEditorCellIndex.ShotCompassCell)],
                           ["clinoTitle", () => shotBox(context, SurveyEditorCellIndex.ShotClinoCell)],
                           ["splaysTitle", () => splaysCell(context)]]
            if (includeLrud) {
                pairs.push(["lTitle", () => stationBox(context, 0, SurveyEditorCellIndex.StationLeftCell)])
                pairs.push(["dTitle", () => stationBox(context, 0, SurveyEditorCellIndex.StationDownCell)])
            }

            for (const [titleName, cellFor] of pairs) {
                tryVerify(() => {
                    const title = titleLabel(context, titleName)
                    const cell = cellFor()
                    if (title === null || cell === null) {
                        return false
                    }
                    const titleX = title.mapToItem(null, 0, 0).x
                    const cellX = cell.mapToItem(null, 0, 0).x
                    return Math.abs(titleX - cellX) <= 1 && Math.abs(title.width - cell.width) <= 1
                }, 5000, titleName + " should line up with its cells: title "
                         + titleLabel(context, titleName).mapToItem(null, 0, 0).x + "+" + titleLabel(context, titleName).width
                         + " cell " + cellFor().mapToItem(null, 0, 0).x + "+" + cellFor().width)
            }
        }

        function test_titlesLineUpWithTheirCells() {
            const context = gotoSurveyTable()
            verifyTitlesMatchCells(context, true)

            const lrud = findInEditor("lrudCheckBox")
            mouseClick(lrud)
            tryVerify(() => !context.trip.calibration.lruds, 5000, "the calibration should drop LRUDs")
            tryVerify(() => !titleLabel(context, "lTitle").visible, 5000, "the L title should hide")

            verifyTitlesMatchCells(context, false)
        }

        function test_columnsStartOn() {
            gotoSurveyTable()

            const lrud = findInEditor("lrudCheckBox")
            const splays = findInEditor("splaysCheckBox")
            verify(findInEditor("tripSettingsEditor") !== null)
            verify(lrud.checked, "LRUD should start checked")
            verify(lrud.enabled, "LRUD should start enabled")
            verify(splays.checked, "Splays should start checked")
            verify(splays.enabled, "Splays should start enabled")
        }

        function test_hidingLrudHidesTheColumnAndItsWarnings() {
            const context = gotoSurveyTable()
            const lrud = findInEditor("lrudCheckBox")

            mouseClick(lrud)
            tryVerify(() => !lrud.checked, 5000, "LRUD should uncheck")
            tryVerify(() => !context.trip.calibration.lruds, 5000, "the calibration should drop LRUDs")

            for (const name of ["lTitle", "rTitle", "uTitle", "dTitle"]) {
                const label = titleLabel(context, name)
                tryVerify(() => !label.visible, 5000, name + " should hide")
            }

            for (let station = 0; station < 2; station++) {
                for (const cellRole of lrudCells) {
                    tryVerify(() => {
                        const box = stationBox(context, station, cellRole)
                        return box === null || !box.visible
                    }, 5000, "station " + station + " cell " + cellRole + " should hide")
                }
            }

            tryCompare(context.trip.errorModel, "warningCount", 0)
        }

        function test_hidingSplaysHidesTheColumn() {
            const context = gotoSurveyTable()
            const splays = findInEditor("splaysCheckBox")

            mouseClick(splays)
            tryVerify(() => !context.trip.calibration.splays, 5000, "the calibration should drop splays")

            const label = titleLabel(context, "splaysTitle")
            tryVerify(() => !label.visible, 5000, "the Splays title should hide")

            for (let station = 0; station < 2; station++) {
                const row = surveyTableId.stationRow(context, station)
                const item = surveyTableId.rowItem(this, context, row)
                tryVerify(() => {
                    const box = findChild(item, "splaysBox." + row)
                    return box === null || !box.visible
                }, 5000, "station " + station + " should hide its splays cell")
            }
        }

        function test_lrudTextLocksTheColumnOn() {
            const context = gotoSurveyTable()
            const lrud = findInEditor("lrudCheckBox")
            const wrapper = findInEditor("lrudCheckBoxWrapper")
            const toolTip = findInEditor("lrudCheckBoxToolTip")

            setLeft(context, "1.5")
            tryVerify(() => context.trip.hasLrudData, 5000, "the trip should see its LRUD text")
            tryVerify(() => !lrud.enabled, 5000, "LRUD should lock while a box holds text")
            verify(lrud.checked, "a locked LRUD stays checked")

            hover(wrapper)
            tryVerify(() => toolTip.visible, 5000, "hovering the locked checkbox should explain why")
            verify(toolTip.text.indexOf("empty") >= 0, "the tooltip should say the boxes must be empty: " + toolTip.text)

            setLeft(context, "")
            tryVerify(() => lrud.enabled, 5000, "LRUD should unlock once the boxes are empty")
        }

        function test_aSplayLocksTheColumnOn() {
            const context = gotoSurveyTable()
            const splays = findInEditor("splaysCheckBox")
            const wrapper = findInEditor("splaysCheckBoxWrapper")
            const toolTip = findInEditor("splaysCheckBoxToolTip")

            const splay = TestHelper.a4SplayReadings()[0]
            TestHelper.addStationSplay(context.chunk, 0, splay.distance, splay.compass, splay.clino)
            tryVerify(() => context.trip.hasSplayData, 5000, "the trip should see its splay")
            tryVerify(() => !splays.enabled, 5000, "Splays should lock while a station has a splay")
            verify(splays.checked, "a locked Splays stays checked")

            hover(wrapper)
            tryVerify(() => toolTip.visible, 5000, "hovering the locked checkbox should explain why")
            verify(toolTip.text.indexOf("removed") >= 0, "the tooltip should say the splays must go: " + toolTip.text)
        }

        function test_aHiddenColumnCanAlwaysComeBack() {
            const context = gotoSurveyTable()

            // A trip that reached disk with LRUD text and the column off, which
            // the checkbox itself never allows
            setLeft(context, "1.5")
            context.trip.calibration.lruds = false

            const lrud = findInEditor("lrudCheckBox")
            tryVerify(() => !lrud.checked, 5000, "LRUD should read unchecked")
            verify(lrud.enabled, "an unchecked column can always be turned back on")

            mouseClick(lrud)
            tryVerify(() => context.trip.calibration.lruds, 5000, "the column should come back")
        }
    }
}
