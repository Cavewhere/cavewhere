import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: toolId
    objectName: "selectionExportAreaTool"

    required property GLTerrainRenderer view
    required property CaptureManager manager;

    anchors.fill: parent
    visible: parent !== null

    // onVisibleChanged: {
    //     if(!visible) {
    //         // quoteBoxId.visible = true
    //         resetTool()
    //     }
    // }

    function activate() {
        toolButtonId.state = "INIT"
    }

    function addRectangle(rectangle) {
        if(rectangle.width === 0 || rectangle.height === 0) {
            return;
        }

        var viewObject = captureViewComponentId.createObject()
        viewObject.view = toolId.view.renderer
        viewObject.sceneManager = RootData.regionSceneManager
        viewObject.viewport = rectangle
        viewObject.cameraAzimuth = view.turnTableInteraction.azimuth
        viewObject.cameraPitch = view.turnTableInteraction.pitch
        viewObject.capture();
        manager.addCaptureViewport(viewObject);
    }

    // function resetTool() {
    //     interactionId.deactivate()
    //     // toolId.visible = false
    //     // selectionRectangleId.visible = false
    //     // selectionRectangleId.width = 0
    //     // selectionRectangleId.height = 0
    //     // toolButtonId.enabled = true
    //     // toolButtonId.state = "INIT"
    // }

    QQ.Component {
        id: captureViewComponentId
        CaptureViewport { }
    }

    SelectExportAreaInteraction {
        id: interactionId
        objectName: "selectAreaInteraction"
        anchors.fill: parent
        selectionRectangle: selectionRectangleId
    }

    QC.Button {
        id: toolButtonId
        objectName: "selectionToolButton"

        anchors.top: parent.top
        anchors.left: parent.left
        text: " Select Area"
        enabled: true
        icon.source: "qrc:/twbs-icons/icons/box-arrow-down-right.svg"
        state: "DEACTIVE"
        visible: true

        states: [
            QQ.State {
                name: "DEACTIVE"
                QQ.PropertyChanges {
                    toolId {
                        visible: false
                    }
                }
            },

            QQ.State {
                name: "INIT"
                QQ.PropertyChanges {
                    quoteBoxId{
                        visible: true
                    }

                    toolId {
                        visible: true
                    }

                    toolButtonId {
                        enabled: true
                        visible: true
                        onClicked: {
                            interactionId.activate()
                            toolButtonId.state = "ACTIVE_STATE"
                        }
                    }

                    selectionRectangleId {
                        visible: false
                        width: 0
                        height:  0
                    }
                    toolButtonId {
                        enabled: true
                        visible: true
                    }
                }
            },

            QQ.State {
                name: "ACTIVE_STATE"
                QQ.PropertyChanges {
                    toolId {
                        visible: true
                    }

                    toolButtonId {
                        enabled: false
                        visible: true
                    }

                    selectionRectangleId {
                        visible: true
                    }

                    interactionId {
                        onHasDraggedChanged: {
                            if(hasDragged) {
                                toolButtonId.state = "CAN_DONE_STATE"
                            }
                        }
                    }

                    clickAndDragHelpBoxId {
                        visible: true
                    }

                    quoteBoxId {
                        visible: false
                    }
                }
            },

            QQ.State {
                name: "CAN_DONE_STATE"
                QQ.PropertyChanges {
                    toolId {
                        visible: true
                    }

                    toolButtonId {
                        text: " Done"
                        icon.source: "qrc:/twbs-icons/icons/check-lg.svg"
                        enabled: true
                        visible: true
                        onClicked: {
                            var caputerRect = Qt.rect(selectionRectangleId.x,
                                                      selectionRectangleId.y,
                                                      selectionRectangleId.width,
                                                      selectionRectangleId.height)


                            toolId.addRectangle(caputerRect)

                            RootData.pageSelectionModel.back()

                            interactionId.deactivate()
                            toolButtonId.state = "DEACTIVE"
                        }
                    }

                    interactionId {
                        onHasDraggedChanged: {
                            if(hasDragged) {
                                toolButtonId.state = "INIT"
                            } else {
                                toolButtonId.state = "ACTIVE_STATE"
                            }
                        }
                    }

                    clickAndDragHelpBoxId {
                        visible: false
                    }

                    quoteBoxId {
                        visible: false
                    }
                }
            }
        ]
    }

    HelpQuoteBox {
        id: quoteBoxId
        objectName: "quoteBox"
        pointAtObject: toolButtonId
        pointAtObjectPosition: Qt.point(toolButtonId.width / 2.0, toolButtonId.height)
        text: "Select the area to add a map layer"
    }

    HelpBox {
        id: clickAndDragHelpBoxId
        objectName: "clickAndDragHelpBox"
        text: "Click and drag to create a map layer"
        visible: false
    }

    SelectionRectangle {
        id: selectionRectangleId
    }

    onViewChanged: {
        view.interactionManager.add(interactionId)
    }

}
