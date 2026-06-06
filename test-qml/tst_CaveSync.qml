import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder
import "SyncTestHelper.js" as SyncTestHelper

MainWindowTest {
    id: rootId

    TestCase {
        id: testCaseId
        name: "CaveSync"
        when: windowShown

        function tryVerifyWithDiagnostics(predicate, timeoutMs, label, onPending) {
            SyncTestHelper.tryVerifyWithDiagnostics(testCaseId, predicate, timeoutMs, label, onPending)
        }

        function loadFixtureAndOpenFirstTrip() {
            return SyncTestHelper.loadFixtureAndOpenFirstTrip(testCaseId, RootData, TestHelper)
        }

        function caveByName(caveName) {
            for (let i = 0; i < RootData.region.caveCount; ++i) {
                let cave = RootData.region.cave(i)
                if (cave !== null && String(cave.name) === String(caveName)) {
                    return cave
                }
            }
            return null
        }

        function cavePageAddress(caveName) {
            return "Source/Data/Cave=" + caveName
        }

        function tripNamesFromCave(cave) {
            let names = []
            for (let i = 0; i < cave.rowCount(); ++i) {
                names.push(String(cave.trip(i).name))
            }
            names.sort()
            return names
        }

        function tripNamesFromTableView(tableView) {
            let names = []
            let model = tableView.model
            for (let row = 0; row < model.count; ++row) {
                let idx = model.index(row, 0)
                names.push(String(model.data(idx, CavePageModel.TripNameRole)))
            }
            names.sort()
            return names
        }

        // Regression test for issue #429: when another user adds a trip and the
        // local user syncs while on the cave page, the new trip must appear in
        // the cave-page trip ListView. The bug is that the tripTableView model
        // does not refresh even though the underlying cave model has the trip.
        function test_addTripSyncAndCheckoutUpdatesCavePageList() {
            let context = loadFixtureAndOpenFirstTrip()
            let caveName = context.caveName

            verify(caveByName(caveName) !== null)

            SyncTestHelper.openCavePage(testCaseId, RootData, caveName)
            waitForRendering(rootId)

            let addedTripName = "SYNC-NEW-TRIP-A"

            let restoreCavePage = function(stage) {
                let address = cavePageAddress(caveName)
                tryVerifyWithDiagnostics(() => {
                    if (RootData.pageSelectionModel.currentPageAddress !== address) {
                        RootData.pageSelectionModel.currentPageAddress = address
                    }
                    return RootData.pageView.currentPageItem !== null
                           && RootData.pageSelectionModel.currentPageAddress === address
                           && RootData.pageView.currentPageItem.objectName === "cavePage"
                }, 20000, "restoreCavePage (" + stage + ")")
                waitForRendering(rootId)
            }

            let snapshotCaveState = function() {
                let c = caveByName(caveName)
                verify(c !== null)
                return JSON.stringify({
                    tripCount: c.rowCount(),
                    tripNames: tripNamesFromCave(c)
                })
            }

            let snapshotUiState = function() {
                let cavePage = RootData.pageView.currentPageItem
                verify(cavePage !== null)
                let tableView = findChild(cavePage, "tripTableView")
                verify(tableView !== null)
                return JSON.stringify({
                    tripCount: tableView.model.count,
                    tripNames: tripNamesFromTableView(tableView)
                })
            }

            SyncTestHelper.runProjectSyncRoundTrip(testCaseId, RootData, TestHelper, {
                tripPageAddress: context.tripPageAddress,
                restorePage: restoreCavePage,
                getter: function() {
                    return snapshotCaveState()
                },
                setter: function(expectedStateJson) {
                    let c = caveByName(caveName)
                    verify(c !== null)
                    c.addTrip()
                    let newTrip = c.trip(c.rowCount() - 1)
                    verify(newTrip !== null)
                    newTrip.name = addedTripName

                    tryVerifyWithDiagnostics(() => {
                        return snapshotCaveState() === expectedStateJson
                    }, 3000, "verify new trip added locally")
                },
                nextValue: function(baselineStateJson) {
                    let state = JSON.parse(baselineStateJson)
                    state.tripCount += 1
                    state.tripNames.push(addedTripName)
                    state.tripNames.sort()
                    return JSON.stringify(state)
                },
                uiGetter: function() {
                    return snapshotUiState()
                },
                uiExpectedFromValue: function(stateJson) {
                    return stateJson
                }
            })
        }
    }
}
