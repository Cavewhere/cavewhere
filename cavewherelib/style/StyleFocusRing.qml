import QtQuick as QQ
import cavewherelib

// Keyboard-focus outline drawn outside its parent's edge.
QQ.Rectangle {
    anchors.fill: parent
    anchors.margins: -(Theme.focusRingOffset + Theme.focusRingWidth)
    radius: (parent ? parent.radius : 0) + Theme.focusRingOffset + Theme.focusRingWidth
    color: "transparent"
    border.width: Theme.focusRingWidth
    border.color: Theme.focusRing
}
