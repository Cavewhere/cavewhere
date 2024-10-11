import QtQuick 2.0 as QQ
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

        onPressed: {
            initialPoint.x = mouse.x
            initialPoint.y = mouse.y
            selectionRectangle.x = initialPoint.x
            selectionRectangle.y = initialPoint.y
            selectionRectangle.width = 0;
            selectionRectangle.height = 0;
            hasDragged = false;
        }

        onPositionChanged: {
            var clampedMouseX = Math.max(0, Math.min(mouse.x, interactionId.width))
            var clampedMouseY = Math.max(0, Math.min(mouse.y, interactionId.height))

            var width = clampedMouseX - initialPoint.x
            var height = clampedMouseY - initialPoint.y

            var x = width > 0 ? initialPoint.x : clampedMouseX
            var y = height > 0 ? initialPoint.y : clampedMouseY

            selectionRectangle.width = Math.min(Math.abs(width), interactionId.width - x);
            selectionRectangle.height = Math.min(Math.abs(height), interactionId.height - y);

            selectionRectangle.x = x;
            selectionRectangle.y = y;

            hasDragged = true
            selectionRectangle.visible = true
        }



//        onReleased: {
//            deactivate()
//        }
    }
}
