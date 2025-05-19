import QtQuick
import QtQuick.Shapes
import QtQuick.Controls as QC
import QtQuick.Layouts
import CaveWhereSketch
import cavewherelib

StandardPage {
    id: sketchPageId
    clip: true

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

    QC.ButtonGroup {
        id: strokeWidthGroupId
    }

    QC.GroupBox {
        title: "Stroke Width"
        z: 1
        opacity: 0.5


        ColumnLayout {
            id: columnLayoutId
            QC.RadioButton {
                id: wallId
                text: "Wall"
                QC.ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = 4.0
                }

                Binding {
                    wallId.checked: windowId.fuzzyEquals(penModel.currentStrokeWidth, 4.0)
                }
            }

            QC.RadioButton {
                id: featuresId
                text: "Features"
                QC.ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = 2.5
                }

                checked: windowId.fuzzyEquals(penModel.currentStrokeWidth, 2.5)
            }

            QC.RadioButton {
                text: "With Pressure"
                QC.ButtonGroup.group: strokeWidthGroupId
                onClicked: {
                    penModel.currentStrokeWidth = -1.0;
                }
            }

            QC.Button {
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

    // QC.Button {

    //     id: biggerId2
    //     z: 1
    //     x: 0
    //     y: 400
    //     text: "Bigger add 0.1 to " + penModel.currentStrokeWidth.toFixed(2)
    //     onClicked: {
    //         penModel.currentStrokeWidth += 0.1
    //     }
    // }

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
    //         // target: parent
    //         // grabPermissions:
    //         // grabPermissions: PointerHandler.CanTakeOverFromAnything
    //         // acceptedDevices: handler.acceptedDevices |
    //         gesturePolicy: TapHandler.WithinBounds
    //         onTapped: {
    //             console.log("Tapped!")
    //             tapRectangleId.count++;4
    //         }
    //     }
    // }
    // Rectangle {
    //     id: tapRectangleId2
    //     z: 1
    //     color: "green"
    //     width: 100
    //     height: 100
    //     x: 300
    //     y: 350
    //     opacity: 0.5
    //     property int count: 0

    //     Text {
    //         text: "Tap count:" + tapRectangleId2.count
    //     }

    //     TapHandler {
    //         // target: parent
    //         // grabPermissions: PointHandler.TakeOverForbidden
    //         // grabPermissions: PointerHandler.CanTakeOverFromAnything
    //         // acceptedDevices: handler.acceptedDevices |
    //         gesturePolicy: TapHandler.WithinBounds
    //         acceptedDevices:  PointerDevice.Stylus | PointerDevice.Mouse
    //         onTapped: {
    //             console.log("Tapped!")
    //             tapRectangleId2.count++;4
    //         }
    //     }
    // }


    PinchHandler {
        parent: sketchPageId
        target: containerId
        rotationAxis.enabled: false
        scaleAxis.enabled: false
    }

    ExclusivePointHandler {
        id: handler

        property double pressureScale: 10.0

        property point _mappedHandlerPoint

        //This works for ipad over sidecar on macOS
        //See this bug: https://bugreports.qt.io/browse/QTBUG-80072
        //acceptedDevices: PointerDevice.Unknown

        //Works for ipad, windows 11 with pen, android with spen
        acceptedDevices:  PointerDevice.Stylus | PointerDevice.Mouse //| PointerDevice.TouchScreen
        // acceptedPointerTypes: PointerDevice.Pen

        grabPermissions: PointerHandler.ApprovesTakeOverByAnything //PointerHandler.CanTakeOverFromHandlersOfSameType | PointerHandler.ApprovesTakeOverByAnything

        cursorShape: Qt.BlankCursor
        target: Rectangle {
            parent: containerId
            color: "red"
            visible: handler.active
            x: handler._mappedHandlerPoint.x - width / 2
            y: handler._mappedHandlerPoint.y - height / 2
            width: handler.pressureScale * handler.point.pressure;
            height: width;
            radius: width / 2
        }
        parent: sketchPageId
        // parent: containerId
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
                _mappedHandlerPoint = sketchPageId.mapToItem(containerId, handler.point.position);

                let penPoint = penModel.penPoint(_mappedHandlerPoint, handler.point.pressure)
                penModel.addPoint(painterPathModel.activeLineIndex, penPoint)
            }
        }

        onGrabChanged: (transition, point) => {
            console.log("Grab changed:" + transition + " Exculive:" + PointerDevice.GrabExclusive + " Ungrab:" + PointerDevice.UngrabExclusive + " passive:" + PointerDevice.GrabPassive + " passive ungrab:" + PointerDevice.UngrabPassive)
        }
    }

    Item {
        id: containerId
        width: 1000
        height: 1000


        // Rectangle {
        //     // opacity: .5
        //     anchors.fill: parent
        //     // color: "red"
        //     border.width: 5
        // }

        Rectangle {
            width: 5
            height: 5
            color: "black"
        }

        ShapePathInstantiator {
            anchors.fill: parent
            model: RootDataSketch.centerlinePainterModel
            // shape: centerline
        }

        ShapePathInstantiator {
            anchors.fill: parent
            model: painterPathModel
            // shape: shapeId
        }

        // Shape {
        //     id: centerline
        //     anchors.fill: parent

        // }

        // Shape {
        //     id: shapeId
        //     anchors.fill: parent

        //     // preferredRendererType: Shape.CurveRenderer
        //     // asynchronous: true

        //     // SketchShapePath {

        //     // }

        //     // preferredRendererType: Shape.SoftwareRenderer

        //     ShapePathInstantiator {
        //         model: painterPathModel
        //         shape: shapeId
        //     }
        // }
    }

}
