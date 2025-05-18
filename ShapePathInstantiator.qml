import QtQuick
import QtQuick.Shapes
import CaveWhereSketch

Instantiator {

    required property Shape shape;

    // model: painterPathModel
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
                       shape.data.push(object)
                   }
    onObjectRemoved: (index, object) => {
                         object.destroy()
                     }
}
