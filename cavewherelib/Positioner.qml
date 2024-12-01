import QtQuick

Item {
    id: positioner

    default property alias conentData: contentId.data

    Item {
        id: contentId
        scale: 1.0 / positioner.parent.scale
        rotation: -positioner.parent.rotation
    }
}
