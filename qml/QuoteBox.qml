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

    property Item pointAtObject
    property point pointAtObjectPosition

    /**
      This points the arrow of the quote box at position relative to item
      */
    function pointAt(item, position) {
        if(item !== null) {
            var triangleCoords = item.mapToItem(triangleItem, position.x, position.y);

            //Top and center
            var pointX = triangleCoords.x - triangleItem.x - triangleItem.implicitWidth / 2 - shadowPadding;
            var pointY = triangleCoords.y - shadowPadding

            var mappedPoint = triangleItem.mapToItem(root, pointX, pointY);
            var x = root.x + mappedPoint.x
            var y = root.y + mappedPoint.y

            return Qt.point(x, y);
        }
        return Qt.point(0, 0)
    }

    x: pointAt(pointAtObject, pointAtObjectPosition).x
    y: pointAt(pointAtObject, pointAtObjectPosition).y

    onPointAtObjectChanged: {
        console.log("Object changed")
        pointAt(pointAtObject, pointAtObjectPosition)
    }

    onPointAtObjectPositionChanged: {
        console.log("Object Position changed" + pointAtObjectPosition)
        pointAt(pointAtObject, pointAtObjectPosition);
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
                x: 20
                source: "qrc:/icons/quoteTriangle.png"
                sourceSize.height: 10;
                smooth: true
            }

            Rectangle {
                id: rectangleItem
                width: childrenContainer.width + margin
                height: childrenContainer.height + margin
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
            visible: false
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
        anchors.fill: itemWithoutShadow

        clip: false
        cached: true
        horizontalOffset: 2
        verticalOffset: 2
        radius: 5
        samples: 32
        color: "#262626"
        source: itemWithoutShadow
    }

    Item {
        id: childrenContainer
        width: childrenRect.width + margin
        height: childrenRect.height + margin
        x: margin + shadowPadding
        y: rectangleItem.y + margin + shadowPadding
    }
}

