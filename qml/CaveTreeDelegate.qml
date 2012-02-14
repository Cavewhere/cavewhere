import QtQuick 1.1
import Cavewhere 1.0

FocusScope {

    id: focusScope

    height: 26
    anchors.left: parent.left
    anchors.right: parent.right

    Rectangle {
        id: caveDelegate

        property bool selected: index === view.currentIndex;

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
                anchors.right: dateTextInput.left
                anchors.leftMargin: 7
                //anchors.topMargin: 5
                spacing: 2

                Rectangle {
                    id: removeHolder
                    width:  16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter

                    RemoveButton {
                        id: removeButton
                        anchors.fill: parent
                        visible: caveDelegate.selected

                        onClicked: {
                            var itemIndex = regionVisualDataModel.modelIndex(index);
                            regionModel.removeIndex(itemIndex);
                        }
                    }
                }

                Image {
                    id: icon
                    //anchors.left: parent.left
                    //anchors.leftMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    source: iconSource
                }

                DoubleClickTextInput {
                    id: nameTextInput
                    text: name
                    anchors.verticalCenter: parent.verticalCenter
                    font.bold: caveDelegate.selected

                    onStartedEditting: {
                        view.currentIndex = index
                        view.focus = true
                        focusScope.focus = true
                    }

                    onFinishedEditting: {
                        var currentIndex = regionVisualDataModel.modelIndex(index);
                        regionModel.setData(currentIndex, newText, RegionTreeModel.NameRole)
                    }
                }
            }

            DoubleClickTextInput {
                id: dateTextInput
                anchors.verticalCenter: parent.verticalCenter; //tabBackground.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 5
                text: Qt.formatDateTime(date, "yyyy-MM-dd")

                onStartedEditting: {
                    view.currentIndex = index
                    view.focus = true
                    focusScope.focus = true
                }

                onFinishedEditting: {
                    var currentIndex = regionVisualDataModel.modelIndex(index);
                    regionModel.setData(currentIndex, newText, RegionTreeModel.DateRole)
                }
            }


            Button {
                id: tripsButton
                text: "Trips"
                iconSource: "qrc:icons/moreArrow.png"
                iconOnTheLeft: false
                opacity: 0.0
                iconSize: Qt.size(10, 10)
                height: 18
                visible: indexType === RegionTreeModel.CaveType //Is a cave

                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.verticalCenter: parent.verticalCenter

                onClicked: {
                    //Change to trips
                    //Update the page's current index
                    regionVisualDataModel.rootIndex = regionVisualDataModel.modelIndex(index);
                    view.currentIndex = caveElement.viewIndex

                }


            }

            onClicked: {
                var modelIndex = regionModel.index(index, 0, regionVisualDataModel.rootIndex);
                regionSelectionModel.setCurrentIndex(modelIndex, 0x0010); //select current
                view.currentIndex = index;
                view.focus = true;
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
                name: "editDate";
                PropertyChanges {
                    target:  dateTextInput
                    visible: true
                    focus: true
                }

                PropertyChanges {
                    target: dateText
                    visible: false
                }
            }

        ]
    }
}
