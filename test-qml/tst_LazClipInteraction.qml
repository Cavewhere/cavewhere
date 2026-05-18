import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// State-machine test for LazClipInteraction. Drives the interaction
// directly (no camera) and uses addWorldPoint to bypass screen→world
// unprojection, since the state transitions are what we're verifying.
TestCase {
    name: "LazClipInteraction"

    LazClipInteraction {
        id: clipperId
    }

    SignalSpy {
        id: clipFailedSpy
        target: clipperId
        signalName: "clipFailed"
    }

    function init() {
        clipperId.cancel()
        clipFailedSpy.clear()
    }

    function test_startsIdle() {
        compare(clipperId.state, LazClipInteraction.Idle)
        compare(clipperId.pointCount, 0)
        compare(clipperId.canCommit, false)
    }

    function test_drawAndCloseStateMachine() {
        clipperId.addWorldPoint(Qt.point(0, 0))
        compare(clipperId.state, LazClipInteraction.Drawing)
        compare(clipperId.pointCount, 1)
        compare(clipperId.canCommit, false)

        clipperId.addWorldPoint(Qt.point(10, 0))
        clipperId.addWorldPoint(Qt.point(10, 10))
        compare(clipperId.pointCount, 3)

        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)
        compare(clipperId.canCommit, true)
    }

    function test_closeRejectsFewerThanThreeVertices() {
        clipperId.addWorldPoint(Qt.point(0, 0))
        clipperId.addWorldPoint(Qt.point(10, 0))
        compare(clipperId.pointCount, 2)

        clipperId.closePolygon()
        // Stays in Drawing; surfaces an error message instead.
        compare(clipperId.state, LazClipInteraction.Drawing)
        verify(clipperId.errorMessage.length > 0)
    }

    function test_cancelResetsState() {
        clipperId.addWorldPoint(Qt.point(0, 0))
        clipperId.addWorldPoint(Qt.point(10, 0))
        clipperId.addWorldPoint(Qt.point(10, 10))
        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)

        clipperId.cancel()
        compare(clipperId.state, LazClipInteraction.Idle)
        compare(clipperId.pointCount, 0)
        compare(clipperId.errorMessage, "")
    }

    function test_addPointBlockedAfterClose() {
        clipperId.addWorldPoint(Qt.point(0, 0))
        clipperId.addWorldPoint(Qt.point(10, 0))
        clipperId.addWorldPoint(Qt.point(10, 10))
        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)

        // Trying to add another vertex after close is a no-op — state and
        // count both unchanged.
        clipperId.addWorldPoint(Qt.point(5, 5))
        compare(clipperId.state, LazClipInteraction.Closed)
        compare(clipperId.pointCount, 3)
    }

    function test_commitFailsWithNoSceneNode() {
        clipperId.addWorldPoint(Qt.point(0, 0))
        clipperId.addWorldPoint(Qt.point(10, 0))
        clipperId.addWorldPoint(Qt.point(10, 10))
        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)

        // No region / no scene node bound — commit must surface clipFailed
        // synchronously (it never gets as far as the async operation).
        clipperId.commit(LazClipInteraction.Keep)
        compare(clipFailedSpy.count, 1)
    }
}
