import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    // Temporary model to add notes in narrow mode where the gallery is inactive
    SurveyNotesConcatModel {
        id: tempNotesModel
    }

    TestCase {
        name: "NotePageNavigation"
        when: windowShown

        readonly property string tripAddress: "Source/Data/Cave=Jaws of the Beast/Trip=2019c154_-_party_fault"

        function init() {
            // Start narrow so the NotesGallery Loader is inactive —
            // avoids QQmlComponent delegate creation conflict between
            // the NotesGallery ListView and the note page Instantiator
            rootId.width = 400

            TestHelper.loadProjectFromZip(RootData.project, "://datasets/lidarProjects/jaws of the beast.zip")
            RootData.pageSelectionModel.currentPageAddress = tripAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })
            waitForRendering(rootId)

            // Get the trip from the TripPage and add notes via our temp model
            let tripPageItem = RootData.pageView.currentPageItem
            let trip = tripPageItem.currentTrip
            verify(trip !== null, "currentTrip should not be null")

            tempNotesModel.trip = trip

            let image1 = TestHelper.copyToTempDirUrl("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")
            let image2 = TestHelper.copyToTempDirUrl("://datasets/lidarProjects/9_15_2025 3.glb")
            tempNotesModel.addFiles([image1, image2])

            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            // Verify note pages are registered (names include file extensions)
            let tripPage = RootData.pageSelectionModel.currentPage
            tryVerify(() => { return tripPage.childPage("Note=PhakeCave.PNG") !== null })
            tryVerify(() => { return tripPage.childPage("Note=9_15_2025 3.glb 2") !== null })
        }

        function cleanup() {
            tempNotesModel.trip = null
            rootId.width = 1200
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function test_navigateToSecondNoteByAddress() {
            let note1Address = tripAddress + "/Note=PhakeCave.PNG"
            let note2Address = tripAddress + "/Note=9_15_2025 3.glb 2"

            // Navigate to the first note
            RootData.pageSelectionModel.currentPageAddress = note1Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            })
            compare(RootData.pageSelectionModel.currentPageAddress, note1Address)
            let notePage = RootData.pageView.currentPageItem
            compare(notePage.currentNoteIndex, 0, "First note should have currentNoteIndex 0")

            // Navigate to the second note — verify currentNoteIndex updates
            RootData.pageSelectionModel.currentPageAddress = note2Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            })
            compare(RootData.pageSelectionModel.currentPageAddress, note2Address)
            compare(notePage.currentNoteIndex, 1, "Second note should have currentNoteIndex 1")
        }
    }
}
