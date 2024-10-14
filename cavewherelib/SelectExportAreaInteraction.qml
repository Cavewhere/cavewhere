import QtQuick as QQ
import cavewherelib

Interaction {
    id: interactionId

    property QQ.Rectangle selectionRectangle
    property bool hasDragged

    onDeactivated: {
        hasDragged = false
    }

    QQ.MouseArea {
        anchors.fill: parent

        property point initialPoint

        onPressed: (mouse) => {
            initialPoint.x = mouse.x
            initialPoint.y = mouse.y
            interactionId.selectionRectangle.x = initialPoint.x
            interactionId.selectionRectangle.y = initialPoint.y
            interactionId.selectionRectangle.width = 0;
            interactionId.selectionRectangle.height = 0;
            interactionId.hasDragged = false;
        }

        onPositionChanged: (mouse) => {
            var clampedMouseX = Math.max(0, Math.min(mouse.x, interactionId.width))
            var clampedMouseY = Math.max(0, Math.min(mouse.y, interactionId.height))

            var width = clampedMouseX - initialPoint.x
            var height = clampedMouseY - initialPoint.y

            var x = width > 0 ? initialPoint.x : clampedMouseX
            var y = height > 0 ? initialPoint.y : clampedMouseY

            interactionId.selectionRectangle.width = Math.min(Math.abs(width), interactionId.width - x);
            interactionId.selectionRectangle.height = Math.min(Math.abs(height), interactionId.height - y);

            interactionId.selectionRectangle.x = x;
            interactionId.selectionRectangle.y = y;

            interactionId.hasDragged = true
            interactionId.selectionRectangle.visible = true
        }



//        onReleased: {
//            deactivate()
//        }
    }
}
