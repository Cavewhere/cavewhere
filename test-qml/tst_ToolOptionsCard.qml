import QtQuick as QQ
import QtTest
import cavewherelib

// The card both tool surfaces show the armed tool's options in. What each host
// does with it is covered where that host is — tst_ToolPropertyFlyout for the
// sidebar hinge, tst_ToolsTab for the drawer. What is covered here is the one
// thing neither host can see and both used to restate: the body has to end up
// with a real size.
QQ.Item {
    id: rootId
    width: 300
    height: 400

    BaseTurnTableInteraction { id: defaultInteractionId; anchors.fill: parent }
    BaseTurnTableInteraction { id: toolInteractionId; anchors.fill: parent }

    InteractionManager {
        id: managerId
        interactions: [defaultInteractionId, toolInteractionId]
        defaultInteraction: defaultInteractionId
    }

    QQ.Component {
        id: optionsComponentId

        QQ.Rectangle {
            objectName: "optionsContent"
            implicitWidth: 120
            implicitHeight: 40
            color: "red"
        }
    }

    ToolItem {
        id: toolId
        interaction: toolInteractionId
        text: "With Options"
        flyoutTitle: "With Options"
        iconSource: "qrc:/twbs-icons/icons/crop.svg"
        propertyContent: optionsComponentId
    }

    ToolOptionsCard {
        id: cardId
        objectName: "toolOptionsCard"
        width: rootId.width

        interactionManager: managerId
        toolModel: [toolId]
    }

    TestCase {
        name: "ToolOptionsCard"
        when: windowShown

        function init() {
            managerId.activeDefaultInteraction()
        }

        // The card's body once ended up with a zero-height hit rect: the options
        // drew, because nothing here clips, and took no input. Clicking cannot
        // catch that — QtTest maps to the item's own center, and hit-testing
        // descends through unclipped parents whatever their size — so this
        // asserts the size the input depends on instead.
        function test_theOptionsBodyIsGivenARealSize() {
            toolInteractionId.activate()
            tryVerify(() => cardId.hasOptions, 2000, "the armed tool's options showed")

            let content = findChild(cardId, "optionsContent")
            verify(content !== null, "optionsContent not found")

            let loader = content.parent
            tryVerify(() => loader.height >= content.implicitHeight, 2000,
                      "the body is as tall as the options it holds, not zero")
            verify(loader.width > 0, "and as wide as the card gave it")

            verify(cardId.height > loader.height,
                   "and the card grew by the body, on top of its own header")
        }
    }
}
