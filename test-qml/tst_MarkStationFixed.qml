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

            // The input CS comes from the shared picker, the same one the
            // FixStationPage rows use. Picked before the coordinate is typed,
            // as cwFixStation asks: the system decides which axis the text leads
            // with, and a new row starts on WGS84, where the UTM pair below
            // would be read latitude first.
            const model = context.cave.fixStations
            const modelIndex = model.index(0)

            const picker = findChild(popup, "fixStationPopupCS")
            verify(picker !== null, "popup should offer the shared CS picker")
            picker.committed("EPSG:26916")
            compare(model.data(modelIndex, FixStationModel.InputCSRole), "EPSG:26916")

            // Typing a coordinate: editingFinished is the same handler a keyboard
            // commit runs. One field takes the whole thing (#621).
            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            verify(coordinate !== null, "popup should offer the coordinate field")

            coordinate.text = "500200.5, 4194100, 2750m"
            coordinate.editingFinished()

            compare(model.data(modelIndex, FixStationModel.EastingRole), 500200.5)
            compare(model.data(modelIndex, FixStationModel.NorthingRole), 4194100.0)
            compare(model.data(modelIndex, FixStationModel.ElevationRole), 2750.0)
            compare(popup.parseError, "", "a coordinate it could read leaves no complaint")

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

        function test_popupShowsTheStringTheUserTyped() {
            // U14. This field is always an editor — there is no display half to
            // keep in project units — so it shows the user's own string
            // throughout, and falls back to rendering the numbers only for a row
            // that never had one.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            verify(coordinate !== null, "popup should offer the coordinate field")

            // The project is metric, so this elevation is stored as 838.2 m and
            // would read back as "838.2m" if the string weren't kept.
            coordinate.text = "500200.5, 4194100, 2750ft"
            coordinate.editingFinished()

            const model = context.cave.fixStations
            const modelIndex = model.index(0)
            fuzzyCompare(model.data(modelIndex, FixStationModel.ElevationRole),
                         2750 * 0.3048, 1e-9)

            const done = findChild(popup, "fixStationPopupDone")
            done.clicked()
            tryCompare(popup, "opened", false)

            openFixMenu(focusedStationBox(context, 0)).mark.triggered()
            tryCompare(popup, "opened", true)
            compare(coordinate.text, "500200.5, 4194100, 2750ft",
                    "re-opening offers what was typed, not a conversion of it")

            // A number written from anywhere else invalidates the string, and
            // the field drops back to rendering the row.
            model.setData(modelIndex, 1000.0, FixStationModel.ElevationRole)
            done.clicked()
            tryCompare(popup, "opened", false)

            openFixMenu(focusedStationBox(context, 0)).mark.triggered()
            tryCompare(popup, "opened", true)
            compare(coordinate.text, "500200.5, 4194100, 1000.000m",
                    "with no string of its own the field renders the numbers")

            done.clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupRejectsAPasteItCantRead() {
            // U11/U12 — free-form entry creates an error class ("couldn't make
            // sense of that") the popup had nowhere to put before this.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            // A projected system first: a new row starts on WGS84, where these
            // numbers are not a coordinate at all — 500200.5 is no latitude —
            // and the popup would rightly complain about the readable half too.
            findChild(popup, "fixStationPopupCS").committed("EPSG:26916")

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

            // Spelled out rather than lettered: a single N or W is a hemisphere
            // and reads fine (#654), so the near-miss that gets refused here is
            // the word.
            coordinate.text = "46.12113 north, 115.59902 west"
            coordinate.editingFinished()
            tryVerify(() => errorRow.visible, 2000, "the paste is refused")

            const picker = findChild(popup, "fixStationPopupCS")
            verify(picker !== null, "popup should offer a CS picker")

            // A new row starts on WGS84, so commit something else — committing
            // the CS it already holds would leave the model untouched and prove
            // nothing about what a CS change does to the pending text.
            const model = context.cave.fixStations
            compare(model.data(model.index(0), FixStationModel.InputCSRole), "EPSG:4326",
                    "a new row starts on WGS84")
            picker.committed("EPSG:32611")

            compare(coordinate.text, "46.12113 north, 115.59902 west",
                    "the refused text survives the CS change")
            tryVerify(() => errorRow.visible, 2000,
                      "and so does the reason it was refused")

            // The CS itself still landed, and a good coordinate still commits.
            tryVerify(() => model.data(model.index(0),
                                       FixStationModel.InputCSRole) === "EPSG:32611",
                      2000, "the CS the user picked must reach the row")

            coordinate.text = "478000, 4430000, 304m"
            coordinate.editingFinished()
            tryVerify(() => !errorRow.visible, 2000)

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupCSPickerOffersTheModesTheFixStationPageDoes() {
            // The popup is the second entry surface for a fix, and it sets its
            // picker's flags by hand rather than sharing a configured component
            // with FixStationPage — so the two can drift apart without a test
            // noticing.
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const picker = findChild(popup, "fixStationPopupCS")
            verify(picker !== null, "popup should offer a CS picker")

            // A fix station always has a coordinate system of its own, so there
            // is no "Local" here — that is what a blank input CS used to mean.
            const modeCombo = findChild(picker, "csModePicker")
            verify(modeCombo !== null, "csModePicker should be reachable")
            verify(modeCombo.model.indexOf("Lat/Lon (WGS84)") >= 0,
                   "the popup must offer a geographic fix CS")
            verify(modeCombo.model.indexOf("Local") < 0,
                   "a fix-station row must not offer Local")
            compare(modeCombo.model.length, 3, "Lat/Lon, UTM and Custom")

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

            // Written without spaces so the field can't be showing a rendering
            // of the numbers and pass for the text: format() always spaces its
            // separators, so only the stored string reads back like this.
            coordinate.text = "478000,4430000,1655m"
            coordinate.editingFinished()

            const model = context.cave.fixStations
            const modelIndex = model.index(0)
            compare(model.data(modelIndex, FixStationModel.EastingRole), 478000.0)
            compare(model.data(modelIndex, FixStationModel.NorthingRole), 4430000.0)

            // Switching to lat/long changes what the field asks for and how the
            // string is read — the coordinate *is* the string, so correcting the
            // system re-reads it rather than leaving it read the old way. The
            // words the user typed are untouched either way.
            picker.committed("EPSG:4326")
            tryCompare(label, "text", "Lat, Long, Elev", 2000,
                       "a geographic CS asks for latitude first")
            compare(coordinate.text, "478000,4430000,1655m",
                    "the string the user typed stays exactly as they typed it")
            compare(model.data(modelIndex, FixStationModel.NorthingRole), 478000.0,
                    "and it is now read latitude first")
            compare(model.data(modelIndex, FixStationModel.EastingRole), 4430000.0)

            coordinate.text = "46.12113, -115.59902, 304m"
            coordinate.editingFinished()
            compare(popup.parseError, "")
            fuzzyCompare(model.data(modelIndex, FixStationModel.NorthingRole), 46.12113, 1e-9)
            fuzzyCompare(model.data(modelIndex, FixStationModel.EastingRole), -115.59902, 1e-9)

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

//! Give A1 a fix carrying \a coordinate with no coordinate system to
        //! read it under — the shape a project saved before fixes carried their
        //! own system arrives in, and the one the popup can't create itself.
        //! Built before the popup opens, since the popup fills its fields once.
        function fixA1WithNoSystem(context, coordinate) {
            const model = context.cave.fixStations
            const row = model.addFixStation("A1")
            compare(row, 0)
            model.setData(model.index(row), "", FixStationModel.InputCSRole)
            compare(model.setCoordinateText(row, coordinate, ProjectUnits.unitSystem), "")
            return model
        }

        function test_popupSaysWhenACoordinateHasNoSystemToReadItUnder() {
            // The popup showed nothing for such a row: the field carried the
            // text, the row anchored no station, and the two facts never met.
            const context = gotoSurveyTable()
            const model = fixA1WithNoSystem(context, "610016.792, 5615117.075, 304m")
            const popup = popupForA1(context)

            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            const errorRow = findChild(popup, "fixStationPopupError")
            const errorText = findChild(popup, "fixStationPopupErrorText")

            compare(coordinate.text, "610016.792, 5615117.075, 304m",
                    "the text is still the user's")
            tryVerify(() => errorRow.visible, 5000, "and the popup says it can't be used")
            verify(popup.coordinateError !== "", "the complaint is the coordinate's")
            compare(errorText.text, popup.coordinateError)
            verify(errorText.text.indexOf("coordinate system") >= 0,
                   "and asks for the system: " + errorText.text)
            compare(coordinate.color, Theme.errorText, "the field carrying it is tinted")

            // Naming the system it was always in clears it, with no retyping.
            findChild(popup, "fixStationPopupCS").committed("EPSG:32611")
            tryVerify(() => !errorRow.visible, 5000, "naming the system clears the complaint")
            compare(model.data(model.index(0), FixStationModel.EastingRole), 610016.792)
            compare(coordinate.color, Theme.text)

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

        function test_popupAsksWhichWayRoundBeforeReadingASystemlessRowLatitudeFirst() {
            // The transposition nothing can detect afterwards, on the surface
            // that creates fixes for people who never open FixStationPage.
            const context = gotoSurveyTable()
            const model = fixA1WithNoSystem(context, "46.12113, -115.59902, 304m")
            const popup = popupForA1(context)

            const askBox = findChild(popup, "coordinateOrderAskBox")
            verify(askBox !== null, "the popup should host the order question")
            verify(!askBox.opened, "nothing has been asked yet")

            findChild(popup, "fixStationPopupCS").committed("EPSG:4326")
            tryVerify(() => askBox.opened, 5000,
                      "naming a geographic system on such a row has to ask")
            compare(findChild(askBox, "coordinateOrderAskSwapped").text,
                    "-115.59902, 46.12113, 304m")

            findChild(askBox, "coordinateOrderSwapButton").clicked()
            tryVerify(() => !askBox.opened, 5000)

            compare(model.data(model.index(0), FixStationModel.CoordinateTextRole),
                    "-115.59902, 46.12113, 304m", "the swap reaches the row")
            // And the field the user is looking at, which fills itself by hand
            // and would otherwise still show the coordinate they just replaced.
            tryCompare(findChild(popup, "fixStationPopupCoordinate"), "text",
                       "-115.59902, 46.12113, 304m", 5000)

            findChild(popup, "fixStationPopupDone").clicked()
            tryCompare(popup, "opened", false)
        }

        function test_answeringTheOrderQuestionKeepsTextTheModelRefused() {
            // Naming a coordinate system is the natural next move after the
            // model refuses a coordinate, and the order question follows the
            // naming — so answering it must not overwrite the text the user is
            // still being asked to correct. The CS commit itself already guards
            // this; the swap has to as well, or the guard only half exists.
            const context = gotoSurveyTable()
            const model = fixA1WithNoSystem(context, "46.12113, -115.59902, 304m")
            const popup = popupForA1(context)

            const field = findChild(popup, "fixStationPopupCoordinate")
            const askBox = findChild(popup, "coordinateOrderAskBox")

            // Refused, so the model still holds the original and the row is
            // still system-less — which is what leaves the question askable.
            field.text = "not a coordinate"
            field.editingFinished()
            tryVerify(() => popup.parseError !== "", 5000, "the model should refuse this")
            compare(model.data(model.index(0), FixStationModel.CoordinateTextRole),
                    "46.12113, -115.59902, 304m", "and keep what it had")

            findChild(popup, "fixStationPopupCS").committed("EPSG:4326")
            tryVerify(() => askBox.opened, 5000, "the question is still asked")
            findChild(askBox, "coordinateOrderSwapButton").clicked()
            tryVerify(() => !askBox.opened, 5000)

            // The row took the swap...
            compare(model.data(model.index(0), FixStationModel.CoordinateTextRole),
                    "-115.59902, 46.12113, 304m", "the swap still reaches the row")
            // ...and the field still holds what the user has to fix.
            compare(field.text, "not a coordinate",
                    "the refused text survives the answer")

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

        // The popup offers the same "pick it off the 3D view" the Fix Stations
        // page does, and starts the same pick — so the fix a station was marked
        // with from the survey table can be placed by clicking the terrain
        // without going anywhere else first.
        function test_popupOffersThePickFromViewButton() {
            const context = gotoSurveyTable()
            const popup = popupForA1(context)

            const pickButton = findChild(popup, "fixStationPopupPickFromView")
            verify(pickButton !== null, "the popup should offer a pick button")

            // Nothing is anywhere in particular until the project has a frame.
            compare(pickButton.canPick, false)

            findChild(popup, "fixStationPopupCS").committed("EPSG:4326")
            const coordinate = findChild(popup, "fixStationPopupCoordinate")
            coordinate.text = "37, -84, 300m"
            coordinate.editingFinished()
            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 5000,
                      "the typed fix should have anchored a frame")
            tryCompare(pickButton, "canPick", true, 5000)

            pickButton.clicked()

            // The popup gets out of the way and the pick names this fix.
            tryCompare(popup, "opened", false)
            compare(FixStationPick.active, true)
            compare(FixStationPick.cave, context.cave)
            compare(FixStationPick.stationName, "A1")

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "the pick should jump to the 3D view")

            FixStationPick.cancel()
        }
    }
}
