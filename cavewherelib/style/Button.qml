import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.Button {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    verticalPadding: Theme.controlVerticalPadding
    horizontalPadding: control.text.length > 0 ? Theme.buttonHorizontalPadding : Theme.controlVerticalPadding + 2
    spacing: Theme.controlSpacing

    icon.width: Theme.iconSizeButton
    icon.height: Theme.iconSizeButton
    icon.color: control.highlighted ? Theme.buttonPrimaryText : control.palette.buttonText

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.highlighted ? Theme.buttonPrimaryText : control.palette.buttonText
        opacity: !control.enabled ? Theme.disabledOpacity
                 : control.checkable && !control.checked ? Theme.uncheckedToggleOpacity : 1
    }

    background: StyleButtonPanel {
        implicitWidth: Theme.controlHeight
        implicitHeight: Theme.controlHeight
        hovered: control.hovered
        down: control.down
        checked: control.checked
        focused: control.visualFocus
        quiet: control.flat
        primary: control.highlighted
    }
}
