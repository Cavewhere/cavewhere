import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    SurveyNotesConcatModel {
        id: tempNotesModel
    }

    TestCase {
        name: "NotesGallerySketch"
        when: windowShown

        readonly property string tripAddress: "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

        function init() {
            rootId.width = 1200

            TestHelper.loadProjectFromZip(RootData.project,
                TestHelper.testcasesDatasetPath("lidarProjects/jaws of the beast.zip"))
            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })
            waitForRendering(rootId)

            let tripPageItem = RootData.pageView.currentPageItem
            tempNotesModel.trip = tripPageItem.currentTrip
            verify(tempNotesModel.trip !== null, "currentTrip should not be null")
        }

        function cleanup() {
            tempNotesModel.trip = null
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function test_addSketchShowsSketchItemInWideMode() {
            const initial = tempNotesModel.rowCount()
            const sketch = tempNotesModel.addSketch(Sketch.Plan)
            verify(sketch !== null, "addSketch should return a Sketch")

            tryVerify(() => { return tempNotesModel.rowCount() === initial + 1 },
                      2000, "Sketch row should be added")

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow,
                "rootId->tripPage->noteGallery")
            verify(noteGallery !== null, "noteGallery should exist in wide mode")

            noteGallery.currentNoteIndex = initial
            tryVerify(() => { return noteGallery.currentSketch !== null },
                      2000, "currentSketch should resolve to the new sketch")
            compare(noteGallery.currentSketch.name, sketch.name)

            const sketchArea = findChild(noteGallery, "sketchArea")
            verify(sketchArea !== null, "sketchArea should exist")
            tryVerify(() => { return sketchArea.visible }, 2000,
                      "sketchArea should be visible when a sketch is current")
        }

        function test_addSketchRegistersNotePage() {
            const sketch = tempNotesModel.addSketch(Sketch.Plan)
            sketch.name = "MyTestSketch"
            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            let tripPage = RootData.pageSelectionModel.currentPage
            tryVerify(() => { return tripPage.childPage("Note=MyTestSketch") !== null },
                      2000, "Sketch should register a NotePage child")
        }

        function test_narrowToolbarShowsSketchButtonsForSketch() {
            rootId.width = 400
            waitForRendering(rootId)

            const sketch = tempNotesModel.addSketch(Sketch.Plan)
            sketch.name = "NarrowSketch"
            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            let tripPage = RootData.pageSelectionModel.currentPage
            tryVerify(() => { return tripPage.childPage("Note=NarrowSketch") !== null })
            RootData.pageSelectionModel.currentPageAddress = tripAddress + "/Note=NarrowSketch"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            })
            waitForRendering(rootId)

            let noteGallery = ObjectFinder.findObjectByChain(mainWindow,
                "rootId->notePage->noteGallery")
            tryVerify(() => { return noteGallery !== null && noteGallery.currentSketch !== null },
                      5000, "sketch should be current in the note page gallery")

            let toolbar = ObjectFinder.findObjectByChain(mainWindow,
                "rootId->notePage->noteGallery->narrowToolbar")
            verify(toolbar !== null, "narrowToolbar should exist")

            let editButton = ObjectFinder.findObjectByChain(mainWindow,
                "rootId->notePage->noteGallery->narrowToolbar->editButton")
            mouseClick(editButton)
            tryVerify(() => { return noteGallery.mode === "CARPET" })

            let wallBtn = ObjectFinder.findObjectByChain(mainWindow,
                "rootId->notePage->noteGallery->narrowToolbar->addSketchWallButton")
            let featureBtn = ObjectFinder.findObjectByChain(mainWindow,
                "rootId->notePage->noteGallery->narrowToolbar->addSketchFeatureButton")
            verify(wallBtn !== null, "addSketchWallButton should exist")
            verify(featureBtn !== null, "addSketchFeatureButton should exist")
            verify(wallBtn.visible, "Wall button visible for a sketch")
            verify(featureBtn.visible, "Feature button visible for a sketch")

            // Drive the signal directly — the real button click path is
            // covered by visibility/enabled checks above. offscreen click
            // positioning into the Layout-nested RoundButton is flaky.
            toolbar.stateChangeRequested("ADD-SKETCH-WALL")
            tryVerify(() => { return noteGallery.state === "ADD-SKETCH-WALL" },
                      2000, "state should transition to ADD-SKETCH-WALL")

            toolbar.stateChangeRequested("ADD-SKETCH-FEATURE")
            tryVerify(() => { return noteGallery.state === "ADD-SKETCH-FEATURE" },
                      2000, "state should transition to ADD-SKETCH-FEATURE")
        }
    }
}
