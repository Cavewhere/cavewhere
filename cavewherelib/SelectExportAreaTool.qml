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
        viewObject.view = toolId.view.renderer
        viewObject.sceneManager = RootData.regionSceneManager
        viewObject.viewport = rectangle
        viewObject.cameraAzimuth = view.turnTableInteraction.azimuth
        viewObject.cameraPitch = view.turnTableInteraction.pitch
        viewObject.capture();
        manager.addCaptureViewport(viewObject);
    }

    function resetTool() {
        interactionId.deactivate()
        toolId.visible = false
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
        // icon.width: 10
        // icon.height: 10

        onClicked: {
            interactionId.activate()
            state = "ACTIVE_STATE"
        }

        states: [
            QQ.State {
                name: "ACTIVE_STATE"
                QQ.PropertyChanges {
                    toolButtonId {
                        enabled: false
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
                    toolButtonId {
                        text: " Done"
                        icon.source: "qrc:/twbs-icons/icons/check-lg.svg"
                        enabled: true
                        onClicked: {
                            var caputerRect = Qt.rect(selectionRectangleId.x,
                                                      selectionRectangleId.y,
                                                      selectionRectangleId.width,
                                                      selectionRectangleId.height)


                            toolId.addRectangle(caputerRect)

                            RootData.pageSelectionModel.back()

                            resetTool()
                        }
                    }

                    interactionId {
                        onHasDraggedChanged: {
                            if(!hasDragged) {
                                toolButtonId.state = "ACTIVE_STATE"
                            } else {
                                toolButtonId.state = ""
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
