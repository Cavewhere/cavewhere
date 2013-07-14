/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

Rectangle {
    id: cavePageArea

    property Cave currentCave: null

    anchors.fill: parent;


    DoubleClickTextInput {
        id: caveNameText
        text: currentCave != null ? currentCave.name : "" //From c++
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        font.bold: true
        font.pointSize: 20

        onFinishedEditting: {
            currentCave.name = newText
        }
    }

    UsedStationsWidget {
        id: usedStationWidgetId

        anchors.left: lengthDepthContainerId.right
        anchors.top: caveNameText.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 5
        anchors.bottomMargin: 5
        width: 250
    }

    Rectangle {
        id: lengthDepthContainerId

        color: "lightgray"
        anchors.left: parent.left
        anchors.top: usedStationWidgetId.top
        anchors.margins: 5

        width: caveLengthAndDepthId.width + 10
        height: caveLengthAndDepthId.height + 10

        CaveLengthAndDepth {
            id: caveLengthAndDepthId

            anchors.centerIn: parent

            currentCave: cavePageArea.currentCave
        }
    }
}
