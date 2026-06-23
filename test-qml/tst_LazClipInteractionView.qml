import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// View-level test for LazClipInteractionView. Verifies the QML wiring
// for Escape — when the view has focus, pressing Escape must deactivate
// the clip interaction (handing focus back to the default turn-table
// interaction via InteractionManager).
Item {
    id: rootId
    width: 400
    height: 300

    BaseTurnTableInteraction {
        id: turnTableId
        anchors.fill: parent
    }

    LazClipInteractionView {
        id: clipViewId
        objectName: "lazClipInteractionView"
        turnTableInteraction: turnTableId
    }

    // Mimics the always-on, full-fill tap-away overlay (LeadView) that sits
    // ABOVE the clip view in GLTerrainRenderer: a bare TapHandler covering the
    // whole viewport. Reproduces the real composition so the clip toolbar's
    // buttons are clicked through a higher overlay, not in isolation.
    Item {
        id: tapAwayOverlayId
        anchors.fill: parent

        property int tapAwayCount: 0

        TapHandler {
            onTapped: tapAwayOverlayId.tapAwayCount++
        }
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [turnTableId, clipViewId]
        defaultInteraction: turnTableId
    }

    SignalSpy {
        id: deactivatedSpy
        target: clipViewId
        signalName: "deactivated"
    }

    TestCase {
        name: "LazClipInteractionView"
        when: windowShown

        function init() {
            deactivatedSpy.clear()
            // Make sure we start in the default state.
            interactionManagerId.activeDefaultInteraction()
        }

        function test_escapeDeactivatesClipInteraction() {
            // Activate the clip tool — InteractionManager makes it
            // visible/enabled and gives it active focus.
            clipViewId.activate()
            compare(interactionManagerId.activeInteraction, clipViewId)
            tryVerify(() => clipViewId.activeFocus,
                      2000, "Clip view should have active focus after activate()")

            // The handler under test: QQ.Keys.onEscapePressed should
            // call deactivate(), which routes through InteractionManager
            // to restore the default interaction.
            keyClick(Qt.Key_Escape)

            compare(deactivatedSpy.count, 1)
            tryVerify(() => interactionManagerId.activeInteraction === turnTableId,
                      2000, "Default (turn-table) interaction should be active after Escape")
            tryVerify(() => !clipViewId.enabled,
                      2000, "Clip view should be disabled after Escape")
        }

        // Escape with an in-progress polygon must both drop the polygon
        // (via the C++ onDeactivated → cancel() wire) and exit the tool.
        function test_escapeWithDrawnPolygonResetsAndDeactivates() {
            clipViewId.activate()
            tryVerify(() => clipViewId.activeFocus, 2000)

            clipViewId.addWorldPoint(Qt.vector3d(0, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 0, 0))
            compare(clipViewId.state, LazClipInteraction.Drawing)
            compare(clipViewId.pointCount, 2)

            keyClick(Qt.Key_Escape)

            tryVerify(() => interactionManagerId.activeInteraction === turnTableId, 2000)
            compare(clipViewId.state, LazClipInteraction.Idle)
            compare(clipViewId.pointCount, 0)
            compare(clipViewId.errorMessage, "")
        }

        // Clicking the Cancel button exits the tool the same way Escape
        // does — the button's onClicked is wired to deactivate().
        function test_cancelButtonDeactivates() {
            clipViewId.activate()
            tryVerify(() => clipViewId.activeFocus, 2000)

            clipViewId.addWorldPoint(Qt.vector3d(0, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 10, 0))
            clipViewId.closePolygon()
            compare(clipViewId.state, LazClipInteraction.Closed)

            let cancelButton = findChild(clipViewId, "lazClipCancelButton")
            verify(cancelButton !== null, "Cancel button should be findable")
            tapAwayOverlayId.tapAwayCount = 0
            mouseClick(cancelButton)

            // The button must win the tap over the higher full-fill tap-away
            // overlay — otherwise the overlay swallows it and Cancel is dead.
            compare(tapAwayOverlayId.tapAwayCount, 0,
                    "tap-away overlay must not steal the Cancel button tap")
            compare(deactivatedSpy.count, 1)
            tryVerify(() => interactionManagerId.activeInteraction === turnTableId, 2000)
            compare(clipViewId.state, LazClipInteraction.Idle)
            compare(clipViewId.pointCount, 0)
        }

        // Pressing Crop must immediately exit the tool (the worker runs in
        // the background under cwFutureManagerToken). With no scene node
        // bound, commit() takes its synchronous-failure path, which still
        // leaves the QML to deactivate the view.
        function test_cropButtonDeactivatesEvenOnSynchronousFailure() {
            clipViewId.activate()
            tryVerify(() => clipViewId.activeFocus, 2000)

            clipViewId.addWorldPoint(Qt.vector3d(0, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 10, 0))
            clipViewId.closePolygon()
            compare(clipViewId.state, LazClipInteraction.Closed)

            let cropButton = findChild(clipViewId, "lazClipCropButton")
            verify(cropButton !== null, "Crop button should be findable")
            mouseClick(cropButton)

            compare(deactivatedSpy.count, 1)
            tryVerify(() => interactionManagerId.activeInteraction === turnTableId, 2000)
        }
    }
}
