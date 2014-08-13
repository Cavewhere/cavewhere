import QtQuick 2.0
import QtQuick.Controls 1.2

Item {
    id: toolId

    property GLTerrainRenderer view

    signal capturedRectangle(var rectangle)

    anchors.fill: parent
    visible: parent !== null



    SelectExportAreaInteraction {
        id: interactionId
        anchors.fill: parent
        selectionRectangle: selectionRectangleId
    }

    Button {
        id: toolButtonId
       anchors.top: parent.top
       anchors.right: parent.right
       text: "Select Area"

       onClicked: {
           interactionId.activate()
           state = "ACTIVE_STATE"
       }

       states: [
            State {
               name: "ACTIVE_STATE"
               PropertyChanges {
                   target: toolButtonId
                   enabled: false
               }
           },

           State {
              name: "CAN_DONE_STATE"
              when: interactionId.hasDragged
              PropertyChanges {
                  target: toolButtonId
                  text: "Done"
                  enabled: true
                  onClicked: {
                      var caputerRect = Qt.rect(selectionRectangleId.x,
                                                selectionRectangleId.y,
                                                selectionRectangleId.width,
                                                selectionRectangleId.height)

                      interactionId.deactivate()
                      selectionRectangleId.visible = false
                      selectionRectangleId.width = 0
                      selectionRectangleId.height = 0

                      state = ""

                      capturedRectangle(caputerRect)
                  }
              }
          }
       ]
    }

    Rectangle {
        id: selectionRectangleId

        property int mouseBuffer: 5

        opacity: 0.30
        color: "#11B0FF"
        border.width: 1
        border.color: "#676767"

        onVisibleChanged: {
            console.log("Visible changed:" + visible)
        }

        MouseArea {
            id: leftSide
            width: selectionRectangleId.mouseBuffer
            anchors.horizontalCenter: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: selectionRectangleId.mouseBuffer
            anchors.bottomMargin: selectionRectangleId.mouseBuffer
            hoverEnabled: true
            cursorShape: Qt.SplitHCursor
            onPressed: { }
            onPositionChanged: {
                if(pressed) {
                    var leftSide = mapToItem(toolId, mouse.x, 0).x
                    var rightSide = selectionRectangleId.x + selectionRectangleId.width;
                    var width = rightSide - leftSide
                    if(width >= selectionRectangleId.mouseBuffer) {
                        selectionRectangleId.x = leftSide
                        selectionRectangleId.width = width
                    }
                }
            }
        }

        MouseArea {
            id: rightSide
            width: selectionRectangleId.mouseBuffer
            anchors.horizontalCenter: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: selectionRectangleId.mouseBuffer
            anchors.bottomMargin: selectionRectangleId.mouseBuffer
            hoverEnabled: true
            cursorShape: Qt.SplitHCursor
            onPressed: { }
            onPositionChanged: {
                if(pressed) {
                    var leftSide = selectionRectangleId.x;
                    var rightSide = mapToItem(toolId, mouse.x, 0).x
                    var width = rightSide - leftSide
                    if(width >= selectionRectangleId.mouseBuffer) {
                        selectionRectangleId.width = width
                    }
                }
            }
        }

        MouseArea {
            id: topSide
            height: selectionRectangleId.mouseBuffer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.top
            anchors.leftMargin: selectionRectangleId.mouseBuffer
            anchors.rightMargin: selectionRectangleId.mouseBuffer
            hoverEnabled: true
            cursorShape: Qt.SplitVCursor
            onPressed: { }
            onPositionChanged: {
                if(pressed) {
                    var topSide = mapToItem(toolId, 0, mouse.y).y
                    var bottomSide = selectionRectangleId.y + selectionRectangleId.height
                    var height = bottomSide - topSide
                    if(height >= selectionRectangleId.mouseBuffer) {
                        selectionRectangleId.y = topSide
                        selectionRectangleId.height = height
                    }
                }
            }
        }

        MouseArea {
            id: bottomSide
            height: selectionRectangleId.mouseBuffer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.bottom
            anchors.leftMargin: selectionRectangleId.mouseBuffer
            anchors.rightMargin: selectionRectangleId.mouseBuffer
            hoverEnabled: true
            cursorShape: Qt.SplitVCursor
            onPressed: { }
            onPositionChanged: {
                if(pressed) {
                    var topSide = selectionRectangleId.y
                    var bottomSide = mapToItem(toolId, 0, mouse.y).y
                    var height = bottomSide - topSide
                    if(height >= selectionRectangleId.mouseBuffer) {
                        selectionRectangleId.height = height
                    }
                }
            }
        }

    }

    onViewChanged: {
        console.log("Added! " + view)
        view.interactionManager.add(interactionId)
    }

}
