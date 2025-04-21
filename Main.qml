import QtQuick
import QtQuick.Shapes
import QtQuick.Controls
import QtQuick.Layouts
import CaveWhereSketch

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")

    PenLineModel {
        id: penModel
    }

    MovingAverageProxyModel {
        id: movingAverageProxyModelId
        sourceModel: penModel
    }

    PainterPathModel {
        id: painterPathModel
        // penLineModel: penModel
        penLineModel: movingAverageProxyModelId
    }

    ButtonGroup {
        id: strokeWidthGroupId
    }

    GroupBox {
        title: "Stroke Width"
        z: 1
        ColumnLayout {
            RadioButton {
                text: "Wall"
                ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = 4.0
                }
            }

            RadioButton {
                text: "Features"
                ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = 2.5
                }
            }

            RadioButton {
                text: "With Pressure"
                ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = -1.0;
                }
            }
        }
    }

    Item {
        id: containerId
        width: 1000
        height: 1000


        PinchHandler {
            target: containerId
            rotationAxis.enabled: false
        }

        Rectangle {
            // opacity: .5
            anchors.fill: parent
            // color: "red"
            border.width: 5
        }



        Shape {
            id: shapeId
            anchors.fill: parent

            // preferredRendererType: Shape.CurveRenderer
            // asynchronous: true

            // SketchShapePath {

            // }

            // preferredRendererType: Shape.SoftwareRenderer

            Instantiator {
                model: painterPathModel
                delegate: SketchShapePath {
                    required property double strokeWidthRole

                    //painterPath is a required property defined in c++
                    // parent: shapeId
                    pathHints: ShapePath.PathLinear
                    strokeColor: strokeWidthRole > 0 ? "black" : "transparent"
                    // strokeColor: strokeWidthRole > 0 ? "black" : "red" //For Debugging
                    // fillColor: strokeWidthRole > 0 ? "lightgray" : "black"
                    fillColor: strokeWidthRole > 0 ? "transparent" : "black"
                    capStyle: ShapePath.RoundCap
                    fillRule: ShapePath.WindingFill
                    // strokeWidth: strokeWidthRole < 0 ? 0.1 : strokeWidthRole //For Debugging
                    strokeWidth: strokeWidthRole


                }
                onObjectAdded: (index, object) => {
                                   // Manually add to the Shape’s data
                                   // console.log("object:" + object)
                                   shapeId.data.push(object)
                               }
                onObjectRemoved: (index, object) => {
                                     object.destroy()
                                 }
            }
        }

        PointHandler {
            id: handler

            property double pressureScale: 10.0

            acceptedDevices: PointerDevice.Unknown
            // acceptedDevices:  PointerDevice.Stylus | PointerDevice.Mouse
            cursorShape: Qt.BlankCursor
            target: Rectangle {
                parent: containerId
                color: "red"
                visible: handler.active
                x: handler.point.position.x - width / 2
                y: handler.point.position.y - height / 2
                width: handler.pressureScale * handler.point.pressure;
                height: width;
                radius: width / 2
            }
            // target: containerId

            onActiveChanged: {
                // console.log("Active changed!" + active)
                if(active) {
                    painterPathModel.activeLineIndex = penModel.rowCount();
                    penModel.addNewLine();
                }
            }

            onPointChanged: {
                // console.log("Point.pressure:" + handler.point.position + " " + handler.point.pressure + active)

                if(active) {
                    let penPoint = penModel.penPoint(handler.point.position, handler.point.pressure)
                    penModel.addPoint(painterPathModel.activeLineIndex, penPoint)
                }
            }
        }
    }
}
