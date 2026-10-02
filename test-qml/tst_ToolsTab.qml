import QtQuick as QQ
import QtTest
import cavewherelib

// The compact Tools drawer: the same list<ToolItem> the sidebar rail renders,
// grouped, as named rows, with the armed tool's options in line under them.
QQ.Item {
    id: rootId
    width: 320
    height: 600

    BaseTurnTableInteraction { id: defaultInteractionId; anchors.fill: parent }
    BaseTurnTableInteraction { id: pickInteractionId; anchors.fill: parent }
    BaseTurnTableInteraction { id: clipInteractionId; anchors.fill: parent }
    BaseTurnTableInteraction { id: lengthInteractionId; anchors.fill: parent }

    InteractionManager {
        id: managerId
        interactions: [defaultInteractionId, pickInteractionId, clipInteractionId,
                       lengthInteractionId]
        defaultInteraction: defaultInteractionId
    }

    QQ.Component {
        id: optionsComponentId
        QQ.Rectangle {
            objectName: "toolsTabTestContent"
            implicitWidth: 120
            implicitHeight: 40
            color: "red"
        }
    }

    ToolItem {
        id: pickToolId
        interaction: pickInteractionId
        group: "Measure"
        groupId: "measure"
        text: "Pick"
        buttonObjectName: "pickToolButton"
        iconSource: "qrc:/twbs-icons/icons/crop.svg"
    }

    // Shares Measure with the pick tool, so that grouping the list and not
    // grouping it are two different pictures — with one tool per group they are
    // the same one, and nothing here could tell them apart.
    ToolItem {
        id: lengthToolId
        interaction: lengthInteractionId
        group: "Measure"
        groupId: "measure"
        text: "Length"
        buttonObjectName: "lengthToolButton"
        iconSource: "qrc:/twbs-icons/icons/rulers.svg"
    }

    ToolItem {
        id: clipToolId
        interaction: clipInteractionId
        group: "Point Cloud"
        groupId: "pointCloud"
        text: "Clip"
        flyoutTitle: "Point Cloud Clip"
        buttonObjectName: "clipToolButton"
        iconSource: "qrc:/twbs-icons/icons/scissors.svg"
        propertyContent: optionsComponentId
    }

    ToolsTab {
        id: toolsTabId
        objectName: "toolsTab"
        anchors.fill: parent

        interactionManager: managerId
        toolModel: [pickToolId, lengthToolId, clipToolId]
    }

    TestCase {
        name: "ToolsTab"
        when: windowShown

        function init() {
            managerId.activeDefaultInteraction()
            toolsTabId.toolModel = [pickToolId, lengthToolId, clipToolId]
        }

        // The rail groups its buttons and so does this, off the same shared
        // helper — which nothing else in the suite covers, since the rail has no
        // test file of its own.
        function test_toolsAreGatheredUnderOneHeadingPerGroup() {
            compare(toolsTabId.groups.length, 2, "two groups out of three tools")

            compare(toolsTabId.groups[0].group, "Measure", "in first-seen order")
            compare(toolsTabId.groups[0].tools.length, 2, "both Measure tools under it")

            compare(toolsTabId.groups[1].group, "Point Cloud")
            compare(toolsTabId.groups[1].tools.length, 1)
        }

        function test_everyToolGetsANamedRow() {
            let pick = findChild(toolsTabId, "pickToolButton")
            verify(pick !== null, "pickToolButton not found")
            compare(pick.text, "Pick", "the drawer has room for the name, unlike the rail")

            verify(findChild(toolsTabId, "clipToolButton") !== null, "clipToolButton not found")
        }

        function test_aRowArmsItsTool() {
            let clip = findChild(toolsTabId, "clipToolButton")
            verify(clip !== null, "clipToolButton not found")
            verify(!clip.checked, "nothing armed to begin with")

            waitForRendering(toolsTabId)
            mouseClick(clip)

            tryVerify(() => managerId.activeInteraction === clipInteractionId, 2000,
                      "clicking the row armed the tool")
            verify(clip.checked, "and the row shows it")
        }

        // The rail and the flyout are two controls because a 50px sidebar cannot
        // hold the options. A drawer can, so they are one column here.
        function test_theArmedToolsOptionsAppearInLine() {
            let card = findChild(toolsTabId, "toolsTabOptionsCard")
            verify(card !== null, "toolsTabOptionsCard not found")
            verify(!card.visible, "no options card while nothing is armed")

            clipInteractionId.activate()
            tryVerify(() => card.visible, 2000, "arming a tool with options showed its card")
            compare(findChild(card, "flyoutCardTitle").text, "Point Cloud Clip")
            verify(findChild(card, "toolsTabTestContent") !== null,
                   "the armed tool's propertyContent is loaded")
        }

        function test_aToolWithNoOptionsShowsNoCard() {
            pickInteractionId.activate()
            compare(managerId.activeInteraction, pickInteractionId)

            let card = findChild(toolsTabId, "toolsTabOptionsCard")
            verify(!card.visible, "a tool with no options has no card to show")
        }

        // Closing the options card is disarming the tool — there is nothing else
        // for it to mean, and it is the drawer's only way back to no tool.
        function test_closingTheOptionsCardDisarmsTheTool() {
            clipInteractionId.activate()
            let card = findChild(toolsTabId, "toolsTabOptionsCard")
            tryVerify(() => card.visible, 2000)

            mouseClick(findChild(card, "flyoutCardCloseButton"))

            tryVerify(() => managerId.activeInteraction === defaultInteractionId, 2000,
                      "closing the card disarmed back to the default interaction")
        }

        // Pages that publish no tools reach this drawer too, and an empty tab
        // that says nothing reads as something that failed to load.
        function test_aPageWithNoToolsSaysSo() {
            let empty = findChild(toolsTabId, "toolsTabEmptyLabel")
            verify(empty !== null, "toolsTabEmptyLabel not found")
            verify(!empty.visible, "nothing to say while there are tools")

            toolsTabId.toolModel = []
            verify(empty.visible, "and it says so when the page has none")
        }
    }
}
