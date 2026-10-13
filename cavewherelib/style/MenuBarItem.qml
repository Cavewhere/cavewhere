import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.MenuBarItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    verticalPadding: Theme.controlVerticalPadding
    horizontalPadding: Theme.menuBarItemHorizontalPadding
    spacing: Theme.controlSpacing

    icon.width: Theme.iconSizeButton
    icon.height: Theme.iconSizeButton
    icon.color: Theme.chromeText

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: Qt.AlignLeft
        icon: control.icon
        text: control.text
        font: control.font
        color: Theme.chromeText
        opacity: control.enabled ? 1 : Theme.disabledOpacity
    }

    background: QQ.Rectangle {
        radius: Theme.rowRadius
        color: control.down || control.highlighted ? Theme.hoverOverlay : "transparent"
    }
}
