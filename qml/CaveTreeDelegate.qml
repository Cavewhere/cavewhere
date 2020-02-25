/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0

FocusScope {

    id: focusScope

    height: 26
    anchors.left: parent.left
    anchors.right: parent.right

    QQ.Rectangle {
        id: caveDelegate

        property bool selected: index === view.currentIndex;

        anchors.fill: parent

        color: "#00000000"

        DataSidebarItemTab {
            id: tabBackground
            selected: caveDelegate.selected
        }

        RemoveAskBox {
            id: removeChallengeId
            removeName: name
            onRemove: {
                var itemIndex = regionVisualDataModel.modelIndex(indexToRemove);
                regionModel.removeIndex(itemIndex);
            }
        }

        QQ.MouseArea {

            id: caveMouseArea
            anchors.fill: parent;
            hoverEnabled: true;

            QQ.Row {
                id: rowId
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left;
                anchors.right: dateTextInput.left
                anchors.leftMargin: 7
                //anchors.topMargin: 5
                spacing: 2

                QQ.Rectangle {
                    id: removeHolder
                    width:  16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter

                    RemoveButton {
                        id: removeButton
                        anchors.fill: parent
                        visible: caveDelegate.selected

                        onClicked: removeChallengeId.show()

                    }



                }

                QQ.Image {
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
                QQ.State {
                    name: "showTripButton";
                    when: caveMouseArea.containsMouse | caveDelegate.selected;
                    QQ.PropertyChanges { target: tripsButton; opacity: 1.0; }
                }
            ]

            transitions: [
                 QQ.Transition {
                    QQ.NumberAnimation { target: tripsButton; properties: "opacity"; duration: 100 }
                }
            ]
        }

        states: [

            QQ.State {
                name: "editDate";
                QQ.PropertyChanges {
                    target:  dateTextInput
                    visible: true
                    focus: true
                }

                QQ.PropertyChanges {
                    target: dateText
                    visible: false
                }
            }

        ]
    }
}
