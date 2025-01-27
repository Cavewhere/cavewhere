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

    property size _orginialSize

    // onCaptureOffsetChanged: {
    //     console.log("CaptureOffset:" + captureOffset + this)
    // }

    // on_ScaleChanged: {
    //     console.log("Scale changed:" + _scale);
    // }

    // on_ViewRectChanged: {
    //     console.log("_viewRectChanged:" + _viewRect)
    // }

    // onXChanged: {
    //     console.log("X:" + x + this)
    // }

    // onYChanged: {
    //     console.log("Y:" + y + this)
    // }

    // onWidthChanged: {
    //     console.log("Width:" + width + " " + this)
    // }

    // onHeightChanged: {
    //     console.log("Height:" + height + " " + this);
    // }

    /**
      This find the maxium delta from the x and y.

      @param delta - Qt.point() that's the mouse change in position
      @param fixedPoint - QQ.Item.TransformOrigin (ie. QQ.Item.TopLeft for example). This will
      control the sign of the dragLength that's returned.
      @return The maximium drag length. This is used for resizing the CaptureItem
      */
    function dragLength(delta, fixedPosition) {



        let sign = 1.0;
        let originalDelta = delta;

        console.log("Fix position:" + fixedPosition + " " + QQ.Item.TopLeft + " " + QQ.Item.TopRight + " " + QQ.Item.BottomLeft + " " + QQ.Item.BottomRight)

        switch(fixedPosition) {
        case QQ.Item.TopLeft:
            sign = -1;
            console.log("TopLeft!")
            break;
        case QQ.Item.TopRight:
            console.log("TopRight!")
            delta.y = -delta.y;
            break;
        case QQ.Item.BottomLeft:
            console.log("BottomLeft")
            delta.y = -delta.y;
            sign = -1;
            break;
        case QQ.Item.BottomRight:
            console.log("BottomRight")
            // sign =;
            break
        default:
            console.error("FixedPosition is invalid:" + fixedPosition);
        }

        console.log("Delta orig: " + originalDelta + " fixed: " + delta)

        let length = 0;
        if(delta.y >= 0 && delta.x >= 0) {
            console.log("here1")
            length = Math.max(delta.y, delta.x);
        } else if(delta.y <= 0 && delta.x <= 0) {
            console.log("here2")
            length = Math.min(delta.y, delta.x);
        } else {
            if(Math.abs(delta.y) > Math.abs(delta.x)) {
                console.log("here3")
                length = delta.y
            } else {
                console.log("here4")
                length = delta.x
            }
        }

        console.log("Sign:" + sign + " Length:" + length)



        return sign * length;
    }

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
    function dragRotationHandler(delta, oldPoint) {
        let center = interactionId.mapToItem(null, centerItemId.x, centerItemId.y);

        let p1 = oldPoint
        let p2 = Qt.point(delta.x + p1.x, delta.y + p1.y)
        let v1 = Qt.point(p1.x - center.x, p1.y - center.y)
        let v2 = Qt.point(p2.x - center.x, p2.y - center.y)

        let angle = VectorMath.angleBetween(v1, v2);
        let sign = VectorMath.crossProduct(v1, v2) > 0 ? -1 : 1;

        captureItem.rotation += sign * angle;
    }

    function updatePaperScene() {
        captureItem.positionOnPaper = quickSceneView.toPaper(Qt.point(x, y));
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
            extend: "INIT_STATE"

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
                    onDragDelta: dragRotationHandler(delta, oldPoint)
                }
            }
            QQ.PropertyChanges {
                topRightHandle {
                    imageSource: "qrc:icons/dragArrow/rotateArrowBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/rotateArrow.png"
                    imageRotation: 180
                    onDragDelta: dragRotationHandler(delta, oldPoint)
                }
            }
            QQ.PropertyChanges {
                bottomLeftHandle {
                    imageSource: "qrc:icons/dragArrow/rotateArrowBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/rotateArrow.png"
                    imageRotation: 0
                    onDragDelta: dragRotationHandler(delta, oldPoint)
                }
            }
            QQ.PropertyChanges {
                bottomRightHandle {
                    imageSource: "qrc:icons/dragArrow/rotateArrowBlack.png"
                    selectedImageSource: "qrc:icons/dragArrow/rotateArrow.png"
                    imageRotation: 270
                    onDragDelta: dragRotationHandler(delta, oldPoint)
                }
            }
        }
    ]
}
