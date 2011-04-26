import QtQuick 1.0

Rectangle {
    id: rectangle1
    //    width: 100
    //    height: 62

    //  border.width: 1

    anchors.fill: parent
    anchors.bottomMargin: 1;
    anchors.rightMargin: 1;

    Image {
        id: splitter
        fillMode: Image.TileVertically
        source: "icons/verticalLine.png"
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: parent.right
        anchors.leftMargin: -3
        //        z:1
    }

    Column {
        id: staticElements
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        anchors.leftMargin: 3
        anchors.rightMargin: 0

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            height:  30
            color: "#00000000"

            onHeightChanged: {
                console.log("Height:" + children.height);
            }

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "Caves"

            }

            Button {
                id: addCaveButton

                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.verticalCenter: parent.verticalCenter

                text: "Add Cave"
                iconSource: "icons/plus.png"
            }
        }
    }

    VisualDataModel {
        id: regionVisualDataModel

        model: regionModel

        delegate:
            Rectangle {
            id: caveDelegate

            property bool selected: index == view.currentIndex;

            height: 35
            anchors.left: parent.left
            anchors.right: parent.right

            color: "#00000000"

            Rectangle {
                id: tabBackground
                width:  parent.width
                height: parent.height
                x: selected ? parent.x : parent.x + parent.width
                color: "#00000000"

                StandardBorder {
                    anchors.rightMargin: -7
                }

                Behavior on x {
                    NumberAnimation {
                        duration: 100;
                    }
                }
            }

            Text {
                id: nameText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 7
                text: name

                font.bold: selected
            }

            MouseArea {
                id: caveMouseArea
                anchors.fill: parent;

                hoverEnabled: true;

                Button {
                    id: tripsButton
                    text: "Trips"
                    opacity: 0.0

                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                }

                onClicked: {
                    view.currentIndex = index;
                }
            }

            states: [
                State {
                    name: "showTripButton";
                    when: caveMouseArea.containsMouse | caveDelegate.selected;
                    PropertyChanges { target: tripsButton; opacity: 1.0; }
                }
            ]

            transitions: [
                Transition {
                    NumberAnimation { target: tripsButton; properties: "opacity"; duration: 100 }
                }
            ]
        }
    }

    ListView {
        id: view
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: staticElements.bottom
        //anchors.rightMargin: -3
        clip: true

        model:  regionVisualDataModel
    }


}
