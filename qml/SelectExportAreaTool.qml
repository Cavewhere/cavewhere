import QtQuick 2.0
import QtQuick.Controls 1.2
import Cavewhere 1.0

Item {
    id: toolId

    property GLTerrainRenderer view
    property CaptureManager manager;

    anchors.fill: parent
    visible: parent !== null

    onVisibleChanged: {
        if(!visible) {
            resetTool()
        }
    }

    function addRectangle(rectangle) {
        var viewObject = captureViewComponentId.createObject()
        viewObject.view = view
        viewObject.viewport = rectangle
        viewObject.capture();
        manager.addViewportCapture(viewObject);
    }

    function resetTool() {
        interactionId.deactivate()
        selectionRectangleId.visible = false
        selectionRectangleId.width = 0
        selectionRectangleId.height = 0
        toolButtonId.enabled = true
        state = ""
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
        enabled: true

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

                        resetTool()

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

    onViewChanged: {
        view.interactionManager.add(interactionId)
    }

}
