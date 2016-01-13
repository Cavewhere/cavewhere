import QtQuick 2.0
import Cavewhere 1.0
import QtGraphicalEffects 1.0

/**

  This will make a box around you qml elements like this.
     /\
  --/  \-------------
  |  Child items    |
  -------------------
  */



Item {
    id: root
    default property alias defaultChildren: childrenContainer.children
    property alias color: boxColorOverlay.color
    property alias borderColor: boxOutlineColorOverlay.color
    property alias borderWidth: box.borderWidth
    property alias radius: rectangleItem.radius
    property double margin: 5
    property double shadowPadding: 5
    property int triangleEdge: Qt.TopEdge //Can be TopEdge, Left, Bottom, Right

    /**
      Triangle offset positions the triangle, relative to the triangleAlignment. This parameter


      When triangleAlignment == Qt.TopEdge, 0.0 = top left, 1.0 = top right
      When triangleAlignment == Qt.LeftEdge, 0.0 = top left, 1.0 = bottom left
      When triangleAlignment == Qt.RightEdge, 0.0 = top right, 1.0 = bottom right
      When triangleAlignment == Qt.BottomEdge, 0.0 = bottom left, 1.0 = bottom right
      */
    property double triangleOffset: .1

    //These two properties will position the QuoteBox's arrow to point to pointAtObject with
    //and offset of pointAtObjectPosition
    property Item pointAtObject
    property point pointAtObjectPosition

    function updatePosition() {
        if(!visible) { return; }
        var newPos = pointAt(pointAtObject, pointAtObjectPosition)
        x = newPos.x
        y = newPos.y
    }

    /**
      This points the arrow of the quote box at position relative to item
      */
    function pointAt(item, position) {
        if(item !== null) {

            var globalPos = item.mapToItem(null, position.x, position.y)
            var globalTrianglePos = arrowTipId.mapToItem(null, 0, 0)

            var x = root.x + (globalPos.x - globalTrianglePos.x)
            var y = root.y + (globalPos.y - globalTrianglePos.y)

            return Qt.point(x, y);
        }
        return Qt.point(0, 0)
    }

    onPointAtObjectChanged: {
        updatePosition()
    }

    onPointAtObjectPositionChanged: {
        updatePosition()
    }

    onVisibleChanged: {
        updatePosition()
    }

    Item {
        id: itemWithoutShadow
        visible: false

        width: boxOutlineColorOverlay.width + shadowPadding * 2
        height: boxOutlineColorOverlay.height + shadowPadding * 2

        Item {
            id: boxOutline

            visible: false

            width: rectangleItem.width
            height: triangleItem.height + rectangleItem.height
            x: shadowPadding
            y: shadowPadding

            Image {
                id: triangleItem
                x: rectangleItem.width * triangleOffset
                source: "qrc:/icons/quoteTriangle.png"
                sourceSize.height: 10;
                smooth: true
            }

            Rectangle {
                id: rectangleItem
                width: {
                    switch(triangleEdge) {
                    case Qt.TopEdge:
                    case Qt.BottomEdge:
                        return childrenContainer.width + margin
                    case Qt.LeftEdge:
                    case Qt.RightEdge:
                        return childrenContainer.height + margin
                    default:
                        return 0
                    }
                }
                height: {
                        switch(triangleEdge) {
                        case Qt.TopEdge:
                        case Qt.BottomEdge:
                            return childrenContainer.height + margin
                        case Qt.LeftEdge:
                        case Qt.RightEdge:
                            return childrenContainer.width + margin
                        default:
                            return 0
                        }
                    }
                radius: 0
                y: triangleItem.height
                color: "black"
            }
        }

        Item {
            id: box

            property int borderWidth: 1.0

            anchors.centerIn: boxOutline

            width: rectangleItem.width
            height: triangleItem.height + rectangleItem.height
            visible: false

            Image {
                id: triangleItemBorder
                x: triangleItem.x
                y: box.borderWidth
                source: "qrc:/icons/quoteTriangle.png"
                sourceSize.height: triangleItem.sourceSize.height;

                smooth: true
            }

            Rectangle {
                id: rectangleItemBorder
                width: rectangleItem.width - box.borderWidth * 2
                height: rectangleItem.height - box.borderWidth * 2
                radius: rectangleItem.radius - box.borderWidth
                x: box.borderWidth
                y: triangleItem.height + box.borderWidth
                color: "black"
            }
        }

        ColorOverlay {
            id: boxOutlineColorOverlay
            cached: true
            color: "darkgray"
            source: boxOutline
            anchors.fill: boxOutline
        }

        ColorOverlay {
            id: boxColorOverlay
            cached: true
            color: "white"
            source: box
            anchors.fill: box
        }
    }


    DropShadow {
        id: dropShadowId

        anchors.fill: itemWithoutShadow

        clip: false
        cached: true
        horizontalOffset: 2 * Math.cos(Math.PI / 180.0 * rotationId.angle)
                          + 2 * Math.sin(Math.PI / 180.0 * rotationId.angle)
        verticalOffset: 2 * Math.cos(Math.PI / 180.0 * rotationId.angle)
                        - 2 * Math.sin(Math.PI / 180.0 * rotationId.angle)
        radius: 5
        samples: 32
        color: "#262626"
        source: itemWithoutShadow

        //This item is used to position the QuoteBox. This item is locate at the tip of the
        //arrow of the quotebox
        Item {
            id: arrowTipId
            x: rectangleItem.width * triangleOffset + triangleItem.implicitWidth * 0.5 + shadowPadding
            y: shadowPadding - 2
        }

        transform: [
            Rotation {
                id: rotationId
                angle:  {
                    switch(triangleEdge) {
                    case Qt.TopEdge:
                        return 0
                    case Qt.BottomEdge:
                        return 180
                    case Qt.LeftEdge:
                        return 270
                    case Qt.RightEdge:
                        return 90
                    default:
                        return 0
                    }
                }
                origin.x: 0
                origin.y: 0

                onAngleChanged:  {
                    root.updatePosition()
                }
            },

            Translate {
                x: {
                    switch(triangleEdge) {
                    case Qt.TopEdge:
                        return 0;
                    case Qt.BottomEdge:
                        return dropShadowId.width
                    case Qt.LeftEdge:
                        return -triangleItem.height
                    case Qt.RightEdge:
                        return dropShadowId.height
                    default:
                        return 0
                    }
                }

                y: {
                    switch(triangleEdge) {
                    case Qt.TopEdge:
                        return 0;
                    case Qt.BottomEdge:
                        return dropShadowId.height + triangleItem.height
                    case Qt.LeftEdge:
                        return dropShadowId.width + triangleItem.height
                    case Qt.RightEdge:
                        return triangleItem.height
                    default:
                        return 0
                    }
                }

                onXChanged: {
                    root.updatePosition()
                }

                onYChanged:  {
                    root.updatePosition()
                }
            }
        ]

    }

    Item {
        id: childrenContainer
        width: childrenRect.width + margin
        height: childrenRect.height + margin
        x: margin + shadowPadding
        y: rectangleItem.y + margin + shadowPadding
    }

    Component.onCompleted: {
        root.updatePosition()
    }
}

