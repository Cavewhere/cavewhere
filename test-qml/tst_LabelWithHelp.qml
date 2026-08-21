import QtQuick as QQ
import QtTest
import cavewherelib

QQ.Item {
    id: rootId

    width: 300
    height: 200

    // Two labels, because a synthetic click sticks: offscreen, the MouseArea
    // under a mouseClick() keeps containsMouse true and later moves away never
    // clear it, so a clicked label can no longer answer a hover question. A
    // label that is only ever hovered enters and leaves cleanly, in any order.
    LabelWithHelp {
        id: clickLabelId
        objectName: "clickLabel"
        anchors.top: rootId.top
        anchors.left: rootId.left

        text: "Grid convergence:"
        helpArea: clickHelpId
    }

    LabelWithHelp {
        id: hoverLabelId
        objectName: "hoverLabel"
        anchors.top: rootId.top
        anchors.right: rootId.right

        text: "Declination:"
        helpArea: hoverHelpId
    }

    HelpArea {
        id: clickHelpId
        objectName: "clickHelp"
        anchors.top: clickLabelId.bottom
        anchors.left: rootId.left
        width: rootId.width / 2

        text: "Explains the value the clicked label names."
    }

    HelpArea {
        id: hoverHelpId
        objectName: "hoverHelp"
        anchors.top: hoverLabelId.bottom
        anchors.right: rootId.right
        width: rootId.width / 2

        text: "Explains the value the hovered label names."
    }

    TestCase {
        name: "LabelWithHelp"
        when: windowShown

        // Well clear of both labels, for parking the mouse.
        readonly property point offLabels: Qt.point(rootId.width / 2, rootId.height - 1)

        function init() {
            clickHelpId.visible = false
            hoverHelpId.visible = false

            mouseMove(rootId, offLabels.x, offLabels.y)
            tryVerify(() => hoverLabelId.state === "", 1000,
                      "the hover label starts each case at rest")
        }

        // ── A real click toggles the help ────────────────────────────────────

        // Clicked rather than driven through helpArea.visible: the label's whole
        // job is to be a hit target, and only a press proves one reaches it.
        function test_clickTogglesTheHelpArea() {
            compare(clickHelpId.visible, false, "the help area starts hidden")

            mouseClick(clickLabelId, clickLabelId.width / 2, clickLabelId.height / 2)
            tryVerify(() => clickHelpId.visible, 1000,
                      "clicking the label opens its help area")

            mouseClick(clickLabelId, clickLabelId.width / 2, clickLabelId.height / 2)
            tryVerify(() => clickHelpId.visible === false, 1000,
                      "clicking the label again closes it")
        }

        // ── Hover dresses the label as a link ───────────────────────────────

        function test_hoverAccentsAndUnderlinesTheLabel() {
            compare(hoverLabelId.font.underline, false, "the label is plain at rest")

            mouseMove(hoverLabelId, hoverLabelId.width / 2, hoverLabelId.height / 2)

            tryVerify(() => hoverLabelId.font.underline, 1000,
                      "hovering underlines the label")
            compare(hoverLabelId.color, Theme.accent,
                    "and accents it, so it reads as clickable")

            mouseMove(rootId, offLabels.x, offLabels.y)
            tryVerify(() => hoverLabelId.font.underline === false, 1000,
                      "leaving the label puts the plain styling back")
        }

        // ── ...and the cursor agrees with that styling (#663) ────────────────

        // The label looked like a link on hover long before it pointed like one,
        // which is the whole of #663: accent + underline under an arrow cursor
        // reads as decoration rather than as something to click.
        function test_hoverShowsAPointingHandCursor() {
            const area = findChild(hoverLabelId, "labelWithHelpMouseArea")
            verify(area !== null, "the label's hover area must be findable")
            compare(area.cursorShape, Qt.PointingHandCursor,
                    "the label offers a browser's pointing hand")
        }
    }
}
