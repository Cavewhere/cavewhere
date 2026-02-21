import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "TripSync"
        when: windowShown

        function openTripPage(caveName, tripName) {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + caveName + "/Trip=" + tripName
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000)
        }

        function verifyStillOnTripPage(tripPageAddress) {
            tryCompare(RootData.pageSelectionModel, "currentPageAddress", tripPageAddress)
            tryVerify(() => {
                return RootData.pageView.currentPageItem !== null
                       && RootData.pageView.currentPageItem.objectName === "tripPage"
            }, 10000)
        }

        function setTripDate(dateInput, newDateText) {
            dateInput.openEditor()
            keyClick(65, Qt.ControlModifier) // Ctrl+A
            for (let i = 0; i < newDateText.length; ++i) {
                keyClick(newDateText.charAt(i))
            }
            keyClick(16777220, 0) // Return
        }

        function loadFixtureAndOpenFirstTrip() {
            let fixture = TestHelper.createLocalSyncFixtureWithLfsServer()
            compare(fixture.errorMessage, "")
            verify(fixture.projectFilePath !== "")
            verify(fixture.remoteRepoPath !== "")
            verify(fixture.lfsEndpoint !== "")
            compare(TestHelper.fileExists(TestHelper.toLocalUrl(fixture.projectFilePath)), true)

            TestHelper.loadProjectFromPath(RootData.project, fixture.projectFilePath)
            tryVerify(() => {
                return RootData.region.caveCount > 0
            }, 10000)
            let cave = null
            let trip = null
            for (let i = 0; i < RootData.region.caveCount; ++i) {
                let candidateCave = RootData.region.cave(i)
                if (candidateCave !== null && candidateCave.rowCount() > 0) {
                    cave = candidateCave
                    trip = candidateCave.trip(0)
                    break
                }
            }
            verify(cave !== null)
            verify(trip !== null)
            let caveName = String(cave.name)
            let tripName = String(trip.name)
            verify(caveName.length > 0)
            verify(tripName.length > 0)
            openTripPage(caveName, tripName)
            return {
                tripPageAddress: "Source/Data/Cave=" + caveName + "/Trip=" + tripName
            }
        }

        function waitForProjectSyncToFinish() {
            tryVerify(() => {
                return RootData.project.syncInProgress === false
            }, 30000)
        }

        function runTripSyncRoundTrip(tripPageAddress, getter, setter, nextValue) {
            let baselineValue = String(getter())
            verify(baselineValue.length > 0)

            let commitA = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitA !== "")

            let syncedValue = String(nextValue(baselineValue))
            setter(syncedValue)
            tryVerify(() => {
                return String(getter()) === syncedValue
            }, 10000)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitB = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitB !== "")
            verify(commitB !== commitA)

            let checkoutError = TestHelper.checkoutProjectRef(RootData.project, commitA, true)
            compare(checkoutError, "")

            verifyStillOnTripPage(tripPageAddress)
            tryVerify(() => {
                return String(getter()) === baselineValue
            }, 10000)

            verify(RootData.project.sync())
            waitForProjectSyncToFinish()

            let commitC = TestHelper.projectHeadCommitOid(RootData.project)
            verify(commitC !== "")
            verify(commitB === commitC)

            verifyStillOnTripPage(tripPageAddress)
            tryVerify(() => {
                return String(getter()) === syncedValue
            }, 10000)
        }

        function test_tripDateSyncAndCheckout() {
            let context = loadFixtureAndOpenFirstTrip()

            let findTripDateInput = function() {
                let dateInput = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->tripDate")
                verify(dateInput !== null)
                return dateInput
            }

            runTripSyncRoundTrip(
                context.tripPageAddress,
                function() {
                    return findTripDateInput().text
                },
                function(newValue) {
                    setTripDate(findTripDateInput(), newValue)
                },
                function(baselineValue) {
                    return baselineValue === "2026-01-15" ? "2026-01-16" : "2026-01-15"
                }
            )
        }
    }
}
