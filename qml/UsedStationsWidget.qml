/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import QtQuick.Controls 1.0
import Cavewhere 1.0

Rectangle {
    id: usedStationsArea

    border.width: 1
    border.color: "#A7A7A7"

    ScrollView {
        anchors.rightMargin: 2
        anchors.bottomMargin: 2
        anchors.leftMargin: 2
        anchors.top: lineId.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.topMargin: 0
        ListView {
            id: usedStationsView


            clip: true
            model: usedStationsModel.usedStations;

            delegate: Text {
                id: textId

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 3
                anchors.rightMargin: 3

                text: modelData
            }

            UsedStationTaskManager {
                id: usedStationsModel;
                cave: currentCave != null ? currentCave : null
                listenToChanges: parent.visible
            }
        }
    }

    BreakLine {
        id: lineId
        anchors.top: usedStationsLabel.bottom
    }

    Text {
        id: usedStationsLabel
        text: "Used Stations"
        font.bold: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 4
        font.pixelSize:14
    }
}
