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

    SignalSpy {
        id: deactivatedSpy
        target: clipperId
        signalName: "deactivated"
    }

    function init() {
        clipperId.cancel()
        clipFailedSpy.clear()
        deactivatedSpy.clear()
    }

    function test_startsIdle() {
        compare(clipperId.state, LazClipInteraction.Idle)
        compare(clipperId.pointCount, 0)
        compare(clipperId.canCommit, false)
    }

    function test_drawAndCloseStateMachine() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        compare(clipperId.state, LazClipInteraction.Drawing)
        compare(clipperId.pointCount, 1)
        compare(clipperId.canCommit, false)

        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 10, 0))
        compare(clipperId.pointCount, 3)

        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)
        compare(clipperId.canCommit, true)
    }

    function test_closeRejectsFewerThanThreeVertices() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        compare(clipperId.pointCount, 2)

        clipperId.closePolygon()
        // Stays in Drawing; surfaces an error message instead.
        compare(clipperId.state, LazClipInteraction.Drawing)
        verify(clipperId.errorMessage.length > 0)
    }

    function test_cancelResetsState() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 10, 0))
        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)

        clipperId.cancel()
        compare(clipperId.state, LazClipInteraction.Idle)
        compare(clipperId.pointCount, 0)
        compare(clipperId.errorMessage, "")
    }

    function test_addPointBlockedAfterClose() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 10, 0))
        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)

        // Trying to add another vertex after close is a no-op — state and
        // count both unchanged.
        clipperId.addWorldPoint(Qt.vector3d(5, 5, 0))
        compare(clipperId.state, LazClipInteraction.Closed)
        compare(clipperId.pointCount, 3)
    }

    // deactivate() is what the view's Escape handler calls. It emits the
    // deactivated signal (InteractionManager uses this to switch back to
    // the default interaction), and the wired-up onDeactivated slot then
    // calls cancel() — so an in-progress polygon and any error must be
    // cleared as a side-effect of deactivation alone.
    function test_deactivateResetsStateAndEmitsSignal() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        compare(clipperId.state, LazClipInteraction.Drawing)
        compare(clipperId.pointCount, 2)

        clipperId.deactivate()

        compare(deactivatedSpy.count, 1)
        compare(clipperId.state, LazClipInteraction.Idle)
        compare(clipperId.pointCount, 0)
        compare(clipperId.errorMessage, "")
    }

    function test_deactivateClearsClosedPolygon() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 10, 0))
        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)

        clipperId.deactivate()
        compare(deactivatedSpy.count, 1)
        compare(clipperId.state, LazClipInteraction.Idle)
        compare(clipperId.pointCount, 0)
    }

    function test_commitFailsWithNoSceneNode() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 10, 0))
        clipperId.closePolygon()
        compare(clipperId.state, LazClipInteraction.Closed)

        // No region / no scene node bound — commit must surface clipFailed
        // synchronously (it never gets as far as the async operation).
        clipperId.commit(LazClipInteraction.Keep)
        compare(clipFailedSpy.count, 1)
    }

    // commit() never deactivates on its own — the QML Crop/Erase buttons
    // do that. So calling commit() leaves the interaction "active"
    // (deactivatedSpy stays at 0) until the QML side calls deactivate().
    // This test pins that contract so future refactors don't accidentally
    // re-introduce a hidden deactivate() inside commit() and double-fire
    // the signal once the QML also deactivates.
    function test_commitDoesNotDeactivateOnItsOwn() {
        clipperId.addWorldPoint(Qt.vector3d(0, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 0, 0))
        clipperId.addWorldPoint(Qt.vector3d(10, 10, 0))
        clipperId.closePolygon()

        clipperId.commit(LazClipInteraction.Keep)
        // Synchronous failure path (no scene node) — clipFailed fires but
        // no deactivate emission.
        compare(clipFailedSpy.count, 1)
        compare(deactivatedSpy.count, 0)
    }
}
