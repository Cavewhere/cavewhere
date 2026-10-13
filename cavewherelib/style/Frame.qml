import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.Frame {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.containerPadding

    background: QQ.Rectangle {
        color: "transparent"
        border.width: 1
        border.color: Theme.border
        radius: Theme.panelRadius
    }
}
