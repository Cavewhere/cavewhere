import QtQuick 2.15
import QtQuick.Shapes 1.15

//
//   This will make a box around you qml elements like this.
//      /\
//   --/  \-------------
//   |  Child items    |
//   -------------------
//

Item {
    id: root
    objectName: "quoteBox"

    default property alias defaultChildren: childrenContainer.children
    property color color: "white"
    property color borderColor: "#727272"
    property real borderWidth: 1.0
    property real margin: 3.0
    // property int triangleEdge: Qt.TopEdge
    property real triangleOffset: 5.0
    property Item pointAtObject
    property point pointAtObjectPosition

    function updatePosition() {
        if (!visible || !pointAtObject) {
            return;
        }
        const newPos = pointAt(pointAtObject, pointAtObjectPosition);
        x = newPos.x;
        y = newPos.y;
    }

    function pointAt(item, position) {
        if (item !== null) {
            const globalPos = item.mapToItem(null, position.x, position.y);
            const globalTrianglePos = box.mapToItem(null, 0, 0);
            return Qt.point(
                        root.x + (globalPos.x - globalTrianglePos.x),
                        root.y + (globalPos.y - globalTrianglePos.y)
                        );
        }
        return Qt.point(0, 0);
    }

    onPointAtObjectChanged: updatePosition()
    onPointAtObjectPositionChanged: updatePosition()
    onVisibleChanged: updatePosition()

    Shape {
        id: box

        ShapePath {
            id: shapePath

            property size triangle: Qt.size(6, 12)
            property real triangleOffset: 10
            property real topEdge: triangle.height + root.triangleOffset
            property point boxTopLeft: Qt.point(-(triangleOffset + 2 * triangle.width), topEdge)
            property point boxBottomRight: Qt.point(childrenContainer.width + boxTopLeft.x,
                                                    childrenContainer.height + boxTopLeft.y)

            strokeColor: root.borderColor
            strokeWidth: root.borderWidth
            fillColor: root.color

            startX: 0; startY: root.triangleOffset //Pointy tip of the triangle
            PathLine { x: shapePath.triangle.width; y: shapePath.topEdge }
            PathLine { x: shapePath.boxBottomRight.x; y: shapePath.boxTopLeft.y } //TopRight
            PathLine { x: shapePath.boxBottomRight.x; y: shapePath.boxBottomRight.y } //BottomRight
            PathLine { x: shapePath.boxTopLeft.x; y: shapePath.boxBottomRight.y } //BottomLeft
            PathLine { x: shapePath.boxTopLeft.x; y: shapePath.boxTopLeft.y } //TopLeft
            PathLine { x: -shapePath.triangle.width; y: shapePath.boxTopLeft.y } //To the bottom of the triangle
            PathLine { x: 0; y: root.triangleOffset } //Pointy tip of the triangle
        }

        Item {
            id: childrenContainer
            x: shapePath.boxTopLeft.x + root.margin
            y: shapePath.topEdge + root.margin
            implicitWidth: childrenRect.width + root.margin * 2.0
            implicitHeight: childrenRect.height + root.margin * 2.0
        }
    }
}
