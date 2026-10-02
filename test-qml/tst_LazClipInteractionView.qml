import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// View-level tests for LazClipInteractionView and how it pairs with the sidebar
// tool-property flyout that now hosts its Crop/Erase actions:
//   * Escape deactivates the tool even after the sidebar steals keyboard focus
//     (the reason the view uses a window Shortcut, not Keys.onEscapePressed).
//   * The flyout renders the clip options (Crop/Erase, no Cancel) while armed.
//   * Crop enables only once a polygon can commit, and committing exits.
Item {
    id: rootId
    width: 400
    height: 500

    // The clip view fills the scene area; the flyout lives below it (as it sits
    // off the 3D view in the app) so the tool's full-fill tap handlers never sit
    // over the flyout's buttons.
    Item {
        id: sceneAreaId
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
    }

    InteractionManager {
        id: interactionManagerId
        interactions: [turnTableId, clipViewId]
        defaultInteraction: turnTableId
    }

    // The real sidebar flyout, wired to the clip tool exactly as GLTerrainRenderer
    // wires it, so the button positions and the propertyContent load are covered
    // as an integration, not just in isolation.
    ToolPropertyFlyout {
        id: flyoutId
        y: sceneAreaId.height
        interactionManager: interactionManagerId
        toolModel: [
            ToolItem {
                interaction: clipViewId
                text: "Clip"
                flyoutTitle: "Point Cloud Clip"
                iconSource: "qrc:/twbs-icons/icons/scissors.svg"
                propertyContent: Component {
                    LazClipToolOptions { interaction: clipViewId }
                }
            }
        ]
    }

    // A focusable sink standing in for the sidebar/flyout taking keyboard focus
    // away from the clip view, as clicking a flyout control does.
    TextInput {
        id: focusThiefId
        objectName: "focusThief"
        x: 250
        y: sceneAreaId.height
        width: 100
        height: 24
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
            // Force the C++ onDeactivated → cancel() wire so any polygon a prior
            // test left in Drawing/Closed is dropped, and (via InteractionManager's
            // deactivated → activeDefaultInteraction connection) the default
            // interaction is restored. active()/activeDefaultInteraction() set
            // visible/enabled directly and never deactivate(), and activate()
            // doesn't reset either — so without this the suite would silently
            // depend on the alphabetical run order ending each test cleanly.
            clipViewId.deactivate()
            deactivatedSpy.clear()
        }

        function test_escapeDeactivatesClipInteraction() {
            // Activate the clip tool — InteractionManager makes it
            // visible/enabled and gives it active focus.
            clipViewId.activate()
            compare(interactionManagerId.activeInteraction, clipViewId)
            tryVerify(() => clipViewId.activeFocus,
                      2000, "Clip view should have active focus after activate()")

            // The window Shortcut under test calls deactivate(), which routes
            // through InteractionManager to restore the default interaction.
            keyClick(Qt.Key_Escape)

            compare(deactivatedSpy.count, 1)
            tryVerify(() => interactionManagerId.activeInteraction === turnTableId,
                      2000, "Default (turn-table) interaction should be active after Escape")
            tryVerify(() => !clipViewId.enabled,
                      2000, "Clip view should be disabled after Escape")
        }

        // The regression this guards: once the Crop/Erase actions moved to the
        // sidebar flyout, clicking them steals keyboard focus from the clip view.
        // A focus-scoped Escape (Keys.onEscapePressed) would then be dead. The
        // window-context Shortcut must still exit the tool.
        function test_escapeDeactivatesEvenWhenFocusStolen() {
            clipViewId.activate()
            tryVerify(() => clipViewId.activeFocus, 2000,
                      "Clip view should have focus after activate()")

            // Move focus off the clip view, as a click in the sidebar would.
            focusThiefId.forceActiveFocus()
            tryVerify(() => focusThiefId.activeFocus, 2000, "focus sink took focus")
            verify(!clipViewId.activeFocus, "clip view no longer holds focus")

            keyClick(Qt.Key_Escape)

            compare(deactivatedSpy.count, 1)
            tryVerify(() => interactionManagerId.activeInteraction === turnTableId, 2000,
                      "Escape must still exit the tool with focus elsewhere")
            tryVerify(() => !clipViewId.enabled, 2000)
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

        // While the clip tool is armed, the flyout shows its options: Crop and
        // Erase (no separate Cancel — the flyout's own × / Escape exit), under
        // the "Point Cloud Clip" heading. Crop/Erase enable only once a polygon
        // can commit.
        function test_flyoutShowsClipOptions() {
            clipViewId.activate()
            tryVerify(() => flyoutId.shown, 2000, "flyout shows while the clip tool is armed")
            verify(flyoutId.visible)
            compare(flyoutId.activeTool.flyoutTitle, "Point Cloud Clip")

            let cropButton = findChild(flyoutId, "lazClipCropButton")
            let eraseButton = findChild(flyoutId, "lazClipEraseButton")
            verify(cropButton !== null, "Crop button is in the flyout")
            verify(eraseButton !== null, "Erase button is in the flyout")
            verify(findChild(flyoutId, "lazClipCancelButton") === null,
                   "no Cancel button — the flyout × and Escape exit instead")

            verify(!cropButton.enabled, "Crop is disabled before a polygon can commit")
            verify(!eraseButton.enabled, "Erase is disabled before a polygon can commit")

            clipViewId.addWorldPoint(Qt.vector3d(0, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 10, 0))
            clipViewId.closePolygon()
            compare(clipViewId.state, LazClipInteraction.Closed)

            tryVerify(() => cropButton.enabled, 2000, "Crop enables once the polygon closes")
            tryVerify(() => eraseButton.enabled, 2000, "Erase enables once the polygon closes")
        }

        // Pressing Crop must immediately exit the tool (the worker runs in
        // the background under cwFutureManagerToken). With no scene node
        // bound, commit() takes its synchronous-failure path, which still
        // leaves the QML to deactivate the view.
        function test_cropButtonDeactivatesEvenOnSynchronousFailure() {
            clipViewId.activate()
            tryVerify(() => flyoutId.shown, 2000)

            clipViewId.addWorldPoint(Qt.vector3d(0, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 0, 0))
            clipViewId.addWorldPoint(Qt.vector3d(10, 10, 0))
            clipViewId.closePolygon()
            compare(clipViewId.state, LazClipInteraction.Closed)

            let cropButton = findChild(flyoutId, "lazClipCropButton")
            verify(cropButton !== null, "Crop button should be findable")
            tryVerify(() => cropButton.enabled, 2000)
            mouseClick(cropButton)

            compare(deactivatedSpy.count, 1)
            tryVerify(() => interactionManagerId.activeInteraction === turnTableId, 2000)
        }
    }
}
