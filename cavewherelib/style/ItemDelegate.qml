import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    verticalPadding: Theme.menuItemVerticalPadding
    horizontalPadding: Theme.fieldHorizontalPadding
    spacing: Theme.controlSpacing

    icon.width: Theme.iconSizeButton
    icon.height: Theme.iconSizeButton
    icon.color: control.palette.active.text

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: control.display === IconLabel.IconOnly || control.display === IconLabel.TextUnderIcon
                   ? Qt.AlignCenter : Qt.AlignLeft
        icon: control.icon
        text: control.text
        font: control.font
        color: control.palette.active.text
        opacity: control.enabled ? 1 : Theme.disabledOpacity
    }

    background: QQ.Rectangle {
        implicitHeight: Theme.listRowHeight
        radius: Theme.rowRadius
        color: {
            if (control.down || control.highlighted) {
                return Theme.popupSelected
            }
            return control.hovered ? Theme.hoverOverlay : "transparent"
        }

        StyleFocusRing {
            visible: control.visualFocus
        }
    }
}
