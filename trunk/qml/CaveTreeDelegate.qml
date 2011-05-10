import QtQuick 1.0
import Cavewhere 1.0

FocusScope {

    id: focusScope

    height: 26
    anchors.left: parent.left
    anchors.right: parent.right

    Rectangle {
        id: caveDelegate

        property bool selected: index == view.currentIndex;

        anchors.fill: parent

        color: "#00000000"

        DataSidebarItemTab {
            id: tabBackground
            selected: caveDelegate.selected
        }

        MouseArea {

            id: caveMouseArea
            anchors.fill: parent;
            hoverEnabled: true;

            Row {
                id: rowId
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left;
                anchors.right: dateText.left
                anchors.leftMargin: 7
                //anchors.topMargin: 5
                spacing: 2

                Rectangle {
                    id: removeHolder
                    width:  16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter

                    IconButton {
                        id: removeButton
                        iconSource: "icons/minusCircleLight.png"
                        hoverIconSource: "icons/minusCircleDark.png"
                        sourceSize: Qt.size(15, 15);

                        anchors.fill: parent
                        visible: caveDelegate.selected

                        onClicked: {
                            region.removeCave(index)
                        }
                    }
                }

                Image {
                    id: icon
                    //anchors.left: parent.left
                    //anchors.leftMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    source: {
                        switch(indexType) {
                        case RegionTreeModel.Cave: //Cave
                            return "icons/cave.png"
                        case RegionTreeModel.Trip: //Trip
                            return "icons/trip.png"
                        }
                        return ""
                    }
                }

                Text {
                    id: nameText
                    anchors.verticalCenter: parent.verticalCenter
                    text: name
                    font.bold: caveDelegate.selected
                }


                TextInput {
                    id: nameTextEdit

                    selectByMouse: true;
                    activeFocusOnPress: false
                    visible: false
                    focus: false

                    onFocusChanged: {
                        console.log("Focus changed: " + (focus) + " " + (index))
                        if(!activeFocus) {
                            caveDelegate.state = ""
                            var currentIndex = regionVisualDataModel.modelIndex(index);
                            regionModel.setData(currentIndex, text, RegionTreeModel.NameRole)
                        }
                    }

                    Keys.onPressed: {
                        if(event.key == Qt.Key_Return) {
                            focus = false;
                            event.accepted = true
                        }
                    }
                }
            }

            Text {
                id: dateText
                anchors.verticalCenter: parent.verticalCenter; //tabBackground.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 5
                text: Qt.formatDateTime(date, "yyyy-MM-dd")
            }




            Button {
                id: tripsButton
                text: "Trips"
                iconSource: "icons/moreArrow.png"
                iconOnTheLeft: false
                opacity: 0.0
                iconSize: Qt.size(10, 10)
                height: 18
                visible: indexType === 0 //Is a cave

                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.verticalCenter: parent.verticalCenter

                onClicked: {
                    //Change to trips
                    caveElement.index = regionVisualDataModel.modelIndex(index)
                    caveElement.name = regionModel.cave(caveElement.index).name
                    currentCave = regionModel.cave(caveElement.index) //Update the global object
                    regionVisualDataModel.rootIndex = caveElement.index
                    view.currentIndex = caveElement.viewIndex
                    caveElement.visible = true;
                }


            }

            onClicked: {
                view.currentIndex = index;
                view.focus = true;
                switch(indexType) {
                case RegionTreeModel.Cave: //Cave
                    currentCave = object;
                    break;
                case RegionTreeModel.Trip: //Trip
                    currentTrip = object;
                    break;
                }
            }

            onDoubleClicked: {
                console.log("Double clicked")
                caveDelegate.state = "editName"
                view.focus = true
                nameTextEdit.selectAll();
                nameTextEdit.text = name

                //nameTextEdit.visible = true
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

        states: [
            State {
                name: "editName";
                PropertyChanges {
                    target: nameTextEdit
                    visible: true
                    focus: true
                }

                PropertyChanges {
                    target: nameText
                    visible: false
                }

                //                PropertyChanges {
                //                    target: caveMouseArea
                //                    enabled: false
                //                }

            }

        ]
    }
}
