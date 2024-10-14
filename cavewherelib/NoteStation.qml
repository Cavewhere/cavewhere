/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import cavewherelib

/**
    This class holds the geometry and interaction for a station on a page of notes
  */
ScrapPointItem {
    id: noteStationId

    function updateItem() {
        if(scrap !== null) {
            stationName.text = scrap.stationData(Scrap.StationName, pointIndex)
            var position = scrap.stationData(Scrap.StationPosition, pointIndex);
            position3D = Qt.vector3d(position.x, position.y, 0.0);
        }
    }

    /**
      When call this opens the editor for this note station`
      */
    function startEditting() {
        stationName.openEditor();
    }

    width: 2
    height: 2

    onScrapChanged: updateItem()
    onPointIndexChanged: updateItem()

    QQ.Keys.onDeletePressed: {
        scrap.removeStation(pointIndex);
    }

    QQ.Keys.onPressed: (event) => {
        if(event.key === Qt.Key_Backspace) {
            scrap.removeStation(pointIndex);
        }
    }

    SelectedBackground {
        id: selectedBackground

        anchors.left: stationImage.left
        anchors.top: stationImage.top
        anchors.right: stationName.right
        anchors.bottom: stationName.bottom

        visible: noteStationId.selected && noteStationId.scrapItem.selected
    }

    QQ.Image {
        id: stationImage
        anchors.centerIn: parent
        source: "qrc:icons/stationGood.png"

        width: sourceSize.width
        height: sourceSize.height

        ScrapPointMouseArea {
            id: stationMouseArea
            anchors.fill: parent

            onPointSelected: noteStationId.select();
            onPointMoved: (noteCoord) => noteStationId.scrap.setStationData(Scrap.StationPosition,
                                                                            noteStationId.pointIndex,
                                                                            Qt.point(noteCoord.x, noteCoord.y));
            onDoubleClicked: stationName.openEditor();
        }
    }

    DoubleClickTextInput {
        id: stationName

        style: Text.Outline
        styleColor: "#FFFFFF"
        font.bold: true

        //So we don't add new station when we click on the station
        acceptMousePress: true

        anchors.verticalCenter: stationImage.verticalCenter
        anchors.left: stationImage.right

        onFinishedEditting: (newText) => {
            noteStationId.scrap.setStationData(Scrap.StationName, noteStationId.pointIndex, newText);
            text = newText;
            noteStationId.forceActiveFocus();
        }

        QQ.MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onClicked: noteStationId.select()
        }
    }
}
