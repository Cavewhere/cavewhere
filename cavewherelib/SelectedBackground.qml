import QtQuick as QQ
import cavewherelib

QQ.Rectangle {
    id: selectedBackground

    anchors.margins: -2

    radius: 4

    color: Theme.accent
    border.width: 1
    border.color: Theme.textInverse

    opacity: .5
}
