import QtQuick 2.0
import Cavewhere 1.0
import "Utils.js" as Utils

Rectangle {
    id: interactionId

    property CaptureItem captureItem: null;
    property double captureScale: 1.0
    property point captureOffset
    property bool selected: false

    /**
      This find the maxium delta from the x and y.

      @param delta - Qt.point() that's the mouse change in position
      @param fixedPoint - Item.TransformOrigin (ie. Item.TopLeft for example). This will
      control the sign of the dragLength that's returned.
      @return The maximium drag length. This is used for resizing the CaptureItem
      */
    function dragLength(delta, fixedPosition) {

        var sign = 1.0;

        switch(fixedPosition) {
        case Item.TopLeft:
            sign = -1;
            break;
        case Item.TopRight:
            delta.y = -delta.y;
            break;
        case Item.BottomLeft:
            delta.y = -delta.y;
            sign = -1;
            break;
        case Item.BottomRight:
            //Do nothing
            break
        default:
            console.error("FixedPosition is invalid:" + fixedPosition);
        }


        var length = 0;
        if(delta.y >= 0 && delta.x >= 0) {
            length = Math.max(delta.y, delta.x);
        } else if(delta.y <= 0 && delta.x <= 0) {
            length = Math.min(delta.y, delta.x);
        } else {
            if(Math.abs(delta.y) > Math.abs(delta.x)) {
                length = delta.y
            } else {
                length = delta.x
            }
        }
        return sign * length;
    }

    /**
      This converts the fixedPoint to a Qt.Point
      @param fixedPoint - Item.TransformOrigin. Only supports Item.TopLeft, Item.BottomLeft,
      Item.BottomLeft, and Item.TopLeft
      */
    function fixedPositionToPoint(fixedPosition) {
        var position = captureItem.positionOnPaper
        var size = captureItem.paperSizeOfItem

        switch(fixedPosition) {
        case Item.TopLeft:
            return position;
        case Item.TopRight:
            return Qt.point(position.x + size.width, position.y);
        case Item.BottomLeft:
            return Qt.point(position.x, position.y + size.height);
        case Item.BottomRight:
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
        return numberOfPixels / interactionId.captureScale
    }

    /**
      This handles the drag Resize of the catpureItem

      @param delta - Qt.point() with the delta of the mouse movement
      @param fixedPoint - Item.TransformOrigin
      */
    function dragResizeHandler(delta, fixedPoint) {
        var length = dragLength(delta, fixedPoint)

        var deltaOnPaper = pixelToPaper(length); //Convert maxDelta from pixels to paper units
        var newWidth = Math.max(captureItem.paperSizeOfItem.width + deltaOnPaper, 0.0);

        if(newWidth > 0.0) {
            var position = captureItem.positionOnPaper
            var size = captureItem.paperSizeOfItem
            var before = fixedPositionToPoint(fixedPoint)

            captureItem.setPaperWidthOfItem(newWidth);

            var sizeAfter = captureItem.paperSizeOfItem
            var after = fixedPositionToPoint(fixedPoint)

            captureItem.positionOnPaper = Qt.point(position.x + (before.x - after.x),
                                                   position.y + (before.y - after.y));
        }
    }

    width: 0
    height: 0
    x: 0
    y: 0

    border.width: 1
    border.color: "black"
    color: "#00000000"

    onSelectedChanged: {
        if(selected) {
            interactionId.state = "SELECTED_RESIZE_STATE"
        } else {
            interactionId.state = "INIT_STATE"
        }
    }

    MouseArea {
        id: selectMouseAreaId

        property point lastPoint;
        property bool positionHasChange: false

        /**
          This should be called on the onReleased.

          @return bool - True if the user has clicked and false if they haven't
          */
        function hasClicked() {
            return !positionHasChange
        }

        anchors.fill: parent
    }

    RectangleHandle {
        id: topLeftHandle
        anchors.bottom: interactionId.top
        anchors.right: interactionId.left
    }

    RectangleHandle {
        id: topRightHandle
        anchors.bottom: interactionId.top
        anchors.left: interactionId.right
    }

    RectangleHandle {
        id: bottomLeftHandle
        anchors.top: interactionId.bottom
        anchors.right: interactionId.left
    }

    RectangleHandle {
        id: bottomRightHandle
        anchors.top: interactionId.bottom
        anchors.left: interactionId.right
    }


    states: [
        State {
            name: "INIT_STATE"
            when: captureItem !== null

            PropertyChanges {
                target: interactionId
                width: captureItem.paperSizeOfItem.width * captureScale
                height: captureItem.paperSizeOfItem.height * captureScale;
                x: (captureItem.positionOnPaper.x - captureOffset.x) * captureScale;
                y: (captureItem.positionOnPaper.y - captureOffset.y) * captureScale
            }

            PropertyChanges {
                target: selectMouseAreaId

                onPressed: {
                    lastPoint = Utils.mousePositionToGlobal(selectMouseAreaId)
                    positionHasChange = false;
                }

                onPositionChanged: {
                    //Translate the item
                    var newPosition = Utils.mousePositionToGlobal(selectMouseAreaId);
                    var delta = Qt.point(pixelToPaper(newPosition.x - lastPoint.x),
                                         pixelToPaper(newPosition.y - lastPoint.y));
                    var origin = captureItem.positionOnPaper;

                    captureItem.positionOnPaper = Qt.point(origin.x + delta.x,
                                                           origin.y + delta.y)
                    lastPoint = newPosition
                    positionHasChange = true
                }

                onReleased: {
                    interactionId.selected = true
                }
            }
        },

        State {
            name: "SELECTED_RESIZE_STATE"
            extend: "INIT_STATE"

            PropertyChanges {
                target: selectMouseAreaId

                onReleased: {
                    if(hasClicked())
                    {
                        interactionId.state = "SELECTED_ROTATE_STATE"
                    }
                }
            }

            PropertyChanges {
                target: topLeftHandle
                imageSource: "qrc:icons/dragArrow/arrowHighLeftBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighLeft.png"
                onDragDelta: {
                    //Since we're moving to topleft, make the bottom right fixed
                    dragResizeHandler(delta, Item.BottomRight)
                }
            }
            PropertyChanges {
                target: topRightHandle
                imageSource: "qrc:icons/dragArrow/arrowHighRightBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighRight.png"
                onDragDelta: {
                    //Since we're moving the top right, make the bottom left fixed
                    dragResizeHandler(delta, Item.BottomLeft)
                }
            }
            PropertyChanges {
                target: bottomLeftHandle
                imageSource: "qrc:icons/dragArrow/arrowHighRightBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighRight.png"
                onDragDelta: {
                    //Since we're moving the bottom left, make the top right fixed
                    dragResizeHandler(delta, Item.TopRight)
                }
            }
            PropertyChanges {
                target: bottomRightHandle
                imageSource: "qrc:icons/dragArrow/arrowHighLeftBlack.png"
                selectedImageSource: "qrc:icons/dragArrow/arrowHighLeft.png"
                onDragDelta: {
                    //Since we're moving the bottom right, make the top left fixed
                    dragResizeHandler(delta, Item.TopLeft)
                }
            }

        },

        State {
            name: "SELECTED_ROTATE_STATE"
            extend: "INIT_STATE"

            PropertyChanges {
                target: selectMouseAreaId

                onReleased: {
                    if(hasClicked())
                    {
                        interactionId.state = "SELECTED_RESIZE_STATE"
                    }
                }
            }
        }

    ]



}
