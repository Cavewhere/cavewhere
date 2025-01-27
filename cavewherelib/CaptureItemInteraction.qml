import QtQuick as QQ
import QtQuick.Window
import cavewherelib
import "VectorMath.js" as VectorMath

QQ.Rectangle {
    id: interactionId

    required property QuickSceneView quickSceneView
    required property CaptureViewport captureItem
    property double captureScale: 1.0
    property point captureOffset
    property bool selected: false

    //Private properties
    property double _scale: captureScale;
    property rect _viewRect: quickSceneView.toView(captureItem.boundingBox)

    //For position
    property size _orginialSize

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
      @param oldPoint - Qt.Point() this is the old mouse position
      */
    function dragRotationHandler(handle, delta, oldPoint) {
        let center = interactionId.mapToItem(null, centerItemId.x, centerItemId.y);

        let p1 = _oldPoint
        let p2 = Qt.point(_startPoint.x + delta.x, _startPoint.y + delta.y)

        let v1 = Qt.point(p1.x - center.x, p1.y - center.y)
        let v2 = Qt.point(p2.x - center.x, p2.y - center.y)

        let angle = VectorMath.angleBetween(v1, v2);
        let sign = VectorMath.crossProduct(v1, v2) > 0 ? 1 : -1;

        _oldPoint = p2;
        captureItem.rotation += sign * angle;
    }

    function updatePaperScene() {
        captureItem.positionOnPaper = quickSceneView.toPaper(Qt.point(x, y));
    }

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
    border.color: "#151515"
    color: "#00000000"

    onSelectedChanged: {
        if(selected) {
            interactionId.state = "SELECTED_RESIZE_STATE"
        } else {
            interactionId.state = "INIT_STATE"
        }
    }

    onCaptureItemChanged: {
        state = interactionId.captureItem === null ? "" : "INIT_STATE"
    }

    QQ.TapHandler {
        id: selectTapId
    }

    QQ.DragHandler {
        id: dragHandler
        enabled: interactionId.selected
        onActiveChanged: {
            if(!active) {
                //Restore x and y bindings
                interactionId.x = Qt.binding(function() { return interactionId._viewRect.x } );
                interactionId.y = Qt.binding(function() { return interactionId._viewRect.y } );
            } else {
                //This removes the bindings by assigning the values to it self, this prevents
                //a binding loop from happening when the drag takes over
                interactionId.x = interactionId.x
                interactionId.y = interactionId.y
            }
        }
    }

    RectangleHandle {
        id: topLeftHandle
        objectName: "topLeftHandle"
        anchors.bottom: interactionId.top
        anchors.right: interactionId.left
    }

    RectangleHandle {
        id: topRightHandle
        objectName: "topRightHandle"
        anchors.bottom: interactionId.top
        anchors.left: interactionId.right
    }

    RectangleHandle {
        id: bottomLeftHandle
        objectName: "bottomLeftHandle"
        anchors.top: interactionId.bottom
        anchors.right: interactionId.left
    }

    RectangleHandle {
        id: bottomRightHandle
        objectName: "bottomRightHandle"
        anchors.top: interactionId.bottom
        anchors.left: interactionId.right
    }

    states: [
        QQ.State {
            name: "INIT_STATE"

            QQ.PropertyChanges {
                selectTapId {
                    onTapped: (eventPoint, button) => {
                        interactionId.selected = true
                    }
                }
            }
        },

        QQ.State {
            name: "SELECTED"
            extend: "INIT_STATE"
            QQ.PropertyChanges {
                interactionId {
                    onXChanged: {
                        //this prevents a binding loop on x
                        if(dragHandler.active) {
                            interactionId.updatePaperScene();
                        }
                    }
                    onYChanged: {
                        //this prevents a binding loop on y
                        if(dragHandler.active) {
                            interactionId.updatePaperScene();
                        }
                    }
                }
            }
        },

        QQ.State {
            name: "SELECTED_RESIZE_STATE"
            extend: "SELECTED"

            QQ.PropertyChanges {
                selectTapId {
                    onTapped: (eventPoint, button) => {
                        interactionId.state = "SELECTED_ROTATE_STATE"
                    }
                }
            }

            QQ.PropertyChanges {
                topLeftHandle {
                    imageSource: "qrc:icons/dragArrow/arrowHighLeftBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/arrowHighLeft.png"
                    imageRotation: 0
                    onDragDelta: (delta) => {
                        dragResizeHandler(delta, QQ.Item.TopLeft)
                    }
                    onDragStarted: () => {
                        interactionId._orginialSize = captureItem.paperSizeOfItem
                    }
                }
            }
            QQ.PropertyChanges {
                topRightHandle {
                    imageSource: "qrc:icons/dragArrow/arrowHighRightBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/arrowHighRight.png"
                    imageRotation: 0
                    onDragDelta: (delta) => {
                        dragResizeHandler(delta, QQ.Item.TopRight)
                    }
                    onDragStarted: () => {
                        interactionId._orginialSize = captureItem.paperSizeOfItem
                    }
                }
            }
            QQ.PropertyChanges {
                bottomLeftHandle {
                    imageSource: "qrc:icons/dragArrow/arrowHighRightBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/arrowHighRight.png"
                    imageRotation: 0
                    onDragDelta: (delta) => {
                        dragResizeHandler(delta, QQ.Item.BottomLeft)
                    }
                    onDragStarted: () => {
                        interactionId._orginialSize = captureItem.paperSizeOfItem
                    }
                }
            }
            QQ.PropertyChanges {
                bottomRightHandle {
                    imageSource: "qrc:icons/dragArrow/arrowHighLeftBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/arrowHighLeft.png"
                    imageRotation: 0
                    onDragDelta: (delta) => {
                        dragResizeHandler(delta, QQ.Item.BottomRight)
                    }
                    onDragStarted: () => {
                        interactionId._orginialSize = captureItem.paperSizeOfItem
                    }
                }
            }

        },

        QQ.State {
            name: "SELECTED_ROTATE_STATE"
            extend: "SELECTED"

            QQ.PropertyChanges {
                selectTapId {
                    onTapped: (eventPoint, button) => {
                        interactionId.state = "SELECTED_RESIZE_STATE"
                    }
                }
            }

            QQ.PropertyChanges {
                topLeftHandle {
                    imageSource: "qrc:icons/dragArrow/rotateArrowBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/rotateArrow.png"
                    imageRotation: 90
                    onDragDelta: (delta) => {dragRotationHandler(topLeftHandle, delta)}
                    onDragStarted: (startPoint) => {
                        interactionId._startPoint = startPoint
                        interactionId._oldPoint = startPoint
                    }
                }
            }
            QQ.PropertyChanges {
                topRightHandle {
                    imageSource: "qrc:icons/dragArrow/rotateArrowBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/rotateArrow.png"
                    imageRotation: 180
                    onDragDelta: (delta) => {dragRotationHandler(topRightHandle, delta)}
                    onDragStarted: (startPoint) => {
                        interactionId._startPoint = startPoint
                        interactionId._oldPoint = startPoint }
                }
            }
            QQ.PropertyChanges {
                bottomLeftHandle {
                    imageSource: "qrc:icons/dragArrow/rotateArrowBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/rotateArrow.png"
                    imageRotation: 0
                    onDragDelta: (delta) => {dragRotationHandler(bottomLeftHandle, delta)}
                    onDragStarted: (startPoint) => {
                        interactionId._startPoint = startPoint
                        interactionId._oldPoint = startPoint
                    }
                }
            }
            QQ.PropertyChanges {
                bottomRightHandle {
                    imageSource: "qrc:icons/dragArrow/rotateArrowBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/rotateArrow.png"
                    imageRotation: 270
                    onDragDelta: (delta) => {dragRotationHandler(bottomRightHandle, delta)}
                    onDragStarted: (startPoint) => {
                        interactionId._startPoint = startPoint
                        interactionId._oldPoint = startPoint
                    }
                }
            }
        }
    ]
}
