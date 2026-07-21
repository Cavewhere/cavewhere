import QtQuick as QQ
import cavewherelib

PositionItem {
    id: positioner

    default property alias conentData: contentId.data

    QQ.Item {
        id: contentId
        scale: positioner.parent ? 1.0 / positioner.parent.scale : 1.0
        rotation: positioner.parent ? -positioner.parent.rotation : 0.0
    }
}
