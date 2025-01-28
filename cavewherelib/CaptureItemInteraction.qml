import QtQuick as QQ
import QtQuick.Window
import cavewherelib
import "VectorMath.js" as VectorMath

QQ.Rectangle {
    id: interactionId

    required property QuickSceneView quickSceneView
    required property CaptureViewport captureItem
    required property SelectionManager selectionManager;
    property double captureScale: 1.0
    property point captureOffset
    property bool selected: false

    //Private properties
    property double _scale: captureScale;
    property rect _viewRect: quickSceneView.toView(captureItem.boundingBox)
    property point _viewPos: quickSceneView.toView(captureItem.positionOnPaper)

    //For position
    property size _orginialSize

    //For Drag
    property point _oldPaperPos

    //For rotation
    property point _oldPoint
    property point _startPoint


    /**
      This converts the fixedPoint to a Qt.Point
      @param fixedPoint - QQ.Item.TransformOrigin. Only supports QQ.Item.TopLeft, QQ.Item.BottomLeft,
      QQ.Item.BottomLeft, and QQ.Item.TopLeft
      */
    function fixedPositionToPoint(fixedPosition) {
        let position = Qt.point(x / _scale, y / _scale) //captureItem.positionOnPaper
        let size = Qt.size(width / _scale, height / _scale) //captureItem.paperSizeOfItem

        switch(fixedPosition) {
        case QQ.Item.TopLeft:
            return position;
        case QQ.Item.TopRight:
            return Qt.point(position.x + size.width, position.y);
        case QQ.Item.BottomLeft:
            return Qt.point(position.x, position.y + size.height);
        case QQ.Item.BottomRight:
            return Qt.point(position.x + size.width, position.y + size.height);
        default:
            return Qt.point(0.0, 0.0);
        }
    }

    /**
      This scales the numberOfPixels (in pixels) into paper units (usually in inches).
      @param numberOfPixels - double in pixels
      @return double - in paper Units
      */
    function pixelToPaper(numberOfPixels) {
        return numberOfPixels / (interactionId._scale)
    }

    function pixelToPaperVec(delta) {
        return Qt.vector2d(pixelToPaper(delta.x), pixelToPaper(delta.y))
    }

    /**
      This handles the drag Resize of the catpureItem

      @param delta - Qt.point() with the delta of the mouse movement
      @param corner - QQ.Item.TransformOrigin
      */
    function dragResizeHandler(delta, corner) {

        let sizeDelta = pixelToPaperVec(delta);
        switch(corner) {
        case QQ.Item.TopLeft:
            sizeDelta = Qt.vector2d(-sizeDelta.x, -sizeDelta.y)
            break;
        case QQ.Item.BottomLeft:
            sizeDelta = Qt.vector2d(-sizeDelta.x, sizeDelta.y)
            break;
        case QQ.Item.TopRight:
            sizeDelta = Qt.vector2d(sizeDelta.x, -sizeDelta.y);
            break
        }

        captureItem.setPaperSizePreserveAspect(Qt.size(interactionId._orginialSize.width + sizeDelta.x,
                                                       interactionId._orginialSize.height + sizeDelta.y),
                                               corner);
    }

    /**
      This handles the drag Rotation of the catpureItem

      @param delta - Qt.Point() with the delta of the mouse movement
      */
    function dragRotationHandler(delta) {
        let center = interactionId.mapToItem(null, centerItemId.x, centerItemId.y);

        let p1 = _oldPoint
        let p2 = Qt.point(_startPoint.x + delta.x, _startPoint.y + delta.y)


        let v1 = Qt.point(p1.x - center.x, p1.y - center.y)
        let v2 = Qt.point(p2.x - center.x, p2.y - center.y)

        function isLengthValid(v) {
            return v.x !== 0.0 && v.y !== 0.0
        }

        if(isLengthValid(v1) && isLengthValid(v2)) {
            let angle = VectorMath.angleBetween(v1, v2);
            let sign = VectorMath.crossProduct(v1, v2) > 0 ? 1 : -1;
            console.log("Center:" + center + " p1:" + p1 + " p2:" + p2 + " sign:" + sign + " angle:" + angle + " v1:" + v1 + " v2:" + v2)

            captureItem.rotation += sign * angle;
        }
        _oldPoint = p2;
    }

    // function updatePaperScene() {
    //     console.log("Position on paper:" + captureItem.positionOnPaper + " new Position:" + quickSceneView.toPaper(Qt.point(x, y)))
    //     let topLeftBoundsOnPaper = quickSceneView.toPaper(Qt.point(x, y));
    //     let diff = topLeftBoundsOnPaper.minus(interactionId._viewPos);

    //     captureItem.positionOnPaper = quickSceneView.toPaper(Qt.point(x, y));
    // }

    function dragOffset() {
        let center = interactionId.mapToItem(null, centerItemId.x, centerItemId.y);
        return Qt.vector2d(center.x, center.y)
    }

    //This item is used to calculate the center of the interaction capture.
    //It is used for the rotation of the item
    QQ.Item {
        id: centerItemId
        anchors.centerIn: parent
    }

    width: _viewRect.width
    height: _viewRect.height

    x: _viewRect.x
    y: _viewRect.y

    border.width: 1
    border.color: "#d3d3d3"
    color: "#00000000"

    // onSelectedChanged: {
    //     if(selected) {
    //         interactionId.state = "SELECTED"
    //     } else {
    //         interactionId.state = "INIT_STATE"
    //     }
    // }

    // onCaptureItemChanged: {
    //     state = interactionId.captureItem === null ? "" : "INIT_STATE"
    // }

    // QQ.Rectangle {
    //     parent: interactionId.parent
    //     width: 10
    //     height: 10
    //     color: "red"
    //     x: interactionId._viewPos.x - width * 0.5
    //     y: interactionId._viewPos.y - height * 0.5
    // }

    QQ.TapHandler {
        id: selectTapId

        onTapped: (eventPoint, button) => {
            interactionId.selectionManager.cycleSelectItem(interactionId, eventPoint.timestamp)
        }
    }

    QQ.DragHandler {
        id: dragHandler
        enabled: false
        target: null
        dragThreshold: 1
        onActiveChanged: {
            if(active) {
                interactionId._oldPaperPos = interactionId.captureItem.positionOnPaper
            }
        }

        onActiveTranslationChanged: {
            let deltaOnPaper = quickSceneView.toPaper(Qt.point(activeTranslation.x, activeTranslation.y))
            let oldPaperPos = interactionId._oldPaperPos;
            let posOnPaper = Qt.point(oldPaperPos.x + deltaOnPaper.x, oldPaperPos.y + deltaOnPaper.y);
            interactionId.captureItem.positionOnPaper = posOnPaper
        }
    }

    component ResizeHandle: RectangleHandle {
        required property int corner

        imageSource: "qrc:/twbs-icons/icons/arrows-angle-expand.svg"
        imageSourceSize: Qt.size(20,20)
        onDragDelta: (delta) => {
            dragResizeHandler(delta, corner)
        }
        onDragStarted: () => {
            interactionId._orginialSize = captureItem.paperSizeOfItem
        }
    }

    ResizeHandle {
        id: topLeftHandle
        objectName: "topLeftHandle"
        anchors.bottom: interactionId.top
        anchors.right: interactionId.left
        anchors.bottomMargin: -imageSourceSize.height * 0.5
        anchors.rightMargin: -imageSourceSize.width * 0.5
        imageRotation: 90
        corner: QQ.Item.TopLeft
        visible: false
    }

    ResizeHandle {
        id: topRightHandle
        objectName: "topRightHandle"
        anchors.bottom: interactionId.top
        anchors.left: interactionId.right
        anchors.bottomMargin: -imageSourceSize.height * 0.5
        anchors.leftMargin: -imageSourceSize.width * 0.5
        imageRotation: 0
        corner: QQ.Item.TopRight
        visible: false
    }

    ResizeHandle {
        id: bottomLeftHandle
        objectName: "bottomLeftHandle"
        anchors.top: interactionId.bottom
        anchors.right: interactionId.left
        anchors.topMargin: -imageSourceSize.height * 0.5
        anchors.rightMargin: -imageSourceSize.width * 0.5
        imageRotation: 0
        corner: QQ.Item.BottomLeft
        visible: false
    }

    ResizeHandle {
        id: bottomRightHandle
        objectName: "bottomRightHandle"
        anchors.top: interactionId.bottom
        anchors.left: interactionId.right
        anchors.topMargin: -imageSourceSize.height * 0.5
        anchors.leftMargin: -imageSourceSize.width * 0.5
        imageRotation: 90
        corner: QQ.Item.BottomRight
        visible: false
    }

    RectangleHandle {
        id: rotationHandle
        objectName: "rotationHandle"
        imageSource: "qrc:/twbs-icons/icons/arrow-repeat.svg"
        anchors.centerIn: interactionId
        imageSourceSize: Qt.size(40, 40)
        onDragDelta: (delta) => {dragRotationHandler(delta)}
        onDragStarted: (startPoint) => {
                           interactionId._startPoint = startPoint
                           interactionId._oldPoint = startPoint
                       }
        visible: false
        QQ.Rectangle {
            color: "#272727"
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
        }

        //We don't want to use a button here because this handle
        //does dragging
        QQ.Rectangle {
            z: -1
            color: "#ebebeb" //Background for button
            anchors.centerIn: parent
            width: rotationHandle.width
            height: rotationHandle.height
            radius: rotationHandle.width * 0.5
        }
    }

    states: [
        QQ.State {
            when: interactionId.selected
            QQ.PropertyChanges {
                topLeftHandle.visible: true
                topRightHandle.visible: true
                bottomLeftHandle.visible: true
                bottomRightHandle.visible: true
                rotationHandle.visible: interactionId.width >= rotationHandle.imageSourceSize.width &&
                                        interactionId.height >= rotationHandle.imageSourceSize.height
                dragHandler.enabled: true
                interactionId.border.color: "#4a4a4a" //Selection color
            }
        }
    ]
}
