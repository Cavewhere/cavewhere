import QtQuick

Item {
    id: positioner

    default property alias conentData: contentId.data

    Item {
        id: contentId
        scale: positioner.parent ? 1.0 / positioner.parent.scale : 1.0
        rotation: positioner.parent ? -positioner.parent.rotation : 0.0
    }
}
