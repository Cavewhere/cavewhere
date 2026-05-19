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

            clipViewId.addWorldPoint(Qt.point(0, 0))
            clipViewId.addWorldPoint(Qt.point(10, 0))
            compare(clipViewId.state, LazClipInteraction.Drawing)
            compare(clipViewId.pointCount, 2)

            keyClick(Qt.Key_Escape)

            tryVerify(() => interactionManagerId.activeInteraction === turnTableId, 2000)
            compare(clipViewId.state, LazClipInteraction.Idle)
            compare(clipViewId.pointCount, 0)
            compare(clipViewId.errorMessage, "")
        }
    }
}
