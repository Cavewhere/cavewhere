import QtQuick 1.0
import Cavewhere 1.0

Rectangle {
    id: pageId

    /**
      This is used to test if the current Index in the dataSideBar is of type type.

      If the type is equal to the current index's type, this returns true, otherwise false.
      */
    function currentIndexIsType(type) {
        var index = dataSideBar.caveSidebar.currentIndex
        var indexsType = regionModel.data(index, RegionTreeModel.TypeRole);
        return (indexsType == type);
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
        id: caveTabs
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: dataSideBar.right
        anchors.right: parent.right
        anchors.leftMargin: -1

        AllCavesTabWidget {
            anchors.fill: parent
            opacity: currentIndexIsType(RegionTreeModel.RegionType) ? 1.0 : 0.0
                Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        CaveTabWidget {
            anchors.fill: parent
            opacity: currentIndexIsType(RegionTreeModel.CaveType) ? 1.0 : 0.0
            currentCave: {
                if(currentIndexIsType(RegionTreeModel.CaveType)) {
                    var index = dataSideBar.caveSidebar.currentIndex
                    return regionModel.data(index, RegionTreeModel.ObjectRole);
                }
                return null;
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        TripTabWidget {
            anchors.fill: parent
            opacity: currentIndexIsType(RegionTreeModel.TripType) ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }
    }
}
