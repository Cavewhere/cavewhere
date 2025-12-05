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

    StationImage {
        id: stationImage

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

    StationDoubleClickTextInput {
        id: stationName

        anchors.verticalCenter: stationImage.verticalCenter
        anchors.left: stationImage.right
        pointItem: noteStationId
        onFinishedEditting: (newText) => {
            noteStationId.scrap.setStationData(Scrap.StationName, noteStationId.pointIndex, newText);
            text = newText;
            noteStationId.forceActiveFocus();
        }
    }
}
