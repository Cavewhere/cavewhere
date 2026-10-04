import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.RoundButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.compactButtonPadding
    spacing: Theme.controlSpacing

    icon.width: Theme.iconSizeButton
    icon.height: Theme.iconSizeButton
    icon.color: control.highlighted ? Theme.buttonPrimaryText : control.palette.active.buttonText

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.highlighted ? Theme.buttonPrimaryText : control.palette.active.buttonText
        opacity: !control.enabled ? Theme.disabledOpacity
                 : control.checkable && !control.checked ? Theme.uncheckedToggleOpacity : 1
    }

    // Callers shrink the button below controlHeight and set radius to 0 or 2,
    // so the panel follows the control's radius rather than controlRadius.
    background: StyleButtonPanel {
        implicitWidth: Theme.controlHeight
        implicitHeight: Theme.controlHeight
        radius: control.radius
        hovered: control.hovered
        down: control.down
        checked: control.checked
        focused: control.visualFocus
        quiet: control.flat
        primary: control.highlighted
    }
}
