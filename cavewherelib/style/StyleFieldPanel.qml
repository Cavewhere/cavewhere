import QtQuick as QQ
import cavewherelib

// Shared background for TextField, TextArea, ComboBox, and SpinBox.
QQ.Rectangle {
    property bool hovered: false
    property bool focused: false

    radius: Theme.controlRadius
    color: Theme.fieldSurface
    border.width: focused ? Theme.focusRingWidth : hovered ? 2 : 1
    border.color: focused ? Theme.focusRing : Theme.fieldBorder
}
