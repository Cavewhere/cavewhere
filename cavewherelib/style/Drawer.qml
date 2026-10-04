import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.Drawer {
    id: control

    parent: T.Overlay.overlay

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    // One extra pixel on the inner edge clears the border line.
    topPadding: QQ.SafeArea.margins.top + (control.edge === Qt.BottomEdge)
    leftPadding: QQ.SafeArea.margins.left + (control.edge === Qt.RightEdge)
    rightPadding: QQ.SafeArea.margins.right + (control.edge === Qt.LeftEdge)
    bottomPadding: QQ.SafeArea.margins.bottom + (control.edge === Qt.TopEdge)

    enter: QQ.Transition { QQ.SmoothedAnimation { velocity: Theme.drawerSlideVelocity } }
    exit: QQ.Transition { QQ.SmoothedAnimation { velocity: Theme.drawerSlideVelocity } }

    background: QQ.Rectangle {
        color: Theme.background

        QQ.Rectangle {
            readonly property bool horizontal: control.edge === Qt.LeftEdge || control.edge === Qt.RightEdge
            width: horizontal ? 1 : parent.width
            height: horizontal ? parent.height : 1
            color: Theme.border
            x: control.edge === Qt.LeftEdge ? parent.width - 1 : 0
            y: control.edge === Qt.TopEdge ? parent.height - 1 : 0
        }
    }

    T.Overlay.modal: QQ.Rectangle {
        color: Theme.overlayScrim
    }
}
