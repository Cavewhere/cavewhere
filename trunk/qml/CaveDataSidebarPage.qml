import QtQuick 1.0

Rectangle {
    id: page

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
    }

    Column {
        id: staticElements
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        spacing: view.spacing

        TreeRootElement {
            id: cavesElement
            regionVisualDataModel: regionVisualDataModel;
            view: view;
            viewIndex: -1; //The root element
            name: "All Caves"
            iconSource: "icons/caves-64x64.png"
            addChildElementText: "Add Cave"

            onClicked: {
                //This returns make the caveElement disappear
                caveElement.visible = false;
            }
        }

        TreeRootElement {
            id: caveElement
            regionVisualDataModel: regionVisualDataModel;
            view: view;
            viewIndex: -2; //The root element
            addChildElementText: "Add Trip"
            iconSource: "icons/cave-64x64.png"
            visible: false

            anchors.leftMargin: 5
        }

    }

    VisualDataModel {
        id: regionVisualDataModel

        model: regionModel

        delegate:
            Rectangle {
            id: caveDelegate

            property bool selected: index == view.currentIndex;

            height: 26
            anchors.left: parent.left
            anchors.right: parent.right

            color: "#00000000"

            DataSidebarItemTab {
                id: tabBackground
                selected: caveDelegate.selected
            }

            Row {
                id: rowId
                anchors.verticalCenter: tabBackground.verticalCenter
                anchors.left: parent.left;
                anchors.right: dateText.left
                anchors.leftMargin: 7
                //anchors.topMargin: 5
                spacing: 2
                Image {
                    id: icon
                    //anchors.left: parent.left
                    //anchors.leftMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    source: {
                        switch(indexType) {
                        case 0: //Cave
                            return "icons/cave.png"
                        case 1: //Trip
                            return "icons/trip.png"
                        }
                        return ""
                    }
                }

                Text {
                    id: nameText
                    anchors.verticalCenter: parent.verticalCenter
                    //                    anchors.left: icon.right
                    //anchors.leftMargin: 2
                    text: name

                    font.bold: selected
                }


            }

            Text {
                id: dateText
                anchors.verticalCenter: tabBackground.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 5
                text: Qt.formatDateTime(date, "yyyy-MM-dd")
            }

            MouseArea {
                id: caveMouseArea
                anchors.fill: parent;

                hoverEnabled: true;

                Button {
                    id: tripsButton
                    text: "Trips"
                    iconSource: "icons/moreArrow.png"
                    iconOnTheLeft: false
                    opacity: 0.0
                    iconSize: 12
                    height: 18
                    visible: indexType === 0 //Is a cave

                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter

                    onClicked: {
                        //Change to trips
                        caveElement.index = regionVisualDataModel.modelIndex(index)
                        caveElement.name = regionModel.cave(caveElement.index).name
                        currentCave = regionModel.cave(caveElement.index)
                        regionVisualDataModel.rootIndex = caveElement.index
                        view.currentIndex = caveElement.viewIndex
                        caveElement.visible = true;

                    }
                }

                onClicked: {
                    view.currentIndex = index;

                    switch(indexType) {
                    case 0: //Cave
                        currentCave = object;
                        break;
                    case 1: //Trip
                        currentTrip = object;
                        break;
                    }

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

        anchors.leftMargin: caveElement.visible ? 10 : 5

        clip: true
        spacing: -3
        currentIndex: -1

        model:  regionVisualDataModel

        onModelChanged: {
            cavesElement.index = regionVisualDataModel.rootIndex
        }
    }

}
