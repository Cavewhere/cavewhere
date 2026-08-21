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

    // The state each test starts from: no editor open, no project, sitting on
    // the view page.
    function resetToViewPage(testCase) {
        if (GlobalShadowTextInput.coreClickInput !== null) {
            GlobalShadowTextInput.coreClickInput.closeEditor()
        }
        GlobalShadowTextInput.enabled = false

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

        RootData.pageSelectionModel.currentPageAddress =
                "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name
        testCase.tryVerify(() => RootData.pageView.currentPageItem !== null
                                 && RootData.pageView.currentPageItem.objectName === "tripPage",
                           5000, "should land on tripPage")

        let view = null
        testCase.tryVerify(() => {
            view = ObjectFinder.findObjectByChain(
                        helperId.mainWindow, "rootId->tripPage->surveyEditor->view")
            return view !== null && view.model !== null
        }, 5000, "survey editor view should be reachable")

        return {
            cave: cave,
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
}
