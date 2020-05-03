import QtQuick 2.0 as QQ
import QtQuick.Controls 1.2
import Cavewhere 1.0

QQ.Item {
    id: toolId

    property Renderer3D view
    property CaptureManager manager;

    anchors.fill: parent
    visible: parent !== null

    onVisibleChanged: {
        if(!visible) {
            resetTool()
        }
    }

    function addRectangle(rectangle) {
        if(rectangle.width === 0 || rectangle.height === 0) {
            return;
        }

        var viewObject = captureViewComponentId.createObject()
        viewObject.camera = view.camera
        viewObject.viewport = rectangle
        viewObject.cameraAzimuth = view.turnTableInteraction.azimuth
        viewObject.cameraPitch = view.turnTableInteraction.pitch
        manager.addCaptureViewport(viewObject);

        //Capture must be added to the manager before capturing
        //because of how the request management system works
        viewObject.capture();
    }

    function resetTool() {
        interactionId.deactivate()
        selectionRectangleId.visible = false
        selectionRectangleId.width = 0
        selectionRectangleId.height = 0
        toolButtonId.enabled = true
        state = ""
    }

    QQ.Component {
        id: captureViewComponentId
        CaptureViewport { }
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
        enabled: true

        onClicked: {
            interactionId.activate()
            state = "ACTIVE_STATE"
        }

        states: [
            QQ.State {
                name: "ACTIVE_STATE"
                QQ.PropertyChanges {
                    target: toolButtonId
                    enabled: false
                }

                QQ.PropertyChanges {
                    target: interactionId
                    onHasDraggedChanged: {
                        if(hasDragged) {
                            toolButtonId.state = "CAN_DONE_STATE"
                        }
                    }
                }
            },

            QQ.State {
                name: "CAN_DONE_STATE"
                QQ.PropertyChanges {
                    target: toolButtonId
                    text: "Done"
                    enabled: true
                    onClicked: {
                        var caputerRect = Qt.rect(selectionRectangleId.x,
                                                  selectionRectangleId.y,
                                                  selectionRectangleId.width,
                                                  selectionRectangleId.height)

                        resetTool()

                        toolId.addRectangle(caputerRect)
                    }
                }

                QQ.PropertyChanges {
                    target: interactionId
                    onHasDraggedChanged: {
                        if(!hasDragged) {
                            toolButtonId.state = "ACTIVE_STATE"
                        } else {
                            toolButtonId.state = ""
                        }
                    }
                }
            }
        ]
    }

    SelectionRectangle {
        id: selectionRectangleId
    }

    onViewChanged: {
        view.interactionManager.add(interactionId)
    }

}
