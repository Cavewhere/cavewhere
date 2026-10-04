import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.MenuItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    verticalPadding: Theme.menuItemVerticalPadding
    leftPadding: Theme.menuItemHorizontalPadding
    rightPadding: Theme.menuItemHorizontalPadding
    spacing: Theme.controlSpacing

    icon.width: Theme.iconSizeButton
    icon.height: Theme.iconSizeButton
    icon.color: control.palette.active.windowText

    // Every row keeps the check column, so checkable and plain items line up.
    contentItem: IconLabel {
        readonly property real arrowPadding: control.subMenu && control.arrow ? control.arrow.width + control.spacing : 0
        readonly property real indicatorPadding: Theme.menuIndicatorColumn + control.spacing
        leftPadding: !control.mirrored ? indicatorPadding : arrowPadding
        rightPadding: control.mirrored ? indicatorPadding : arrowPadding

        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: Qt.AlignLeft
        icon: control.icon
        text: control.text
        font: control.font
        color: control.palette.active.windowText
        opacity: control.enabled ? 1 : Theme.disabledOpacity
    }

    indicator: ColorImage {
        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Theme.menuIndicatorColumn
        height: Theme.menuIndicatorColumn
        sourceSize: Qt.size(width, height)
        source: "qrc:/twbs-icons/icons/check-lg.svg"
        color: control.palette.active.windowText
        visible: control.checked
        opacity: control.enabled ? 1 : Theme.disabledOpacity
    }

    arrow: ColorImage {
        x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Theme.chevronSize
        height: Theme.chevronSize
        sourceSize: Qt.size(width, height)
        source: "qrc:/twbs-icons/icons/chevron-right.svg"
        color: control.palette.active.windowText
        mirror: control.mirrored
        visible: control.subMenu
        opacity: control.enabled ? 1 : Theme.disabledOpacity
    }

    // The highlight stands 1 px clear of the menu's border.
    background: QQ.Rectangle {
        implicitWidth: Theme.menuMinimumWidth
        implicitHeight: Theme.listRowHeight
        x: 1
        width: control.width - 2
        radius: Theme.rowRadius
        color: control.down || control.highlighted ? Theme.popupSelected : "transparent"
    }
}
