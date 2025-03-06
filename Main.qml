import QtQuick
import QtQuick.Shapes
import CaveWhereSketch

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")

    PenLineModel {
        id: penModel
    }

    PainterPathModel {
        id: painterPathModel
        penLineModel: penModel
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

            // SketchShapePath {

            // }

            // preferredRendererType: Shape.SoftwareRenderer

            Instantiator {
                model: painterPathModel
                delegate: SketchShapePath {
                    //painterPath is a required property defined in c++
                    // parent: shapeId
                    strokeColor: "black"
                    // strokeColor: "transparent"
                    // fillColor: "red"
                    // fillColor: "black"
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    // joinStyle: ShapePath.RoundJoin
                    fillRule: ShapePath.WindingFill
                    strokeWidth: 2.5
                    // strokeWidth: -1
                }
                onObjectAdded: (index, object) => {
                                   // Manually add to the Shape’s data
                                   // console.log("object:" + object)
                                   shapeId.data.push(object)
                               }
            }

            // SketchShapePath {
            //     painterPath: painterPathModel.debugPainterPath()
            //     strokeColor: "black"
            //     fillColor: "red"
            //     strokeWidth: 2
            //     //painterPath is a required property defined in c++
            // }



            // ShapePath {
            //     strokeWidth: 4
            //     strokeColor: "red"
            //     // fillGradient: LinearGradient {
            //     //     x1: 20; y1: 20
            //     //     x2: 180; y2: 130
            //     //     GradientStop { position: 0; color: "blue" }
            //     //     GradientStop { position: 0.2; color: "green" }
            //     //     GradientStop { position: 0.4; color: "red" }
            //     //     GradientStop { position: 0.6; color: "yellow" }
            //     //     GradientStop { position: 1; color: "cyan" }
            //     // }
            //     fillColor: Qt.green
            //     strokeStyle: ShapePath.DashLine
            //     dashPattern: [ 1, 4 ]
            //     startX: 20; startY: 20
            //     PathLine { x: 180; y: 130 }
            //     PathLine { x: 20; y: 130 }
            //     PathLine { x: 20; y: 20 }
            // }


            PointHandler {
                id: handler

                property double pressureScaleStylus: 20.0
                property double pressureScaleMouse: 10.0
                property double pressureScale: pressureScaleStylus

                acceptedDevices: PointerDevice.Stylus | PointerDevice.Mouse
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

                onActiveChanged: {
                    console.log("Active changed!" + active)
                    if(active) {
                        painterPathModel.activeLineIndex = penModel.rowCount();
                        penModel.addNewLine();
                    }
                }

                onPointChanged: {
                    // console.log("Point.pressure:" + handler.point.pressure + active)

                    if(active) {
                        let width = pressureScale * handler.point.pressure
                        // if(width === 0.0) {
                        //     console.log("Width is zero!")
                        // }

                        // width = Math.min(3.0, width);

                        let penPoint = penModel.penPoint(handler.point.position, width)
                        penModel.addPoint(painterPathModel.activeLineIndex, penPoint)
                    }
                }
            }
        }
    }
}
