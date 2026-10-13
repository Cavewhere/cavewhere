import QtQuick as QQ
import cavewherelib

// Shared background for Button, ToolButton, RoundButton, and TabButton-like
// controls. The owning control passes its state in; this file reads no control.
QQ.Rectangle {
    id: panel

    property bool hovered: false
    property bool down: false
    property bool checked: false
    property bool focused: false
    // Transparent at rest: ToolButton and flat buttons.
    property bool quiet: false
    // The one solid call-to-action button in a view: Button.highlighted.
    property bool primary: false

    readonly property bool resting: !panel.hovered && !panel.down && !panel.checked

    radius: Theme.controlRadius
    opacity: enabled ? 1 : Theme.disabledOpacity
    color: {
        if (panel.primary) {
            return panel.down || panel.hovered ? Theme.buttonPrimaryHover : Theme.buttonPrimary
        }
        if (panel.down) {
            return Theme.buttonPressed
        }
        if (panel.checked) {
            return Theme.buttonChecked
        }
        if (panel.quiet && !panel.hovered) {
            return "transparent"
        }
        return panel.hovered ? Theme.buttonHover : Theme.buttonSurface
    }
    border.width: panel.primary || (panel.quiet && panel.resting) ? 0 : 1
    border.color: {
        if (panel.checked) {
            return Theme.buttonCheckedBorder
        }
        if (panel.down) {
            return Theme.buttonPressedBorder
        }
        return panel.hovered ? Theme.buttonHoverBorder : Theme.buttonBorder
    }

    // The one-pixel lip under a resting button.
    QQ.Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 1
        anchors.bottomMargin: -1
        radius: panel.radius
        color: Theme.buttonShadow
        visible: !panel.down && !panel.quiet && !panel.primary
    }

    StyleFocusRing {
        visible: panel.focused
    }
}
