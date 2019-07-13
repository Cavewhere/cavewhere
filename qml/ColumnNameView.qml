import QtQuick 2.12
import QtQuick.Layouts 1.0
import Cavewhere 1.0

Item {
    id: root

    property ColumnNameModel model
    property bool isDragging
    property var moveFunction: function(model, oldModel, index, indexOffset, oldIndex) {
        var value = oldModel.get(oldIndex);
        oldModel.remove(oldIndex);
        model.insert(index + indexOffset, value);
    }

    implicitWidth: Math.max(layoutId.width, root.Layout.minimumWidth)
    implicitHeight: layoutId.height

    Row {
        id: layoutId

        move: Transition {
            id: rowTransitionId
            enabled: false
            NumberAnimation { properties: "x"; }
        }

        Repeater {
            id: repeaterId
            model: root.model
            delegate: Item {
                id: delegateId
                property point beginDrag
                property double spacing: 5
                property double dropWidth: 0
                property double dragWidth: rectId.implicitWidth + spacing
                property bool caught: false

                property int currentIndex: index
                property var model: root.model

                implicitWidth: rectLayoutId.implicitWidth
                implicitHeight: rectId.implicitHeight

                states: [
                    State {
                        when: dragAreaId.drag.active

                        PropertyChanges {
                            target: delegateId
                            implicitWidth: 0
                        }
                    }
                ]

                MouseArea {
                    id: dragAreaId
                    anchors.fill: rectLayoutId
                    drag.target: rectLayoutId

                    onPressed: {
                        delegateId.beginDrag = Qt.point(rectLayoutId.x, rectLayoutId.y);
                        rectLayoutId.Drag.hotSpot = Qt.point(mouse.x, mouse.y)
                    }

                    onReleased: {
                        rectLayoutId.Drag.drop()
                        if(!delegateId.caught) {
                            backAnim.stop()
                            backAnimX.from = rectLayoutId.x;
                            backAnimX.to = delegateId.beginDrag.x;
                            backAnimY.from = rectLayoutId.y;
                            backAnimY.to = delegateId.beginDrag.y;
                            backAnim.start()
                        } else {
                            rectLayoutId.x = delegateId.beginDrag.x;
                            rectLayoutId.y = delegateId.beginDrag.y;
                        }

                        delegateId.caught = false
                    }
                }

                ParallelAnimation {
                    id: backAnim
                    SpringAnimation { id: backAnimX; target: rectLayoutId; property: "x"; duration: 100; spring: 2; damping: 0.2 }
                    SpringAnimation { id: backAnimY; target: rectLayoutId; property: "y"; duration: 100; spring: 2; damping: 0.2 }
                }

                Row {
                    id: rectLayoutId

                    spacing: 0
                    Drag.active: dragAreaId.drag.active
                    Drag.source: delegateId
                    Drag.supportedActions: Qt.MoveAction

                    Item {
                        id: leftDropSpaceId
                        height: 1
                        width: insertLeftDropArea.containsDrag ? dropWidth : 0
                    }

                    Rectangle {
                        id: rectId
                        property bool selected: false

                        implicitWidth: textId.width + 6
                        implicitHeight: textId.height + 6

                        color: selected ? "#F39935" : "#E4CC99"
                        border.width: 1
                        border.color: selected ? "#FFEDC7" : "#FFF3D6"

                        radius: 3

                        Text {
                            id: textId
                            anchors.centerIn: parent
                            text: name
                        }
                    }

                    Item {
                        id: rightDropSpaceId
                        height: 1
                        width: insertRightDropArea.containsDrag ? dropWidth : 0
                        visible: insertRightDropArea.containsDrag
                    }
                }

                ColumnNameDropArea {
                    id: insertLeftDropArea
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: leftDropSpaceId.width + rectId.width * 0.5
                    dragArea: dragAreaId
                    delegate: delegateId
                    model: root.model
                    moveFunction: root.moveFunction
                }

                ColumnNameDropArea {
                    id: insertRightDropArea
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: rightDropSpaceId.width + rectId.width * 0.5
                    dragArea: dragAreaId
                    delegate: delegateId
                    model: root.model
                    indexOffset: 1 //Needs to be 1 because when inserting it needs to be right of the current item
                    moveFunction: root.moveFunction
                }
            }
        }
    }

    ColumnNameDropArea {

        anchors.left: layoutId.right
        anchors.right: parent.right

        insertIndex: repeaterId.count
        implicitHeight: layoutId.height

        model: root.model
        moveFunction: root.moveFunction
    }
}
