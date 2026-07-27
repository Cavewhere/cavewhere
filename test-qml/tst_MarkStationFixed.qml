import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// U2 — marking a station Fixed from the survey table, without leaving for
// FixStationPage. The fixes these create are the cave's real cwFixStationModel
// rows, so every assertion here reads back through that model.
MainWindowTest {
    id: rootId

    TestCase {
        name: "MarkStationFixed"
        when: windowShown

        function init() {
            if (GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.closeEditor()
            }
            GlobalShadowTextInput.enabled = false

            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "should be on view page at start of test")
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        // A cave with one trip whose chunk has two named stations, sitting on the
        // trip page with the survey table up.
        function gotoSurveyTable() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "FixCave"
            cave.addTrip()
            const trip = cave.trip(0)
            trip.name = "FixTrip"
            trip.addNewChunk()
            const chunk = trip.chunk(0)

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "should land on tripPage")

            let view = null
            tryVerify(() => {
                view = ObjectFinder.findObjectByChain(
                            mainWindow, "rootId->tripPage->surveyEditor->view")
                return view !== null && view.model !== null
            }, 5000, "survey editor view should be reachable")

            const editorModel = view.model
            const context = {
                cave: cave,
                chunk: chunk,
                view: view,
                editorModel: editorModel
            }

            setStationName(context, 0, "A1")
            setStationName(context, 1, "A2")

            return context
        }

        function stationRow(context, indexInChunk) {
            return context.editorModel.toModelRow(
                        context.editorModel.rowIndex(context.chunk,
                                                     indexInChunk,
                                                     SurveyEditorRowIndex.StationRow))
        }

        function setStationName(context, indexInChunk, name) {
            const row = stationRow(context, indexInChunk)
            verify(row >= 0, "station " + indexInChunk + " should have a model row")
            context.editorModel.setDataAt(
                        context.editorModel.cellIndex(row, SurveyChunk.StationNameRole), name)
            tryVerify(() => context.chunk.data(SurveyChunk.StationNameRole, indexInChunk) === name,
                      5000, "station " + indexInChunk + " should be named " + name)
        }

        // The station cell for `indexInChunk`, scrolled into view and focused —
        // the caret only loads on a focused cell.
        function focusedStationBox(context, indexInChunk) {
            const row = stationRow(context, indexInChunk)
            verify(row >= 0, "station " + indexInChunk + " should have a model row")

            context.view.positionViewAtIndex(row, ListView.Contain)
            context.editorModel.setFocusedCell(
                        context.editorModel.cellIndex(row, SurveyChunk.StationNameRole))

            let box = null
            tryVerify(() => {
                box = ObjectFinder.findObjectByChain(
                            mainWindow,
                            "rootId->tripPage->surveyEditor->view->dataBox."
                            + row + "." + SurveyChunk.StationNameRole)
                return box !== null && box.focus
            }, 5000, "station box " + indexInChunk + " should be focused")

            return box
        }

        // Opens the caret menu on a station cell and returns its items.
        function openFixMenu(stationBox) {
            let caret = null
            tryVerify(() => {
                caret = findChild(stationBox, "fixStationMenuButton")
                return caret !== null
            }, 5000, "focused station cell should offer a fix caret")

            // The menu lives in a Loader that ContextMenuButton activates on
            // click; emitting clicked() runs the same handler without depending
            // on the caret's position under offscreen rendering.
            caret.clicked()

            let markItem = null
            let removeItem = null
            tryVerify(() => {
                markItem = findChild(stationBox, "markStationFixedMenuItem")
                removeItem = findChild(stationBox, "removeStationFixMenuItem")
                return markItem !== null && removeItem !== null
            }, 5000, "fix caret menu should have both items")

            return { mark: markItem, remove: removeItem }
        }

        function fixStationPopup() {
            // Found by QObject search, not by the visual-item chain: a Popup isn't
            // in the survey editor's item tree until it opens.
            const surveyEditor = ObjectFinder.findObjectByChain(
                                   mainWindow, "rootId->tripPage->surveyEditor")
            verify(surveyEditor !== null, "survey editor should be reachable")

            let popup = null
            tryVerify(() => {
                popup = findChild(surveyEditor, "fixStationPopup")
                return popup !== null
            }, 5000, "survey editor should host a fix station popup")
            return popup
        }

        function test_blankStationOffersNoFix() {
            const context = gotoSurveyTable()

            // The trailing virtual station row only exists while its chunk is
            // focused, so focus a real cell in the chunk first.
            focusedStationBox(context, 0)

            // That trailing row has no name, so there is nothing to anchor and no
            // caret to offer.
            const virtualBox = focusedStationBox(context, context.chunk.stationCount)
            compare(virtualBox.canFix, false)
            compare(findChild(virtualBox, "fixStationMenuButton"), null)
        }

        function test_markStationFixedAddsTheFix() {
            const context = gotoSurveyTable()
            compare(context.cave.fixStations.count, 0)

            const stationBox = focusedStationBox(context, 0)
            const menu = openFixMenu(stationBox)
            compare(menu.mark.text, "Mark Station as Fixed")
            compare(menu.remove.enabled, false)

            menu.mark.triggered()

            tryCompare(context.cave.fixStations, "count", 1)
            const model = context.cave.fixStations
            compare(model.data(model.index(0), FixStationModel.StationNameRole), "A1")
        }

        function test_fixedStationShowsBadge() {
            const context = gotoSurveyTable()
            const stationBox = focusedStationBox(context, 0)

            const badge = findChild(stationBox, "fixedStationBadge")
            verify(badge !== null, "station cell should carry a Fixed badge")
            compare(badge.visible, false)

            // Marked from the model rather than the menu: the badge has to track
            // the fix rows themselves, whichever surface added them.
            context.cave.fixStations.addFixStation("A1")
            tryCompare(badge, "visible", true)

            // A rename moves the badge without changing the row count, which is
            // the case countChanged() alone would miss.
            const model = context.cave.fixStations
            model.setData(model.index(0), "A2", FixStationModel.StationNameRole)
            tryCompare(badge, "visible", false)
        }

        function test_markingAnAlreadyFixedStationDoesNotDuplicate() {
            const context = gotoSurveyTable()
            context.cave.fixStations.addFixStation("A1")
            tryCompare(context.cave.fixStations, "count", 1)

            const stationBox = focusedStationBox(context, 0)
            const menu = openFixMenu(stationBox)

            // Already fixed, so the item edits rather than marks.
            compare(menu.mark.text, "Edit Fixed Coordinates...")
            compare(menu.remove.enabled, true)

            menu.mark.triggered()
            compare(context.cave.fixStations.count, 1)
        }

        function test_removeFixDropsTheFix() {
            const context = gotoSurveyTable()
            context.cave.fixStations.addFixStation("A1")
            tryCompare(context.cave.fixStations, "count", 1)

            const stationBox = focusedStationBox(context, 0)
            const menu = openFixMenu(stationBox)
            menu.remove.triggered()

            tryCompare(context.cave.fixStations, "count", 0)

            const badge = findChild(stationBox, "fixedStationBadge")
            tryCompare(badge, "visible", false)
        }

        function test_popupEditsTheFixInPlace() {
            const context = gotoSurveyTable()

            const stationBox = focusedStationBox(context, 0)
            const menu = openFixMenu(stationBox)
            menu.mark.triggered()

            const popup = fixStationPopup()
            tryCompare(popup, "opened", true)
            compare(popup.stationName, "A1")
            compare(popup.row, 0)

            const title = findChild(popup, "fixStationPopupTitle")
            verify(title !== null, "popup should name the station being fixed")
            compare(title.text, "Fix station A1")

            // Typing a coordinate: editingFinished is the same handler a keyboard
            // commit runs.
            const easting = findChild(popup, "fixStationPopupEasting")
            const northing = findChild(popup, "fixStationPopupNorthing")
            const elevation = findChild(popup, "fixStationPopupElevation")
            verify(easting !== null && northing !== null && elevation !== null,
                   "popup should offer all three coordinate fields")

            easting.text = "500200.5"
            easting.editingFinished()
            northing.text = "4194100"
            northing.editingFinished()
            elevation.text = "2750"
            elevation.editingFinished()

            const model = context.cave.fixStations
            const modelIndex = model.index(0)
            compare(model.data(modelIndex, FixStationModel.EastingRole), 500200.5)
            compare(model.data(modelIndex, FixStationModel.NorthingRole), 4194100.0)
            compare(model.data(modelIndex, FixStationModel.ElevationRole), 2750.0)

            // The input CS comes from the shared picker, the same one the
            // FixStationPage rows use.
            const picker = findChild(popup, "fixStationPopupCS")
            verify(picker !== null, "popup should offer the shared CS picker")
            picker.committed("EPSG:26916")
            compare(model.data(modelIndex, FixStationModel.InputCSRole), "EPSG:26916")

            // The picker has to show what the fix now carries: it doesn't own its
            // own value, so without the editor feeding the commit back its controls
            // would still read as the previous CS.
            compare(picker.value, "EPSG:26916")
            const csName = findChild(popup, "fixStationPopupCSName")
            verify(csName !== null, "popup should name the picked CS")
            tryVerify(() => csName.visible && csName.text !== "",
                      5000, "picked CS should resolve to a name")

            // Re-opening reads the stored row back rather than showing stale text.
            const done = findChild(popup, "fixStationPopupDone")
            verify(done !== null, "popup should offer a way to close")
            done.clicked()
            tryCompare(popup, "opened", false)

            const reopenMenu = openFixMenu(focusedStationBox(context, 0))
            reopenMenu.mark.triggered()
            tryCompare(popup, "opened", true)
            compare(easting.text, "500200.5")
            compare(picker.value, "EPSG:26916")

            // Left open, it would sit in the window overlay and eat clicks for
            // whichever test runs next.
            done.clicked()
            tryCompare(popup, "opened", false)
        }
    }
}
