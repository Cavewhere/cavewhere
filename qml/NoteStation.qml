import QtQuick 2.0
import Cavewhere 1.0

/**
    This class holds the geometry and interaction for a station on a page of notes
  */
ScrapPointItem {
    id: noteStation

//    property ScrapStationView stationView;
//    property ScrapItem scrapItem;
//    property Scrap scrap;
//    property int pointIndex;
//    property bool selected: false

    function updateItem() {
        if(scrap !== null) {
            stationName.text = scrap.stationData(Scrap.StationName, pointIndex)
            var position = scrap.stationData(Scrap.StationPosition, pointIndex);
            position3D = Qt.vector3d(position.x, position.y, 0.0);
        }
    }

    function selectStation() {
        noteStation.selected = true
        scrapItem.selected = true
    }

    width: 2
    height: 2
    z: 2

    focus: selected

    onScrapChanged: updateItem()
    onPointIndexChanged: updateItem()

    onSelectedChanged: {
        if(selected) {
            parentView.selectedItemIndex = pointIndex;
            scrapItem.selected = true
            forceActiveFocus();
        }
    }

    Keys.onDeletePressed: {
        scrap.removeStation(pointIndex);
    }

    Keys.onPressed: {
        if(event.key === 0x01000003) { //Backspace key = Qt::Key_Backspace
            scrap.removeStation(pointIndex);
        }
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

            onClicked: selectStation()

            onReleased: ({ })

            onPressed: {
                lastPoint = Qt.point(mouse.x, mouse.y);
                ignoreLength = false;
            }

            onPositionChanged: {
                //Make sure the mouse has move at least three pixel from where it's started
                var length = Math.sqrt(Math.pow(lastPoint.x - mouse.x, 2) + Math.pow(lastPoint.y - mouse.y, 2));
                if(length > 3 || ignoreLength) {
                    ignoreLength = true
                    var parentCoord = mapToItem(parentView, mouse.x, mouse.y);
                    var transformer = parentView.transformUpdater;
                    var noteCoord = transformer.mapFromViewportToModel(Qt.point(parentCoord.x, parentCoord.y));
                    scrap.setStationData(Scrap.StationPosition, pointIndex, Qt.point(noteCoord.x, noteCoord.y));
                }
            }

            onDoubleClicked: {
                stationName.openEditor();
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
            scrap.setStationData(Note.StationName, pointIndex, newText);
            text = newText;
            noteStation.forceActiveFocus();
        }

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onClicked: selectStation()
        }

    }


}
