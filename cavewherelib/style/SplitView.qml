import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.SplitView {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    handle: QQ.Rectangle {
        implicitWidth: control.orientation === Qt.Horizontal ? Theme.splitHandleThickness : control.width
        implicitHeight: control.orientation === Qt.Horizontal ? control.height : Theme.splitHandleThickness
        color: {
            if (T.SplitHandle.pressed) {
                return Theme.accent
            }
            return T.SplitHandle.hovered ? Theme.controlBorder : Theme.border
        }

        // A strip wider than the handle and centered on it, for an easier grab.
        containmentMask: QQ.Item {
            readonly property real overhang: (Theme.splitHandleGrabThickness - Theme.splitHandleThickness) / 2
            readonly property bool horizontal: control.orientation === Qt.Horizontal

            x: horizontal ? -overhang : 0
            y: horizontal ? 0 : -overhang
            width: horizontal ? Theme.splitHandleGrabThickness : control.width
            height: horizontal ? control.height : Theme.splitHandleGrabThickness
        }
    }
}
