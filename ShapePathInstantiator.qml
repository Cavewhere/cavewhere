import QtQuick
import QtQuick.Shapes
import CaveWhereSketch
import cavewherelib

Repeater {
    id: instantiatorId

    // required property Shape shape;

    // model: painterPathModel
    delegate: Shape {
        id: shapeId
        // parent: instantiatorId.parent

        anchors.fill: parent
        // width: 1000
        // height: 1000

        required property double strokeWidthRole
        required property var painterPath

        // DebugRectangle {

        // }

        SketchShapePath {
            //painterPath is a required property defined in c++
            // parent: shapeId
            painterPath: shapeId.painterPath
            pathHints: ShapePath.PathLinear
            strokeColor: shapeId.strokeWidthRole > 0 ? "black" : "transparent"
            // strokeColor: shapeId.strokeWidthRole > 0 ? "black" : "red" //For Debugging
            // fillColor: shapeId.strokeWidthRole > 0 ? "lightgray" : "black"
            fillColor: shapeId.strokeWidthRole > 0 ? "transparent" : "black"
            capStyle: ShapePath.RoundCap
            fillRule: ShapePath.WindingFill
            // strokeWidth: shapeId.strokeWidthRole < 0 ? 0.1 : shapeId.strokeWidthRole //For Debugging
            strokeWidth: shapeId.strokeWidthRole

            // onPainterPathChanged: {
            //     console.log("Painter path changed!")
            // }
        }
    }

    // onObjectAdded: (index, object) => {
    //                    // Manually add to the Shape’s data
    //                    // console.log("object:" + object)
    //                    shape.data.push(object)
    //                }
    // onObjectRemoved: (index, object) => {
    //                      object.destroy()
    //                  }
}
