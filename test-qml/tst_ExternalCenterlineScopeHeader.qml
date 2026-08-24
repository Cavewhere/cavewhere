import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib

// ExternalCenterlineScopeHeader on its own (plans/EXTERNAL_FILE_PHASE3.html
// P3.9): the trip windows a block of its cave's survey file, so the header
// names the cave, shows the prefix that selects the block, and offers the
// one destructive verb the trip still answers to.
MainWindowTest {
    id: rootId

    property Trip trip: null

    ExternalCenterlineScopeHeader {
        id: headerId
        width: 500
        trip: rootId.trip
    }

    ExternalCenterlineTestCase {
        name: "ExternalCenterlineScopeHeader"
        when: windowShown

        function init() {
            rootId.closeAnyOpenEditor()
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
            rootId.trip = null
        }

        function cleanup() {
            rootId.closeAnyOpenEditor()
            rootId.trip = null
            RootData.newProject()
        }

        // The attach creates one Scope trip per block with stations; each
        // owns no file and carries the prefix that windows its block.
        function bindFirstScopeTrip(projectBaseName) {
            const cave = makeSavedCaveAttach(
                           projectBaseName,
                           "external-centerlines/survex_blocks.svx")
            verify(cave.rowCount() > 0, "the attach created Scope trips")

            rootId.trip = cave.trip(0)
            verify(rootId.trip.stationPrefix.length > 0,
                   "a Scope trip carries the prefix that windows its block")
            waitForRendering(headerId)
            return cave
        }

        function test_partOfNamesTheCaveAndLinksToItsPage() {
            const cave = bindFirstScopeTrip("scope-header-partof")

            const link = findChild(headerId, "parentCaveLink")
            verify(link !== null, "parentCaveLink must exist")
            tryCompare(link, "text", cave.name,
                       5000, "the link names the cave the trip belongs to")

            mouseClick(link)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "clicking Part of lands on the cave page")
            waitForRendering(rootId)
        }

        function test_changePrefixEditsTheTripsPrefix() {
            bindFirstScopeTrip("scope-header-prefix")
            const trip = rootId.trip

            const prefixInput = findChild(headerId, "prefixInput")
            verify(prefixInput !== null, "prefixInput must exist")
            tryCompare(prefixInput, "text", trip.stationPrefix,
                       5000, "the field shows the trip's prefix")

            // Change prefix… opens the same editor the field itself opens.
            const changeButton = findChild(headerId, "changePrefixButton")
            verify(changeButton !== null, "changePrefixButton must exist")
            mouseClick(changeButton)
            tryVerify(() => rootId.shadowEditor.coreClickInput === prefixInput,
                      5000, "Change prefix… opens the field's editor")
            rootId.closeAnyOpenEditor()

            prefixInput.finishedEditting("newprefix")
            tryCompare(trip, "stationPrefix", "newprefix",
                       5000, "the committed text lands on the trip")
            tryCompare(prefixInput, "text", "newprefix",
                       5000, "the field shows what the trip now carries")
        }

        function test_removeTripRemovesItFromTheCave() {
            const cave = bindFirstScopeTrip("scope-header-remove")
            const tripName = rootId.trip.name
            const rowsBefore = cave.rowCount()

            const removeButton = findChild(headerId, "removeTripButton")
            verify(removeButton !== null, "removeTripButton must exist")
            mouseClick(removeButton)

            const challenge = findChild(headerId, "removeTripChallenge")
            verify(challenge !== null, "removeTripChallenge must exist")
            tryVerify(() => challenge.visible, 5000, "the prompt opens")
            verify(challenge.message.indexOf(tripName) >= 0,
                   "the prompt names the trip; got: " + challenge.message)

            const confirm = findChild(challenge, "removeButton")
            verify(confirm !== null, "the prompt's confirm button must exist")

            // The prompt has to be drawn where it now stands before it can
            // be clicked: a box the scene graph culled while it was hidden
            // stays unhittable until the next frame.
            waitForRendering(headerId)

            // Nothing may touch the trip after this click: removeTrip with
            // no undo stack destroys it on the spot.
            mouseClick(confirm)

            tryVerify(() => cave.rowCount() === rowsBefore - 1,
                      10000, "confirming removes the trip from the cave")
            rootId.trip = null
        }
    }
}
