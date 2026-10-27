import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A primary action button and a chevron button joined into one
// GitHub-style split button: a single outline, one shared 1px
// divider, rounded only on the outer corners. The primary button
// always fires clicked() directly - the menu never sits between the
// user and the main action. The chevron only appears when a menu is
// set and carries a tooltip naming what's behind it.
//
// Both segments are the style's Button, so padding, font, and icon
// color come from CaveWhereStyle. Only the background is replaced:
// SegmentPanel draws the style's button panel (CaveWhereStyle's
// StyleButtonPanel, which cavewherelib cannot import) with the seam
// corners squared off.
QQ.Row {
    id: splitButtonId

    property QC.Menu menu: null
    property string menuToolTip: qsTr("More options")

    property alias buttonObjectName: mainButtonId.objectName
    property alias text: mainButtonId.text
    property alias iconSource: mainButtonId.icon.source

    signal clicked()

    // Overlap the two 1px borders into a single shared divider.
    spacing: -1

    component SegmentPanel: QQ.Rectangle {
        id: panelId

        required property QC.Button control
        // The edge that joins the other segment: its corners are square.
        property bool seamOnLeft: false
        property bool seamOnRight: false

        implicitWidth: Theme.controlHeight
        implicitHeight: Theme.controlHeight

        radius: Theme.controlRadius
        topLeftRadius: panelId.seamOnLeft ? 0 : panelId.radius
        bottomLeftRadius: panelId.seamOnLeft ? 0 : panelId.radius
        topRightRadius: panelId.seamOnRight ? 0 : panelId.radius
        bottomRightRadius: panelId.seamOnRight ? 0 : panelId.radius

        opacity: enabled ? 1 : Theme.disabledOpacity
        color: {
            if (panelId.control.down) {
                return Theme.buttonPressed
            }
            return panelId.control.hovered ? Theme.buttonHover : Theme.buttonSurface
        }
        border.width: 1
        border.color: {
            if (panelId.control.down) {
                return Theme.buttonPressedBorder
            }
            return panelId.control.hovered ? Theme.buttonHoverBorder : Theme.buttonBorder
        }

        // The one-pixel lip under a resting button.
        QQ.Rectangle {
            z: -1
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: -1
            topLeftRadius: panelId.topLeftRadius
            bottomLeftRadius: panelId.bottomLeftRadius
            topRightRadius: panelId.topRightRadius
            bottomRightRadius: panelId.bottomRightRadius
            color: Theme.buttonShadow
            visible: !panelId.control.down
        }

        // Keyboard-focus outline, square on the seam side like the panel.
        // Its seam edge sits on the divider, so it stays out of the other segment.
        QQ.Rectangle {
            id: focusRingId

            readonly property int ringOutset: Theme.focusRingOffset + Theme.focusRingWidth

            visible: panelId.control.visualFocus
            anchors.fill: parent
            anchors.topMargin: -focusRingId.ringOutset
            anchors.bottomMargin: -focusRingId.ringOutset
            anchors.leftMargin: panelId.seamOnLeft ? 0 : -focusRingId.ringOutset
            anchors.rightMargin: panelId.seamOnRight ? 0 : -focusRingId.ringOutset
            radius: panelId.radius + focusRingId.ringOutset
            topLeftRadius: panelId.seamOnLeft ? 0 : focusRingId.radius
            bottomLeftRadius: panelId.seamOnLeft ? 0 : focusRingId.radius
            topRightRadius: panelId.seamOnRight ? 0 : focusRingId.radius
            bottomRightRadius: panelId.seamOnRight ? 0 : focusRingId.radius
            color: "transparent"
            border.width: Theme.focusRingWidth
            border.color: Theme.focusRing
        }
    }

    QC.Button {
        id: mainButtonId

        // The active segment draws over the shared divider, so its hover,
        // pressed, and focus outline stay unbroken along the seam.
        z: hovered || down || visualFocus ? 1 : 0
        // A whole-pixel width keeps the shared divider on a pixel column
        // instead of blurring it across two.
        width: Math.ceil(implicitWidth)

        background: SegmentPanel {
            control: mainButtonId
            seamOnRight: splitButtonId.menu !== null
        }

        onClicked: {
            splitButtonId.clicked()
        }
    }

    QC.Button {
        id: menuButtonId
        objectName: "menuButton"

        z: hovered || down || visualFocus ? 1 : 0
        visible: splitButtonId.menu !== null
        height: mainButtonId.height

        icon.source: "qrc:/twbs-icons/icons/chevron-down.svg"

        background: SegmentPanel {
            control: menuButtonId
            seamOnLeft: true
        }

        QC.ToolTip.visible: hovered && splitButtonId.menuToolTip.length > 0
        QC.ToolTip.text: splitButtonId.menuToolTip
        QC.ToolTip.delay: Theme.toolTipDelay

        onClicked: {
            splitButtonId.menu.popup(menuButtonId, 0, menuButtonId.height)
        }
    }
}
