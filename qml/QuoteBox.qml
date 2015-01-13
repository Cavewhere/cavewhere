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
    property alias color: boxColorOverlay.color
    property alias borderColor: boxOutlineColorOverlay.color
    property alias borderWidth: box.borderWidth

    Item {
        id: itemWithoutShadow
        visible: false

        width: boxOutlineColorOverlay.width + 10
        height: boxOutlineColorOverlay.height + 10

        Item {
            id: boxOutline

            visible: false

            width: rectangleItem.width
            height: triangleItem.height + rectangleItem.height
            x: 5
            y: 5

            Image {
                id: triangleItem
                x: 20
                source: "qrc:/icons/quoteTriangle.png"
                sourceSize.height: 10;
                smooth: true
            }

            Rectangle {
                id: rectangleItem
                width: 200
                height: 50
                radius: 5
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
                width: rectangleItem.width - box.borderWidth * 1.5
                height: rectangleItem.height - box.borderWidth * 1.5
                radius: rectangleItem.radius - box.borderWidth
                x: box.borderWidth * 0.75
                y: triangleItem.height + box.borderWidth * 0.75
                color: "black"
            }
            visible: false
        }

        ColorOverlay {
            id: boxOutlineColorOverlay
            cached: true
            color: "gray"
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
        radius: 8
        samples: 32
        color: "#262626"
        source: itemWithoutShadow
    }



}

