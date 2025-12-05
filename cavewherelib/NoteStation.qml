/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

/**
    This class holds the geometry and interaction for a station on a page of notes
  */
ScrapPointItem {
    id: noteStationId
    objectName: "station" + stationName.text

    function updateItem() {
        if(scrap !== null) {
            stationName.text = scrap.stationData(Scrap.StationName, pointIndex)
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
        source: "qrc:icons/svg/station.svg"

        sourceSize: Qt.size(18, 18)

        width: sourceSize.width
        height: sourceSize.height

        ScrapPointMouseArea {
            id: stationMouseArea
            objectName: "stationIcon"
            anchors.fill: parent

            scrapItem: noteStationId.scrapItem
            parentView: noteStationId.parentView
            onPointSelected: noteStationId.select();
            onPointMoved: (noteCoord) => {

                              noteStationId.scrap.setStationData(Scrap.StationPosition,
                                                                            noteStationId.pointIndex,
                                                                            Qt.point(noteCoord.x, noteCoord.y));
                          }
            onDoubleClicked: stationName.openEditor();
        }
    }

    DoubleClickTextInput {
        id: stationName

        style: Text.Outline
        styleColor: Theme.text
        color: Theme.textInverse
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

        onClicked: noteStationId.select()
    }
}
