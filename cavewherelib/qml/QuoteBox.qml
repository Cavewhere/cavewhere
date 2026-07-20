import QtQuick 2.15
import QtQuick.Shapes 1.15
import cavewherelib

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
    property color color: Theme.surface
    property color borderColor: Theme.border
    property real borderWidth: 1.0
    property real margin: 3.0
    // property int triangleEdge: Qt.TopEdge
    property real triangleOffset: 5.0
    property bool triangleOnRight: false
    property Item pointAtObject
    property point pointAtObjectPosition

    // Reading every ancestor's x/y makes this a genuine binding, so any of
    // them moving re-fires it. pointAtObjectChanged alone can't see that:
    // the target usually keeps its local x/y while a layout pass shifts an
    // ancestor, which used to leave the box parked where it was first
    // placed until something else forced an update.
    readonly property point targetScenePosition: {
        //Hidden boxes still keep this binding registered on every ancestor,
        //so an unrelated scroll would re-walk the chain for nothing.
        //onVisibleChanged repositions on the way back in.
        if (!root.visible || !root.pointAtObject) {
            return Qt.point(0, 0);
        }
        //Deliberately not named x/y: those would shadow root.x/root.y,
        //the two properties updatePosition() writes, and an accidental
        //read of them here would close a feedback loop.
        let sceneX = root.pointAtObjectPosition.x;
        let sceneY = root.pointAtObjectPosition.y;
        for (let item = root.pointAtObject; item !== null; item = item.parent) {
            sceneX += item.x;
            sceneY += item.y;
        }
        return Qt.point(sceneX, sceneY);
    }

    // When set, the box stops drawing once its anchor scrolls outside this
    // item. A box that points into a clipped, scrolling area is usually a
    // sibling of it, so nothing else would clip the box itself.
    property Item visibilityClip: null

    readonly property bool anchorInsideClip: {
        if (visibilityClip === null || pointAtObject === null) {
            return true;
        }
        //Depend on the reactive scene position so scrolling re-evaluates
        //this; mapToItem on its own registers no dependencies.
        void targetScenePosition;
        const topLeft = pointAtObject.mapToItem(visibilityClip, 0, 0);
        return topLeft.x + pointAtObject.width > 0
                && topLeft.x < visibilityClip.width
                && topLeft.y + pointAtObject.height > 0
                && topLeft.y < visibilityClip.height;
    }

    // Room for the body between the tip and the parent's edge, on whichever
    // side the box opens. The tip is anchored to the target, so this never
    // depends on the body's own width - content can be clamped to it without
    // feeding back into the layout.
    readonly property real maximumContentWidth: {
        if (parent === null) {
            return Number.POSITIVE_INFINITY;
        }
        const stub = shapePath.triangleStubWidth;
        const edges = 3.0 * margin;
        const budget = triangleOnRight
                ? x + stub - edges
                : parent.width - x + stub - edges;
        return Math.max(0.0, budget);
    }

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
    onTargetScenePositionChanged: updatePosition()
    onVisibleChanged: updatePosition()

    Shape {
        id: box

        visible: root.anchorInsideClip

        ShapePath {
            id: shapePath

            property size triangle: Qt.size(6, 12)
            property real triangleOffset: 10
            property real topEdge: triangle.height + root.triangleOffset
            property real triangleStubWidth: triangleOffset + 2 * triangle.width
            property point boxTopLeft: root.triangleOnRight
                ? Qt.point(-(childrenContainer.width - triangleStubWidth), topEdge)
                : Qt.point(-triangleStubWidth, topEdge)
            property point boxBottomRight: root.triangleOnRight
                ? Qt.point(triangleStubWidth, childrenContainer.height + boxTopLeft.y)
                : Qt.point(childrenContainer.width + boxTopLeft.x, childrenContainer.height + boxTopLeft.y)

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
