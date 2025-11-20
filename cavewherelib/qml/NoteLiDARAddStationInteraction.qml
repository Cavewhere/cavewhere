/**************************************************************************
**
**    NoteLiDARAddStationInteraction.qml
**    Interaction that allows tapping the LiDAR view to add stations.
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

Interaction {
    id: addStationInteraction
    objectName: "noteLiDARAddStationInteraction"

    required property NoteLiDAR note
    required property TurnTableInteraction turnTableInteraction

    anchors.fill: parent
    visible: false
    enabled: false

    BaseNoteLiDARStationInteraction {
        id: stationInteraction
    }

    QQ.TapHandler {
        id: tapHandler
        target: addStationInteraction
        acceptedButtons: Qt.LeftButton
        enabled: addStationInteraction.enabled
        onSingleTapped: (eventPoint, button) => {
            if (!addStationInteraction.turnTableInteraction || !addStationInteraction.note) {
                return
            }
            let rayHit = addStationInteraction.turnTableInteraction.pick(eventPoint.position)
            if (rayHit.hit) {
                stationInteraction.addPoint(rayHit.pointModel, addStationInteraction.note)
            }
        }
    }

    HelpBox {
        id: helpBox
        text: "Click to add new LiDAR station"
        visible: addStationInteraction.visible
    }

    QQ.Keys.onEscapePressed: addStationInteraction.deactivate()
}
