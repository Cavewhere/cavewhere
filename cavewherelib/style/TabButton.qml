import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.TabButton {
    id: control

    // TabViewVertical uses tab buttons as list delegates with no tab bar; those
    // round all four corners.
    readonly property bool inTabBar: T.TabBar.tabBar !== null
    readonly property QQ.color textColor: control.checked || control.hovered
                                          ? control.palette.windowText : Theme.tabText

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    verticalPadding: Theme.compactButtonPadding
    horizontalPadding: Theme.buttonHorizontalPadding
    spacing: Theme.controlSpacing

    font.bold: control.checked

    icon.width: Theme.iconSizeButton
    icon.height: Theme.iconSizeButton
    icon.color: control.textColor

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.textColor
        opacity: control.enabled ? 1 : Theme.disabledOpacity
    }

    background: QQ.Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.controlRadius
        bottomLeftRadius: control.inTabBar ? 0 : Theme.controlRadius
        bottomRightRadius: control.inTabBar ? 0 : Theme.controlRadius
        color: control.checked ? Theme.background : "transparent"

        StyleFocusRing {
            visible: control.visualFocus
        }
    }
}
