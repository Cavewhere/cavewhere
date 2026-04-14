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
        name: "TripSwitchNotePages"
        when: windowShown

        readonly property string caveAddress: "Source/Data/Cave=Jaws of the Beast"
        readonly property string trip1Address: caveAddress + "/Trip=2019c154_-_party_fault"
        readonly property string trip2Address: caveAddress + "/Trip=Test Trip 2"

        function init() {
            rootId.width = 400

            TestHelper.loadProjectFromZip(RootData.project,
                "://datasets/lidarProjects/jaws of the beast.zip")

            // Navigate to cave page and add a second trip
            RootData.pageSelectionModel.currentPageAddress = caveAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "cavePage"
            })

            let cavePage = RootData.pageView.currentPageItem
            let cave = cavePage.currentCave
            verify(cave !== null, "Cave should exist")
            cave.addTrip()
            let lastIndex = cave.index(cave.rowCount() - 1)
            let trip2 = cave.data(lastIndex, Cave.TripObjectRole)
            trip2.name = "Test Trip 2"
            waitForRendering(rootId)

            // Navigate to Trip1 and add a note
            RootData.pageSelectionModel.currentPageAddress = trip1Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })
            waitForRendering(rootId)

            let trip1 = RootData.pageView.currentPageItem.currentTrip
            verify(trip1 !== null, "Trip1 should not be null")
            tempNotesModel.trip = trip1
            tempNotesModel.addFiles([
                TestHelper.copyToTempDirUrl("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")
            ])
            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            let trip1PageNode = RootData.pageSelectionModel.currentPage
            tryVerify(() => { return trip1PageNode.childPage("Note=PhakeCave.PNG") !== null },
                      5000, "Trip1 note page should be registered")

            // Navigate to Trip2 and add a note
            RootData.pageSelectionModel.currentPageAddress = trip2Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })
            waitForRendering(rootId)

            let trip2Obj = RootData.pageView.currentPageItem.currentTrip
            verify(trip2Obj !== null, "Trip2 should not be null")
            tempNotesModel.trip = trip2Obj
            tempNotesModel.addFiles([
                TestHelper.copyToTempDirUrl("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")
            ])
            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            let trip2PageNode = RootData.pageSelectionModel.currentPage
            tryVerify(() => { return trip2PageNode.childPage("Note=PhakeCave.PNG") !== null },
                      5000, "Trip2 note page should be registered")
        }

        function cleanup() {
            tempNotesModel.trip = null
            rootId.width = 1200
            RootData.pageSelectionModel.currentPageAddress = "View"
            waitForRendering(rootId)
        }

        function test_switchTripNotePagesResolve() {
            let trip1NoteAddress = trip1Address + "/Note=PhakeCave.PNG"
            let trip2NoteAddress = trip2Address + "/Note=PhakeCave.PNG"

            // Navigate to Trip1 and open its note page
            RootData.pageSelectionModel.currentPageAddress = trip1Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })

            RootData.pageSelectionModel.currentPageAddress = trip1NoteAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            })
            compare(RootData.pageSelectionModel.currentPageAddress, trip1NoteAddress,
                    "Should be on Trip1's note page")

            // Switch to Trip2's trip page (Trip1 note page goes into history)
            RootData.pageSelectionModel.currentPageAddress = trip2Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })

            // Regression: switching trips must not break note page resolution
            RootData.pageSelectionModel.currentPageAddress = trip2NoteAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            }, 5000, "Trip2's note page should be reachable after switching from Trip1's note")
            compare(RootData.pageSelectionModel.currentPageAddress, trip2NoteAddress,
                    "Should be on Trip2's note page")
        }

        function test_switchTripWithoutPriorNoteWorks() {
            // Control test: switching trips WITHOUT visiting a note page first should work
            let trip2NoteAddress = trip2Address + "/Note=PhakeCave.PNG"

            // Navigate to Trip1's trip page (no note visit)
            RootData.pageSelectionModel.currentPageAddress = trip1Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })

            // Switch directly to Trip2's trip page
            RootData.pageSelectionModel.currentPageAddress = trip2Address
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "tripPage"
            })

            // Navigate to Trip2's note — should work since no note is stuck in history
            RootData.pageSelectionModel.currentPageAddress = trip2NoteAddress
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                    && RootData.pageView.currentPageItem.objectName === "notePage"
            }, 5000, "Trip2's note should be reachable when no prior note in history")
            compare(RootData.pageSelectionModel.currentPageAddress, trip2NoteAddress)
        }
    }
}
