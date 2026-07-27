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

        // Opens the popup on station A1 and returns it, ready to edit.
        function popupForA1(context) {
            const menu = openFixMenu(focusedStationBox(context, 0))
            menu.mark.triggered()

            const popup = fixStationPopup()
            tryCompare(popup, "opened", true)
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

            const popup = popupForA1(context)
            compare(popup.stationName, "A1")
            compare(popup.row, 0)

            const title = findChild(popup, "fixStationPopupTitle")
            verify(title !== null, "popup should name the station being fixed")
            compare(title.text, "Fix station A1")

            // Typing a coordinate: editingFinished is the same handler a keyboard
            // commit runs. One field takes the whole thing (#621).
            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            verify(coordinate !== null, "popup should offer the coordinate field")

            coordinate.text = "500200.5, 4194100, 2750m"
            coordinate.editingFinished()

            const model = context.cave.fixStations
            const modelIndex = model.index(0)
            compare(model.data(modelIndex, FixStationModel.EastingRole), 500200.5)
            compare(model.data(modelIndex, FixStationModel.NorthingRole), 4194100.0)
            compare(model.data(modelIndex, FixStationModel.ElevationRole), 2750.0)
            compare(popup.parseError, "", "a coordinate it could read leaves no complaint")

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
            compare(coordinate.text, "500200.5, 4194100, 2750m")
            compare(picker.value, "EPSG:26916")

            // Left open, it would sit in the window overlay and eat clicks for
            // whichever test runs next.
            done.clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupRejectsAPasteItCantRead() {
            // U11/U12 — free-form entry creates an error class ("couldn't make
            // sense of that") the popup had nowhere to put before this.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            verify(coordinate !== null, "popup should offer the coordinate field")

            coordinate.text = "500200.5, 4194100, 2750m"
            coordinate.editingFinished()

            const errorRow = findChild(popup, "fixStationPopupError")
            const errorText = findChild(popup, "fixStationPopupErrorText")
            verify(errorRow !== null && errorText !== null, "popup should have an error line")
            tryVerify(() => !errorRow.visible, 2000, "a readable coordinate shows no error")

            coordinate.text = "somewhere near the entrance"
            coordinate.editingFinished()

            tryVerify(() => errorRow.visible, 2000, "an unreadable paste is reported")
            verify(errorText.text !== "", "and the reason is spelled out, not just flagged")

            // A refused paste must leave the fix exactly as it was.
            const model = context.cave.fixStations
            const modelIndex = model.index(0)
            compare(model.data(modelIndex, FixStationModel.EastingRole), 500200.5)
            compare(model.data(modelIndex, FixStationModel.NorthingRole), 4194100.0)

            // ...and what the user typed stays put, so it can be corrected
            // rather than retyped.
            compare(coordinate.text, "somewhere near the entrance")

            // Fixing the text clears the complaint.
            coordinate.text = "500200.5, 4194100, 2751m"
            coordinate.editingFinished()
            tryVerify(() => !errorRow.visible, 2000, "a readable coordinate clears the error")

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupKeepsRefusedTextWhenTheCSChanges() {
            // Changing the CS is the natural next move after a refusal — the
            // user assumes that's what upset it. The popup re-renders the field
            // on a CS change, because geographic and projected write their axes
            // in opposite orders; doing that while a parse error is pending
            // would replace the text they still have to fix with the coordinate
            // they were trying to replace, and drop the reason with it.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            const errorRow = findChild(popup, "fixStationPopupError")
            verify(coordinate !== null && errorRow !== null)

            coordinate.text = "46.12113 N, 115.59902 W"
            coordinate.editingFinished()
            tryVerify(() => errorRow.visible, 2000, "the paste is refused")

            const picker = findChild(popup, "fixStationPopupCS")
            verify(picker !== null, "popup should offer a CS picker")
            picker.committed("EPSG:4326")

            compare(coordinate.text, "46.12113 N, 115.59902 W",
                    "the refused text survives the CS change")
            tryVerify(() => errorRow.visible, 2000,
                      "and so does the reason it was refused")

            // The CS itself still landed, and a good coordinate still commits.
            const model = context.cave.fixStations
            compare(model.data(model.index(0), FixStationModel.InputCSRole), "EPSG:4326")

            coordinate.text = "46.12113, -115.59902, 304m"
            coordinate.editingFinished()
            tryVerify(() => !errorRow.visible, 2000)

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupShowsTheWarningsTheFixStationPageShows() {
            // U12 — before this, a coordinate typed here could sit outside its
            // own CS and the popup looked exactly the same either way. The only
            // way to find out was to navigate to FixStationPage.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            const picker = findChild(popup, "fixStationPopupCS")
            const errorRow = findChild(popup, "fixStationPopupError")
            const errorText = findChild(popup, "fixStationPopupErrorText")

            picker.committed("EPSG:32613")
            coordinate.text = "478000, 4430000, 1655m"
            coordinate.editingFinished()
            tryVerify(() => !errorRow.visible, 2000, "a coordinate inside its CS is quiet")

            // Transposed leading digit: still a number, still parses, but no
            // longer anywhere near UTM zone 13.
            coordinate.text = "1478000, 4430000, 1655m"
            coordinate.editingFinished()

            tryVerify(() => errorRow.visible, 5000,
                      "a coordinate outside its CS is reported without leaving the popup")
            compare(popup.parseError, "", "it parsed fine — this is the domain warning")
            verify(errorText.text === popup.domainError && popup.domainError !== "",
                   "and the message is the domain one")

            // Both derived roles are read from the same row, so pin each to what
            // the model actually says.
            const diagnostics = context.cave.fixStationDiagnostics
            const diagnosticsIndex = diagnostics.index(0)
            compare(popup.domainError,
                    diagnostics.data(diagnosticsIndex,
                                     FixStationDiagnosticsModel.DomainErrorRole))

            // stationError has to be driven to something before comparing it —
            // both sides are empty while the fix names a real station, so the
            // comparison would hold just as well against a property wired to
            // nothing at all. Renaming the fix to a station the survey doesn't
            // have is what makes it speak.
            const model = context.cave.fixStations
            model.setData(model.index(0), "ZZ9", FixStationModel.StationNameRole)
            tryVerify(() => popup.stationError !== "", 2000,
                      "an unknown station name reaches the popup")
            compare(popup.stationError,
                    diagnostics.data(diagnosticsIndex,
                                     FixStationDiagnosticsModel.StationErrorRole))
            model.setData(model.index(0), "A1", FixStationModel.StationNameRole)
            tryCompare(popup, "stationError", "")

            // A parse failure is the more immediate problem, so it takes the line.
            coordinate.text = "not a coordinate"
            coordinate.editingFinished()
            tryVerify(() => popup.parseError !== "", 2000)
            compare(errorText.text, popup.parseError,
                    "the parse failure outranks the domain warning still sitting there")
            verify(popup.domainError !== "", "which is still there")

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupWritesLatitudeFirstForAGeographicCS() {
            // The popup has to follow the same lat-first rule as the page, and
            // say which order it wants — the numbers alone can't tell the user,
            // and getting it wrong transposes the fix silently.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            const picker = findChild(popup, "fixStationPopupCS")
            const label = findChild(popup, "fixStationPopupCoordinateLabel")
            verify(label !== null, "popup should label the coordinate field")

            picker.committed("EPSG:32613")
            tryCompare(label, "text", "East, North, Elev", 2000,
                       "a projected CS asks for easting first")

            coordinate.text = "478000, 4430000, 1655m"
            coordinate.editingFinished()

            const model = context.cave.fixStations
            const modelIndex = model.index(0)
            compare(model.data(modelIndex, FixStationModel.EastingRole), 478000.0)
            compare(model.data(modelIndex, FixStationModel.NorthingRole), 4430000.0)

            // Switching to lat/long re-renders the field in the other order
            // without touching the stored coordinate.
            picker.committed("EPSG:4326")
            tryCompare(label, "text", "Lat, Long, Elev", 2000,
                       "a geographic CS asks for latitude first")
            tryCompare(coordinate, "text", "4430000, 478000, 1655m", 2000,
                       "and the field swaps to match")
            compare(model.data(modelIndex, FixStationModel.EastingRole), 478000.0,
                    "re-rendering wrote nothing to the model")

            coordinate.text = "46.12113, -115.59902, 304m"
            coordinate.editingFinished()
            compare(popup.parseError, "")
            fuzzyCompare(model.data(modelIndex, FixStationModel.NorthingRole), 46.12113, 1e-9)
            fuzzyCompare(model.data(modelIndex, FixStationModel.EastingRole), -115.59902, 1e-9)

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupErrorsFollowTheRowWhenItIsReopened() {
            // The popup fills itself imperatively, so a stale complaint from the
            // previous row would otherwise survive the next open.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            const errorRow = findChild(popup, "fixStationPopupError")

            coordinate.text = "nonsense"
            coordinate.editingFinished()
            tryVerify(() => errorRow.visible, 2000, "the paste is refused")

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)

            // Give A1's row a coordinate that really is outside its CS, before
            // opening on anyone else. Every other error assertion in this file
            // runs on row 0, so without a flagged row 0 to contrast against,
            // reading the diagnostics at a hardcoded index(0) would look right.
            const model = context.cave.fixStations
            model.setData(model.index(0), "EPSG:32613", FixStationModel.InputCSRole)
            model.setData(model.index(0), 1478000.0, FixStationModel.EastingRole)

            const diagnostics = context.cave.fixStationDiagnostics
            tryVerify(() => diagnostics.data(diagnostics.index(0),
                                             FixStationDiagnosticsModel.DomainErrorRole) !== "",
                      5000, "row 0 really is flagged now")

            // A different station, whose own fix has nothing wrong with it.
            const menu = openFixMenu(focusedStationBox(context, 1))
            menu.mark.triggered()
            tryCompare(popup, "opened", true)
            compare(popup.stationName, "A2")
            compare(popup.row, 1, "the popup is on the second fix")

            compare(popup.parseError, "", "the previous row's complaint doesn't follow")
            compare(popup.domainError, "",
                    "and neither does the other row's, which is the one on file")
            tryVerify(() => !errorRow.visible, 2000, "so the error line is gone")

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }
    }
}
