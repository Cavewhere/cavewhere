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
        name: "NarrowToolbar"
        when: windowShown

        readonly property string tripAddress: "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

        function init() {
            rootId.width = 400
            waitForRendering(rootId)

            TestHelper.loadProjectFromZip(RootData.project, TestHelper.testcasesDatasetPath("lidarProjects/jaws of the beast.zip"))
            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })
            waitForRendering(rootId)

            let tripPageItem = RootData.pageView.currentPageItem
            tempNotesModel.trip = tripPageItem.currentTrip
            verify(tempNotesModel.trip !== null)

            let image1 = TestHelper.toLocalUrl(TestHelper.testcasesDatasetPath("test_cwTextureUploadTask/PhakeCave.PNG"))
            let image2 = TestHelper.toLocalUrl(TestHelper.testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"))
            tempNotesModel.addFiles([image1, image2])

            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            // Navigate to the first note (narrow -> NotePage)
            let tripPage = RootData.pageSelectionModel.currentPage
            tryVerify(() => { return tripPage.childPage("Note=PhakeCave.PNG") !== null })
            RootData.pageSelectionModel.currentPageAddress = tripAddress + "/Note=PhakeCave.PNG"
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            })
            waitForRendering(rootId)

            // Wait for the gallery to have notes and the toolbar to be ready
            let noteGallery = null
            tryVerify(() => {
                noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
                return noteGallery !== null && noteGallery.noteCount > 0
            }, 10000, "noteGallery should have notes")

            tryVerify(() => {
                let tb = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar")
                return tb !== null && tb.visible && tb.height > 0
            }, 5000, "narrowToolbar should be ready after init")

        }

        function cleanup() {
            tempNotesModel.trip = null
            rootId.width = 1200
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function test_toolbarVisibleInNarrowMode() {
            let toolbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar")
            verify(toolbar !== null, "narrowToolbar should exist")
            verify(toolbar.visible, "narrowToolbar should be visible in narrow mode")
            verify(toolbar.height > 0, "narrowToolbar should have positive height")
        }

        function test_prevNextNavigation() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
            compare(noteGallery.currentNoteIndex, 0, "Should start on first note")

            let nextButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->nextButton")
            verify(nextButton.enabled, "Next button should be enabled with 2 notes")
            mouseClick(nextButton)

            tryVerify(() => { return noteGallery.currentNoteIndex === 1 })

            let prevButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->prevButton")
            verify(prevButton.enabled, "Prev button should be enabled on second note")
            mouseClick(prevButton)

            tryVerify(() => { return noteGallery.currentNoteIndex === 0 })

            verify(!prevButton.enabled, "Prev button should be disabled on first note")
        }

        function test_editEntersCarpetMode() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
            let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
            mouseClick(editButton)

            wait(200)

            tryVerify(() => { return noteGallery.mode === "CARPET" })
            verify(editButton.checked, "Edit button should show checked in carpet mode")
        }

        function test_subToolsChangeState() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
            let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
            mouseClick(editButton)
            wait(200)
            tryVerify(() => { return noteGallery.mode === "CARPET" })

            let selectButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->selectButton")
            mouseClick(selectButton)
            tryVerify(() => { return noteGallery.state === "SELECT" })
            verify(selectButton.checked, "Select button should show checked")

            let addScrapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addScrapButton")
            verify(addScrapButton.visible, "Scrap button visible for photo note")
            mouseClick(addScrapButton)
            tryVerify(() => { return noteGallery.state === "ADD-SCRAP" })
            verify(addScrapButton.checked, "Scrap button should show checked")

            let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addStationButton")
            mouseClick(addStationButton)
            tryVerify(() => { return noteGallery.state === "ADD-STATION" })

            let addLeadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addLeadButton")
            verify(addLeadButton.visible, "Lead button visible for photo note")
            mouseClick(addLeadButton)
            tryVerify(() => { return noteGallery.state === "ADD-LEAD" })
        }

        function test_editTogglesCarpetMode() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
            let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
            mouseClick(editButton)
            wait(200)
            tryVerify(() => { return noteGallery.mode === "CARPET" })

            mouseClick(editButton)
            tryVerify(() => { return noteGallery.mode !== "CARPET" })
            verify(!editButton.checked, "Edit button should not be checked after exiting carpet mode")
        }

        function test_lidarNoteHidesPhotoOnlyButtons() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")

            let nextButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->nextButton")
            mouseClick(nextButton)
            tryVerify(() => { return noteGallery.currentNoteIndex === 1 })
            waitForRendering(rootId)

            let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
            mouseClick(editButton)
            wait(200)
            tryVerify(() => { return noteGallery.mode === "CARPET" })

            let addScrapButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addScrapButton")
            verify(!addScrapButton.visible, "Scrap button should be hidden for LiDAR note")

            let addLeadButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addLeadButton")
            verify(!addLeadButton.visible, "Lead button should be hidden for LiDAR note")

            let rotateButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->rotateButton")
            verify(!rotateButton.visible, "Rotate button should be hidden for LiDAR note")

            let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addStationButton")
            verify(addStationButton.visible, "Station button should be visible for LiDAR note")

            mouseClick(editButton)
            tryVerify(() => { return noteGallery.mode !== "CARPET" })
        }

        function test_deleteButtonHiddenWhenNothingSelected() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
            let editButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->editButton")
            mouseClick(editButton)
            wait(200)
            tryVerify(() => { return noteGallery.mode === "CARPET" })

            let selectButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->selectButton")
            mouseClick(selectButton)
            tryVerify(() => { return noteGallery.state === "SELECT" })

            let deleteButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->deleteSelectedButton")
            verify(deleteButton !== null, "deleteSelectedButton should exist")
            verify(!deleteButton.visible, "delete button should be hidden with nothing selected")

            // Delete is a SELECT-mode action — should stay hidden in ADD-* sub-modes.
            let addStationButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->addStationButton")
            mouseClick(addStationButton)
            tryVerify(() => { return noteGallery.state === "ADD-STATION" })
            verify(!deleteButton.visible, "delete button should be hidden in ADD-STATION")
        }

        function test_overlaysStartCollapsed() {
            let noteRes = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->noteArea->noteResolution")
            verify(noteRes.collapsed, "NoteResolution should start collapsed on NotePage")

            let noteTransformEditor = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->noteArea->noteTransformEditor")
            verify(noteTransformEditor.collapsed, "NoteTransformEditor should start collapsed on NotePage")
        }

        function test_notePickerDrawer() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
            compare(noteGallery.currentNoteIndex, 0, "Should start on first note")

            let counterLabel = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->counterLabel")
            mouseClick(counterLabel)

            // Drawer is a Popup — access via the property alias on noteGallery
            let drawer = noteGallery._notePickerDrawer
            verify(drawer !== null, "Drawer should exist")
            tryVerify(() => { return drawer.position > 0 }, 1000, "Drawer should be opening")
            tryVerify(() => { return drawer.position === 1.0 }, 2000, "Drawer should be fully open")

            // Close drawer
            drawer.close()
            tryVerify(() => { return drawer.position === 0 }, 2000)
        }

        // In narrow mode the main gallery panel is collapsed to width 0, but
        // the note picker drawer reuses the same delegate and must still
        // display note thumbnails at a visible size.
        function test_notePickerDrawerShowsImages() {
            let noteGallery = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery")
            let counterLabel = ObjectFinder.findObjectByChain(mainWindow, "rootId->notePage->noteGallery->narrowToolbar->counterLabel")
            mouseClick(counterLabel)

            let drawer = noteGallery._notePickerDrawer
            tryVerify(() => { return drawer.position === 1.0 }, 2000, "Drawer should fully open")

            let list = noteGallery._notePickerList
            verify(list !== null, "notePickerList should exist")
            compare(list.count, 2, "Drawer list should contain both notes")

            tryVerify(() => { return list.itemAtIndex(0) !== null }, 2000, "First delegate should materialize")
            let delegate0 = list.itemAtIndex(0)
            verify(delegate0.width > 0, "Drawer delegate should have non-zero width")
            verify(delegate0.height > 0, "Drawer delegate should have non-zero height")

            drawer.close()
            tryVerify(() => { return drawer.position === 0 }, 2000)
        }
    }
}
