import QtQuick 1.0
import Cavewhere 1.0

/**
    This class holds the geometry and interaction for a station on a page of notes
  */
Positioner3D {
    id: noteStation

    property ScrapStationView stationView;
    property ScrapItem scrapItem;
    property Scrap scrap;
    property int stationId;
    property bool selected: false

    function updateItem() {
        if(scrap !== null) {
            stationName.text = scrap.stationData(Scrap.StationName, stationId)
            var position = scrap.stationData(Scrap.StationPosition, stationId);
//            console.log("stationId:" + stationId + " SCrap:" + scrap + " Position:" + position.x + " " + position.y);
            position3D = Qt.vector3d(position.x, position.y, 0.0);
        }
    }

    width: 2
    height: 2
    z: 2

    focus: selected

    onScrapChanged: updateItem()
    onStationIdChanged: updateItem()

    onSelectedChanged: {
        if(selected) {
            stationView.selectedStationIndex = stationId;
            scrapItem.selected = true
            forceActiveFocus();
        }
    }

    Keys.onDeletePressed: {
        console.log("Delete pressed!");
        scrap.removeStation(stationId);
    }

    Rectangle {
        id: selectedBackground

        anchors.left: stationImage.left
        anchors.top: stationImage.top
        anchors.right: stationName.right
        anchors.bottom: stationName.bottom
        anchors.margins: -2

        radius: 4

        color: "#418CFF"
        border.width: 1
        border.color: "white"

        opacity: .5

        visible: selected && scrapItem.selected
    }

    Image {
        id: stationImage
        anchors.centerIn: parent
        source: "qrc:icons/stationGood.png"

        width: sourceSize.width
        height: sourceSize.height


        MouseArea {
            id: stationMouseArea
            anchors.fill: parent

            property variant lastPoint;
            property bool ignoreLength;

            onClicked: {
                console.log("On selection");
                noteStation.selected = true
                scrapItem.selected = true
            }

            onReleased: ({ })

            onPressed: {
                lastPoint = Qt.point(mouse.x, mouse.y);
                ignoreLength = false;
            }

            onMousePositionChanged: {
                //Make sure the mouse has move at least three pixel from where it's started
                var length = Math.sqrt(Math.pow(lastPoint.x - mouse.x, 2) + Math.pow(lastPoint.y - mouse.y, 2));
                if(length > 3 || ignoreLength) {
                    ignoreLength = true
                    var parentCoord = mapToItem(stationView, mouse.x, mouse.y);
                    var transformer = stationView.transformUpdater;
                    var noteCoord = transformer.mapFromViewportToModel(Qt.point(parentCoord.x, parentCoord.y));
                    scrap.setStationData(Scrap.StationPosition, stationId, Qt.point(noteCoord.x, noteCoord.y));
                }
            }
        }

//        Rectangle {
//            anchors.fill: parent
//            color: "red"
//        }
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

        onFinishedEditting: {
            scrap.setStationData(Note.StationName, stationId, newText);
            text = newText;
        }
    }




}
