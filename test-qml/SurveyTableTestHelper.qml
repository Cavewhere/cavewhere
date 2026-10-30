import QtQuick as QQ
import cavewherelib
import QmlTestRecorder

// Getting a test to the survey table: a fresh project, a cave with one trip and
// one chunk, and the trip page open with the editor's ListView reachable. Every
// test that edits survey data starts here, so the navigation, the ObjectFinder
// chain and the station-naming dance live in one place.
QQ.QtObject {
    id: helperId

    required property QQ.Item mainWindow
    //The test's own window, which owns the shared editor this helper closes
    required property MainWindowTest testWindow

    // The state each test starts from: no editor open, no project, sitting on
    // the view page.
    function resetToViewPage(testCase) {
        helperId.testWindow.closeAnyOpenEditor()

        RootData.futureManagerModel.waitForFinished()
        RootData.newProject()
        RootData.pageSelectionModel.currentPageAddress = "View"
        testCase.tryVerify(() => RootData.pageView.currentPageItem !== null
                                 && RootData.pageView.currentPageItem.objectName === "viewPage",
                           5000, "should be on view page at start of test")
    }

    // Leaves the trip page before the project goes away, so the editor isn't
    // holding a chunk that's about to be deleted.
    function leaveSurveyTable() {
        RootData.pageSelectionModel.currentPageAddress = "View"
        RootData.newProject()
    }

    // A cave named `caveName` with one trip named `tripName` and one chunk,
    // sitting on the trip page with the survey table up. Returns the context the
    // other functions here take.
    function openSurveyTable(testCase, caveName, tripName) {
        RootData.region.addCave()
        const cave = RootData.region.cave(RootData.region.caveCount - 1)
        cave.name = caveName
        cave.addTrip()
        const trip = cave.trip(0)
        trip.name = tripName
        trip.addNewChunk()

        return openSurveyTableForTrip(testCase, "Source/Data/Node=" + cave.name + "/Trip=" + trip.name, trip)
    }

    // Opens the survey table on `trip`, whose first chunk exists, at the page
    // address `tripAddress`. Returns the same context openSurveyTable does.
    function openSurveyTableForTrip(testCase, tripAddress, trip) {
        RootData.pageSelectionModel.currentPageAddress = tripAddress
        testCase.tryVerify(() => RootData.pageView.currentPageItem !== null
                                 && RootData.pageView.currentPageItem.objectName === "tripPage",
                           5000, "should land on tripPage")

        let view = null
        testCase.tryVerify(() => {
            view = ObjectFinder.findObjectByChain(
                        helperId.mainWindow, "rootId->tripPage->surveyEditor->view")
            return view !== null && view.model !== null && view.model.trip === trip
        }, 5000, "survey editor view should show the new trip")

        return {
            cave: trip.parentCave,
            trip: trip,
            chunk: trip.chunk(0),
            view: view,
            editorModel: view.model
        }
    }

    // The editor the table sits in, for the things it owns rather than the rows
    // — popups, and the challenge that guards deleting a whole splay cluster.
    function surveyEditor() {
        return ObjectFinder.findObjectByChain(
                    helperId.mainWindow, "rootId->tripPage->surveyEditor")
    }

    function stationRow(context, indexInChunk) {
        return context.editorModel.toModelRow(
                    context.editorModel.rowIndex(context.chunk,
                                                 indexInChunk,
                                                 SurveyEditorRowIndex.StationRow))
    }

    function setStationName(testCase, context, indexInChunk, name) {
        const row = stationRow(context, indexInChunk)
        testCase.verify(row >= 0, "station " + indexInChunk + " should have a model row")
        context.editorModel.setDataAt(
                    context.editorModel.cellIndex(row, SurveyChunk.StationNameRole), name)
        testCase.tryVerify(() => context.chunk.data(SurveyChunk.StationNameRole, indexInChunk) === name,
                           5000, "station " + indexInChunk + " should be named " + name)
    }

    // The delegate for `row`, scrolled into view so the ListView has built it.
    function rowItem(testCase, context, row) {
        context.view.positionViewAtIndex(row, QQ.ListView.Contain)

        let item = null
        testCase.tryVerify(() => {
            item = context.view.itemAtIndex(row)
            return item !== null
        }, 5000, "row " + row + " should have a delegate")
        return item
    }

    function cell(testCase, context, row, cellRole) {
        const item = rowItem(testCase, context, row)
        let found = null
        testCase.tryVerify(() => {
            found = testCase.findChild(item, "dataBox." + row + "." + cellRole)
            return found !== null
        }, 5000, "row " + row + " should have cell " + cellRole)
        return found
    }

    // Right-clicks the cell and returns its menu once the loader has built it
    function openMenu(testCase, context, row, cellRole, loaderName) {
        const cellItem = cell(testCase, context, row, cellRole)
        testCase.mouseClick(cellItem, cellItem.width / 2, cellItem.height / 2, Qt.RightButton)

        const loaderPath = "rootId->tripPage->surveyEditor->view->dataBox." + row + "." + cellRole
                + "->" + loaderName
        let loader = null
        testCase.tryVerify(() => {
            loader = ObjectFinder.findObjectByChain(helperId.mainWindow, loaderPath)
            return loader !== null && loader.item !== null
        }, 5000, loaderName + " should build its menu")
        testCase.tryVerify(() => loader.item.opened, 5000, loaderName + " should be open")
        return loader.item
    }

    // Opens a submenu of `menu`, which fires the submenu's aboutToShow
    function openSubMenu(testCase, menu, objectName) {
        const subMenu = testCase.findChild(menu, objectName)
        testCase.verify(subMenu !== null, "the menu should offer " + objectName)
        subMenu.open()
        testCase.tryVerify(() => subMenu.opened, 5000, objectName + " should be open")
        return subMenu
    }

    function closeMenu(testCase, menu) {
        menu.close()
        testCase.tryVerify(() => !menu.visible, 5000, "the menu should close")
    }

    function menuItem(testCase, menu, objectName, text) {
        const item = testCase.findChild(menu, objectName)
        testCase.verify(item !== null, "the menu should offer " + objectName)
        testCase.compare(item.text, text)
        return item
    }
}
