import QtQuick 1.0
import Cavewhere 1.0

Rectangle {
    id: pageId

    property variant currentCave
    property variant currentTrip

    onCurrentCaveChanged: {
        if(currentCave != null) {
            currentTrip = null;
        }
    }

    onCurrentTripChanged: {
        if(currentTrip != null) {
            currentCave = null;
        }
    }


    DataSideBar {
        id: dataSideBar
        width: 300;
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 40
    }

    Image {
        fillMode: Image.TileVertically
        source: "qrc:icons/verticalLine.png"
        height: dataSideBar.anchors.topMargin
        anchors.left: dataSideBar.right
        anchors.leftMargin: -4
        anchors.top: parent.top
       // width: 5
        z:2
    }

    Item {
        id: splitter
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: dataSideBar.right
        anchors.leftMargin: -width / 2

        width:  10
        z:1

        MouseArea {
            id: splitterMouseArea
            anchors.fill: parent

            property int lastMousePosition;

            onPressed: {
                lastMousePosition = mapToItem(null, mouse.x, 0).x;
            }

            states: [
                State {
                    when: splitterMouseArea.pressed
                    PropertyChanges {
                        target: splitterMouseArea

                        onMousePositionChanged: {
                            //Change the databar width
                            var mappedX = mapToItem(null, mouse.x, 0).x;
                            dataSideBar.width +=  mappedX - lastMousePosition
                            lastMousePosition = mappedX
                        }
                    }
                }
            ]
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: dataSideBar.right
        anchors.right: parent.right
        anchors.leftMargin: -1

        AllCavesTabWidget {
            anchors.fill: parent
            opacity: currentCave == null && currentTrip == null ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        CaveTabWidget {
            anchors.fill: parent
            opacity: currentCave != null ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        TripTabWidget {
            anchors.fill: parent
            opacity: currentTrip != null ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }
    }
}
