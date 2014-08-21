import QtQuick 2.0
import QtQuick.Controls 1.2
import Cavewhere 1.0

Item {
    id: toolId

    property GLTerrainRenderer view
    property CaptureManager manager;

    anchors.fill: parent
    visible: parent !== null

    function addRectangle(rectangle) {
        console.log("I get here:" + rectangle)
        var viewObject = captureViewComponentId.createObject()
        viewObject.view = view
        viewObject.viewport = rectangle
        viewObject.capture();
        manager.addViewportCapture(viewObject);
    }

    Component {
        id: captureViewComponentId
        ViewportCapture { }
    }

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

                      toolId.addRectangle(caputerRect)
                      expantionAreaId.state = "EXPAND_RIGHT"
                  }
              }
          }
       ]
    }

    SelectionRectangle {
        id: selectionRectangleId
    }

    Item {
        id: expantionAreaId

        property int hiddenWidth: 100

        anchors.fill: parent

        visible: manager.numberOfCaptures

        Rectangle {
            color: "blue"
            opacity: .4
            anchors.fill: parent
        }

        MouseArea {
            id: renderingSelectionArea
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            hoverEnabled: true
            onClicked: {
                expantionAreaId.state = "EXPAND_LEFT"
            }

            Behavior on width {
                   NumberAnimation { duration: 100 }
               }

            Rectangle {
                id: selectionRectangle1
                anchors.fill: parent
                color: "#6574FF"
                opacity: .25
                visible: renderingSelectionArea.containsMouse
            }
        }

        Rectangle {
            anchors.left: renderingSelectionArea.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            color: "red"

            MouseArea {
                id: selectExportViewId
                anchors.fill: parent

                onEnabledChanged: {
                    console.log("Enabled change:" + enabled)
                }

                onClicked: {
                    expantionAreaId.state = "EXPAND_RIGHT"
                }
            }
        }

        states: [
            State {
                name: "EXPAND_LEFT"
                PropertyChanges {
                    target: renderingSelectionArea
                    width: expantionAreaId.width - expantionAreaId.hiddenWidth
                    enabled: false
                }

                PropertyChanges {
                    target: selectExportViewId
                    enabled: true
                }

            },

            State {
                name: "EXPAND_RIGHT"

                PropertyChanges {
                    target: renderingSelectionArea
                    width: expantionAreaId.hiddenWidth
                    enabled: true
                }

                PropertyChanges {
                    target: selectExportViewId
                    enabled: false
                }
            },

            State {
                name: "CUSTOM"
            }
        ]
    }


    onViewChanged: {
        view.interactionManager.add(interactionId)
    }

}
