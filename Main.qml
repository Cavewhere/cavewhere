import QtQuick
import QtQuick.Shapes
import QtQuick.Controls
import QtQuick.Layouts
import CaveWhereSketch

Window {
    id: windowId
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")

    function fuzzyEquals(value1, value2) {
        return Math.abs(value1 - value2) <= 0.0001;
    }

    PenLineModel {
        id: penModel

        onCurrentStrokeWidthChanged: {
            console.log("onCurrentStrokeWidthChanged!" + penModel.currentStrokeWidth + (penModel.currentStrokeWidth.toFixed(1) == 2.5))
            featuresId.checked = windowId.fuzzyEquals(penModel.currentStrokeWidth, 2.5);
        }
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
        opacity: 0.5


        ColumnLayout {
            id: columnLayoutId
            RadioButton {
                id: wallId
                text: "Wall"
                ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = 4.0
                }

                Binding {
                    wallId.checked: windowId.fuzzyEquals(penModel.currentStrokeWidth, 4.0)
                }
            }

            RadioButton {
                id: featuresId
                text: "Features"
                ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = 2.5
                }

                checked: windowId.fuzzyEquals(penModel.currentStrokeWidth, 2.5)
            }

            RadioButton {
                text: "With Pressure"
                ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = -1.0;
                }
            }

            Button {
                id: biggerId
                text: "Bigger add 0.1 to " + penModel.currentStrokeWidth.toFixed(2)
                onClicked: {
                    penModel.currentStrokeWidth += 0.1
                }
            }
        }
    }

    // Rectangle {
    //     color: "red"
    //     width: 100
    //     height: 100
    //     x: 100
    //     y: 100

    //     TapHandler {
    //         target: parent
    //         grabPermissions: PointerHandler.CanTakeOverFromAnything
    //         // acceptedDevices: handler.acceptedDevices |
    //         gesturePolicy: TapHandler.ReleaseWithinBounds
    //         onTapped: {
    //             console.log("Tapped!")
    //         }
    //     }
    // }

    Button {

        id: biggerId2
        z: 1
        x: 0
        y: 400
        text: "Bigger add 0.1 to " + penModel.currentStrokeWidth.toFixed(2)
        onClicked: {
            penModel.currentStrokeWidth += 0.1
        }
    }

    Rectangle {
        id: tapRectangleId
        z: 1
        color: "red"
        width: 100
        height: 100
        x: 250
        y: 300
        opacity: 0.5
        property int count: 0

        Text {
            text: "Tap count:" + tapRectangleId.count
        }

        TapHandler {
            // target: parent
            // grabPermissions:
            // grabPermissions: PointerHandler.CanTakeOverFromAnything
            // acceptedDevices: handler.acceptedDevices |
            gesturePolicy: TapHandler.WithinBounds
            onTapped: {
                console.log("Tapped!")
                tapRectangleId.count++;4
            }
        }
    }
    Rectangle {
        id: tapRectangleId2
        z: 1
        color: "green"
        width: 100
        height: 100
        x: 300
        y: 350
        opacity: 0.5
        property int count: 0

        Text {
            text: "Tap count:" + tapRectangleId2.count
        }

        TapHandler {
            // target: parent
            // grabPermissions: PointHandler.TakeOverForbidden
            // grabPermissions: PointerHandler.CanTakeOverFromAnything
            // acceptedDevices: handler.acceptedDevices |
            gesturePolicy: TapHandler.WithinBounds
            acceptedDevices:  PointerDevice.Stylus | PointerDevice.Mouse
            onTapped: {
                console.log("Tapped!")
                tapRectangleId2.count++;4
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

        ExclusivePointHandler {
            id: handler

            property double pressureScale: 10.0

            //This works for ipad over sidecar on macOS
            //See this bug: https://bugreports.qt.io/browse/QTBUG-80072
            //acceptedDevices: PointerDevice.Unknown

            //Works for ipad, windows 11 with pen, android with spen
            acceptedDevices:  PointerDevice.Stylus | PointerDevice.Mouse | PointerDevice.TouchScreen
            // acceptedPointerTypes: PointerDevice.Pen

            grabPermissions: PointerHandler.ApprovesTakeOverByAnything //PointerHandler.CanTakeOverFromHandlersOfSameType | PointerHandler.ApprovesTakeOverByAnything

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
            parent: containerId
            // target: containerId

            onActiveChanged: {
                // console.log("Active changed!" + active)
                if(active) {
                    painterPathModel.activeLineIndex = penModel.rowCount();
                    penModel.addNewLine();
                }
            }

            onPointChanged: {

                if(active && handler.point.pressure > 0.0) {
                    console.log("Point.pressure:" + handler.point.position + " " + handler.point.pressure + active)
                    let penPoint = penModel.penPoint(handler.point.position, handler.point.pressure)
                    penModel.addPoint(painterPathModel.activeLineIndex, penPoint)
                }
            }

            onGrabChanged: (transition, point) => {
                console.log("Grab changed:" + transition + " Exculive:" + PointerDevice.GrabExclusive + " Ungrab:" + PointerDevice.UngrabExclusive + " passive:" + PointerDevice.GrabPassive + " passive ungrab:" + PointerDevice.UngrabPassive)
            }
        }
    }

    // Rectangle {
    //     id: tapRectangleId
    //     z: 1
    //     color: "red"
    //     width: 100
    //     height: 100
    //     x: 250
    //     y: 300
    //     opacity: 0.5
    //     property int count: 0

    //     Text {
    //         text: "Tap count:" + tapRectangleId.count
    //     }

    //     TapHandler {
    //         target: parent
    //         // grabPermissions:
    //         // grabPermissions: PointerHandler.CanTakeOverFromAnything
    //         // acceptedDevices: handler.acceptedDevices |
    //         gesturePolicy: TapHandler.ReleaseWithinBounds
    //         onTapped: {
    //             console.log("Tapped!")
    //             tapRectangleId.count++;4
    //         }
    //     }
    // }
}
