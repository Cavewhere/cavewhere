import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

// Behaviour of the sidebar tool-property flyout: it tracks the armed tool via
// InteractionManager.activeInteraction, shows only when the armed tool has
// options (ToolItem.propertyContent), loads that tool's content, disarms from
// its close button, and hides when the host gates it off.
Item {
    id: rootId
    width: 500
    height: 400

    BaseTurnTableInteraction { id: defaultInteractionId; anchors.fill: parent }
    BaseTurnTableInteraction { id: toolWithOptionsId; anchors.fill: parent }
    BaseTurnTableInteraction { id: toolNoOptionsId; anchors.fill: parent }
    // Armable, but deliberately absent from the flyout's toolModel — stands in
    // for an interaction the active page's model doesn't publish.
    BaseTurnTableInteraction { id: orphanInteractionId; anchors.fill: parent }

    InteractionManager {
        id: managerId
        interactions: [defaultInteractionId, toolWithOptionsId, toolNoOptionsId, orphanInteractionId]
        defaultInteraction: defaultInteractionId
    }

    Component {
        id: optionsComponentId
        Rectangle {
            objectName: "flyoutTestContent"
            implicitWidth: 120
            implicitHeight: 40
            color: "red"
        }
    }

    // Declared with ids (not inline in toolModel) so the page-swap test can
    // reassign flyoutId.toolModel to different subsets and init() can restore it.
    ToolItem {
        id: withOptionsToolId
        interaction: toolWithOptionsId
        text: "With Options"
        iconSource: "qrc:/twbs-icons/icons/crop.svg"
        propertyContent: optionsComponentId
    }

    ToolItem {
        id: noOptionsToolId
        interaction: toolNoOptionsId
        text: "No Options"
        iconSource: "qrc:/twbs-icons/icons/crop.svg"
    }

    ToolPropertyFlyout {
        id: flyoutId
        interactionManager: managerId
        toolModel: [withOptionsToolId, noOptionsToolId]
    }

    TestCase {
        name: "ToolPropertyFlyout"
        when: windowShown

        function init() {
            managerId.activeDefaultInteraction()
            flyoutId.hostVisible = true
            // Restore the full model — the page-swap test reassigns it.
            flyoutId.toolModel = [withOptionsToolId, noOptionsToolId]
        }

        function test_hiddenWhenDefaultActive() {
            verify(!flyoutId.shown, "no flyout while only the default interaction is active")
            verify(!flyoutId.visible)
        }

        function test_shownForToolWithOptions() {
            toolWithOptionsId.activate()
            compare(managerId.activeInteraction, toolWithOptionsId)
            tryVerify(() => flyoutId.shown, 2000, "flyout shows for an armed tool with options")
            verify(flyoutId.visible)
            compare(flyoutId.activeTool.text, "With Options")

            let content = findChild(flyoutId, "flyoutTestContent")
            verify(content !== null, "the armed tool's propertyContent is loaded")
        }

        function test_hiddenForToolWithoutOptions() {
            toolNoOptionsId.activate()
            compare(managerId.activeInteraction, toolNoOptionsId)
            verify(!flyoutId.shown, "an armed tool with no options opens no flyout")
            verify(!flyoutId.visible)
        }

        function test_closeButtonDisarmsTool() {
            toolWithOptionsId.activate()
            tryVerify(() => flyoutId.shown, 2000)

            let closeButton = findChild(flyoutId, "toolFlyoutCloseButton")
            verify(closeButton !== null, "close button should be findable")
            mouseClick(closeButton)

            tryVerify(() => managerId.activeInteraction === defaultInteractionId, 2000,
                      "closing the flyout disarms back to the default interaction")
            verify(!flyoutId.shown)
        }

        function test_hostVisibleGate() {
            toolWithOptionsId.activate()
            tryVerify(() => flyoutId.shown, 2000)
            verify(flyoutId.visible)

            flyoutId.hostVisible = false
            verify(!flyoutId.visible, "hostVisible false hides the flyout even with a tool armed")
            verify(flyoutId.shown, "shown still reflects the armed tool independent of the host gate")
        }

        // An interaction can be armed yet absent from this page's model (another
        // page owns it). activeTool must fall through the model scan to null
        // rather than latching a stale or first entry — and the flyout stays hidden.
        function test_activeToolNullWhenArmedInteractionNotInModel() {
            orphanInteractionId.activate()
            compare(managerId.activeInteraction, orphanInteractionId)
            verify(flyoutId.activeTool === null,
                   "activeTool is null when the armed interaction isn't in the model")
            verify(!flyoutId.shown)
            verify(!flyoutId.visible)
        }

        // A page swap changes toolModel out from under an armed tool. activeTool
        // is bound to the model, so it recomputes: null (hidden) when the new
        // model drops the armed tool, and tracks it again once a model includes it.
        function test_pageSwapUpdatesActiveTool() {
            toolWithOptionsId.activate()
            tryVerify(() => flyoutId.shown, 2000)
            compare(flyoutId.activeTool.text, "With Options")

            flyoutId.toolModel = [noOptionsToolId]
            verify(flyoutId.activeTool === null,
                   "activeTool recomputes to null when the model no longer holds the armed tool")
            verify(!flyoutId.shown, "flyout hides after the model swaps the armed tool away")

            flyoutId.toolModel = [withOptionsToolId, noOptionsToolId]
            tryVerify(() => flyoutId.shown, 2000,
                      "flyout tracks the armed tool again once the model includes it")
            compare(flyoutId.activeTool.text, "With Options")
        }
    }
}
