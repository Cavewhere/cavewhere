import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// The Fix Stations page's station cell opens the station editor, which offers
// the names a fix on the row's node takes: an attached trip's stations under
// the trip's label ("trip1.entrance"), a label the UI shows nowhere else. The
// completer stays advisory (D6): a name outside the list still commits.
MainWindowTest {
    id: rootId

    //! The visible descendants of \a item whose text is exactly \a text.
    function itemsWithText(item, text) {
        let found = []
        if (item === null) {
            return found
        }
        const children = item.children
        for (let i = 0; i < children.length; ++i) {
            const child = children[i]
            if (child.visible && child.text === text) {
                found.push(child)
            }
            found = found.concat(rootId.itemsWithText(child, text))
        }
        return found
    }

    ExternalCenterlineTestCase {
        name: "FixStationAutocomplete"
        when: windowShown

        readonly property string caveName: "AutoCave"
        readonly property string fixture: "external-centerlines/survex_fix_autocomplete.svx"
        readonly property string fileStationName: "entrance"
        readonly property string strayName: "nowhere"
        readonly property int narrowWidthMargin: 100

        // Captured once, not bound, so a test that narrows the window still
        // restores the default.
        property int defaultWindowWidth: 0

        Component.onCompleted: defaultWindowWidth = rootId.width

        // Retargeted per test: the scope model follows the editor.
        SignalSpy {
            id: resetSpyId
            signalName: "modelReset"
        }

        function init() {
            rootId.closeAnyOpenEditor()

            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "should be on view page at start of test")
        }

        function cleanup() {
            rootId.closeAnyOpenEditor()
            resetSpyId.target = null
            rootId.width = defaultWindowWidth
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        //! A saved project holding cave AutoCave, whose one trip is attached to
        //! the fixture, with \a fixCount blank fix rows on the cave. Returns
        //! the cave and the scoped name a fix on "entrance" takes.
        function makeCaveWithAttachedTrip(projectBaseName, fixCount) {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = caveName
            cave.addTrip()
            const trip = cave.trip(0)

            saveProjectAs(projectBaseName)
            attachSourceToTrip(trip, TestHelper.testcasesDatasetPath(fixture))
            tryVerify(() => trip.externalStations.indexOf(fileStationName) >= 0, 20000,
                      "the scan harvests " + fileStationName + ", got " + trip.externalStations)
            verify(trip.scopePrefix.length > 0, "an attached trip scopes its stations under its label")

            for (let i = 0; i < fixCount; ++i) {
                cave.fixStations.addFixStation()
            }
            tryCompare(cave.fixStations, "count", fixCount)
            return { cave: cave, expectedName: trip.scopePrefix + fileStationName }
        }

        function openFixStations(address) {
            RootData.pageSelectionModel.currentPageAddress = address
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage at " + address)
            return RootData.pageView.currentPageItem
        }

        function openCaveFixStations() {
            return openFixStations("Source/Data/Node=" + caveName + "/Fix Stations")
        }

        //! Page row \a fixRow's station cell, once the row is built and shown.
        function stationCell(fixRow) {
            let cell = null
            tryVerify(() => {
                cell = findChild(RootData.pageView.currentPageItem, "stationCell." + fixRow)
                return cell !== null && cell.visible
            }, 5000, "row " + fixRow + "'s station cell is on the page")
            return cell
        }

        //! Opens \a cell's editor and returns the completer it carries.
        function openStationEditor(cell) {
            waitForRendering(rootId)
            cell.openEditor()
            tryVerify(() => rootId.shadowEditor.coreClickInput === cell, 5000,
                      "the station cell opens the shared editor")
            const editor = rootId.shadowEditor.currentEditor
            verify(editor.completer !== undefined,
                   "the station cell opens the station editor, which carries a completer")
            return editor.completer
        }

        function typeText(text) {
            for (let i = 0; i < text.length; ++i) {
                keyClick(text[i])
            }
        }

        function stationNameOf(cave, row) {
            return cave.fixStations.data(cave.fixStations.index(row), FixStationModel.StationNameRole)
        }

        function stationErrorOf(cave, row) {
            const diagnostics = cave.fixStationDiagnostics
            return diagnostics.data(diagnostics.index(row), FixStationDiagnosticsModel.StationErrorRole)
        }

        //! Waits for a solve to place the cave's stations. A name that is in
        //! no station reads Ok until then, so a canary fix is flagged only once
        //! the network holds the cave's stations.
        function waitForTheCaveToSolve(cave) {
            const canaryRow = cave.fixStations.addFixStation("canary")
            tryVerify(() => stationErrorOf(cave, canaryRow) !== "", 20000,
                      "the solve lands and flags a fix on a name no station has")
        }

        //! Types "ent" into \a cell and checks the dropdown offers the scoped
        //! name. Returns the completer.
        function typeEntAndExpectTheScopedName(cell, expectedName) {
            const completer = openStationEditor(cell)
            typeText("ent")
            tryCompare(completer, "typedText", "ent")
            tryVerify(() => completer.matches.indexOf(expectedName) >= 0, 5000,
                      "typing ent offers " + expectedName + ", got [" + completer.matches + "]")
            tryVerify(() => completer.dropdownVisible, 5000, "the dropdown shows")
            verify(rootId.itemsWithText(rootId.shadowEditor.currentEditor, expectedName).length > 0,
                   "the dropdown shows a row reading " + expectedName)
            return completer
        }

        function test_typingPartOfAFileStationOffersItsScopedName() {
            const setup = makeCaveWithAttachedTrip("fix-autocomplete-offer", 1)
            openCaveFixStations()

            typeEntAndExpectTheScopedName(stationCell(0), setup.expectedName)
        }

        function test_tabThenEnterCommitsTheScopedName() {
            const setup = makeCaveWithAttachedTrip("fix-autocomplete-tab", 1)
            openCaveFixStations()

            typeEntAndExpectTheScopedName(stationCell(0), setup.expectedName)
            keyClick(Qt.Key_Tab)
            compare(rootId.shadowEditor.editorText(), setup.expectedName, "Tab fills the field")
            verify(rootId.shadowEditor.coreClickInput === stationCell(0),
                   "Tab leaves the editor open")
            keyClick(Qt.Key_Return)

            tryVerify(() => rootId.shadowEditor.coreClickInput === null, 5000, "Enter closes the editor")
            tryVerify(() => stationNameOf(setup.cave, 0) === setup.expectedName, 5000,
                      "Tab then Enter stores " + setup.expectedName + ", got " + stationNameOf(setup.cave, 0))
            waitForTheCaveToSolve(setup.cave)
            compare(stationErrorOf(setup.cave, 0), "", "the scoped name is a station the solve placed")
        }

        function test_clickingADropdownRowCommitsTheScopedName() {
            const setup = makeCaveWithAttachedTrip("fix-autocomplete-click", 1)
            openCaveFixStations()

            typeEntAndExpectTheScopedName(stationCell(0), setup.expectedName)
            const rowLabel = rootId.itemsWithText(rootId.shadowEditor.currentEditor, setup.expectedName)[0]
            waitForRendering(rootId)
            mouseClick(rowLabel)

            tryVerify(() => rootId.shadowEditor.coreClickInput === null, 5000, "the pick closes the editor")
            tryVerify(() => stationNameOf(setup.cave, 0) === setup.expectedName, 5000,
                      "the pick stores " + setup.expectedName + ", got " + stationNameOf(setup.cave, 0))
        }

        function test_aNameOutsideTheListStillCommitsWithTheAdvisory() {
            const setup = makeCaveWithAttachedTrip("fix-autocomplete-stray", 1)
            openCaveFixStations()

            const completer = openStationEditor(stationCell(0))
            typeText(strayName)
            tryCompare(completer, "typedText", strayName)
            tryVerify(() => completer.outOfScope, 5000, strayName + " is in no station of the cave")
            const advisory = "“" + strayName + "” isn't a station in " + caveName
            compare(completer.advisoryText, advisory)
            verify(rootId.itemsWithText(rootId.shadowEditor.currentEditor, advisory).length > 0,
                   "the dropdown shows the advisory")

            keyClick(Qt.Key_Return)
            tryVerify(() => stationNameOf(setup.cave, 0) === strayName, 5000,
                      "Enter commits " + strayName + " anyway, got " + stationNameOf(setup.cave, 0))
            tryVerify(() => stationErrorOf(setup.cave, 0) !== "", 20000,
                      "the station diagnostic flags " + strayName + " once the solve lands")
        }

        function test_theNarrowStationCellOffersAndCommitsTheScopedName() {
            const setup = makeCaveWithAttachedTrip("fix-autocomplete-narrow", 1)
            const page = openCaveFixStations()
            rootId.width = Theme.breakpointPanelCollapse - narrowWidthMargin
            tryVerify(() => page.isNarrow, 5000, "the page should be in its narrow layout")
            tryVerify(() => findChild(page, "narrowFixRow.0") !== null, 5000,
                      "the row takes its narrow delegate")

            typeEntAndExpectTheScopedName(stationCell(0), setup.expectedName)
            keyClick(Qt.Key_Tab)
            compare(rootId.shadowEditor.editorText(), setup.expectedName, "Tab fills the field")
            keyClick(Qt.Key_Return)
            tryVerify(() => stationNameOf(setup.cave, 0) === setup.expectedName, 5000,
                      "the narrow cell stores " + setup.expectedName + ", got " + stationNameOf(setup.cave, 0))
        }

        // The project page lists every cave's fixes; a cave's row offers the
        // names a fix on that cave takes, since the cave holds the fix.
        function test_theProjectPageOffersTheNamesOfTheRowsCave() {
            const setup = makeCaveWithAttachedTrip("fix-autocomplete-project", 1)
            openFixStations("Source/Data/Fix Stations")

            typeEntAndExpectTheScopedName(stationCell(0), setup.expectedName)
        }

        // Every station cell shares the page's one scope model; moving to
        // another row of the same node rebuilds nothing.
        function test_aSecondCellOnTheSameNodeReusesTheScope() {
            const setup = makeCaveWithAttachedTrip("fix-autocomplete-shared", 2)
            openCaveFixStations()

            const firstCell = stationCell(0)
            openStationEditor(firstCell)
            const scope = rootId.shadowEditor.currentEditor.stationScope
            verify(scope !== null, "the station cell hands the editor a scope")
            firstCell.closeEditor()

            resetSpyId.target = scope
            resetSpyId.clear()
            const secondCell = stationCell(1)
            openStationEditor(secondCell)
            verify(rootId.shadowEditor.currentEditor.stationScope === scope,
                   "the second cell hands the editor the same scope")
            compare(resetSpyId.count, 0, "reopening on the same node rebuilds no rows")
        }
    }
}
