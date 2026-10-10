import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib

// ExternalCenterlineScopeHeader on its own (plans/EXTERNAL_FILE_PHASE3.html
// P3.9): the trip is one unit of its source root's survey file, so the
// header names the node it belongs to and offers the one destructive verb
// the trip still answers to.
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

        // survex_blocks.svx attaches as a SurvexFile root whose first block,
        // doghill, is a derived node holding its own shots as one derived
        // trip. Binds the header to that trip and returns its node.
        function bindFirstDerivedTrip(projectBaseName) {
            const sourceRoot = makeSavedCaveAttach(
                                 projectBaseName,
                                 "external-centerlines/survex_blocks.svx")
            const doghill = childNamed(sourceRoot, "doghill")
            verify(doghill !== null, "the attach derived the doghill block")
            compare(doghill.rowCount(), 1, "doghill's own shots are one derived trip")

            rootId.trip = doghill.trip(0)
            verify(rootId.trip.sourcePath.length > 0,
                   "a derived trip names its place in the file")
            waitForRendering(headerId)
            return doghill
        }

        function test_partOfNamesTheParentNodeAndLinksToItsPage() {
            const node = bindFirstDerivedTrip("scope-header-partof")

            const link = findChild(headerId, "parentCaveLink")
            verify(link !== null, "parentCaveLink must exist")
            tryCompare(link, "text", node.name,
                       5000, "the link names the node the trip belongs to")

            mouseClick(link)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavePage"
                            && RootData.pageView.currentPageItem.currentNode === node,
                      10000, "clicking Part of lands on the node's page")
            waitForRendering(rootId)
        }

        function test_removeTripRemovesItFromItsNode() {
            const node = bindFirstDerivedTrip("scope-header-remove")
            const tripName = rootId.trip.name
            const rowsBefore = node.rowCount()

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

            tryVerify(() => node.rowCount() === rowsBefore - 1,
                      10000, "confirming removes the trip from its node")
            rootId.trip = null
        }
    }
}
