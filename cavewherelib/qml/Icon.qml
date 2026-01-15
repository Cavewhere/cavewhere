import QtQuick
import QtQuick.Effects
import cavewherelib

Image {
    id: iconId

    property color colorizationColor: Theme.icon
    property bool colorizeEnabled: true

    layer {
        enabled: iconId.colorizeEnabled
        effect: MultiEffect {
            colorization: 1.0
            colorizationColor: iconId.colorizationColor
            brightness: 1.0
        }
    }
}
