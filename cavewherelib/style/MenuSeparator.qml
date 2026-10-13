import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.MenuSeparator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    verticalPadding: Theme.menuItemVerticalPadding
    horizontalPadding: 0

    contentItem: QQ.Rectangle {
        implicitWidth: Theme.menuMinimumWidth
        implicitHeight: 1
        color: Theme.popupBorder
    }
}
