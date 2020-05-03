/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0

QQ.Item {
    id: itemId

    property Camera camera;

    width: childrenRect.width
    height: childrenRect.height

    property real maxTotalWidth: itemId.parent.width * 0.75
    property real minTotalWidth: itemId.parent.width * 0.2

    QQ.QtObject {
        id: privateData
        property real meterPerCell: {
            if(itemId.visible) {
                //Go through the exponents
                var bestCellSizeWidth = 0.0 //In pixels
                var labelIncrement = 0.0 //In meters
                var nearestToBest = 10000.0 //In pixels

                var numberOfLargeCells = 5; //Number of cells in the scale bar
                var increments = [1.0, 2.5, 5.0];

                var bestWidth = maxTotalWidth - minTotalWidth / 2.0;

                //Search for the best label incement
                for(var i = 0; i < increments.length; i++) {
                    for(var ii = -5; ii < 10; ii++) {
                        var currentLabelIncrement = increments[i] * Math.pow(10.0, ii); //In meters
                        var currentCellWidth = currentLabelIncrement * camera.pixelsPerMeter;
                        var currentTotalWidth = currentCellWidth * numberOfLargeCells;

                        if(currentTotalWidth >= minTotalWidth && currentTotalWidth <= maxTotalWidth) {
                            var currentNearest = Math.abs(bestWidth - currentTotalWidth);
                            if(currentNearest < nearestToBest) {
                                bestCellSizeWidth = currentCellWidth;
                                labelIncrement = currentLabelIncrement;
                                nearestToBest = currentNearest;
                            }
                        }
                    }
                }

                return labelIncrement;
            }

            return 10.0;
        }

        property real smallCellWidth: cellWidth / smallGrid.columns
        property real cellWidth: camera.pixelsPerMeter * meterPerCell
        property real cellHeight: 8
    }


    QQ.Repeater {
        model: 2
        QQ.Rectangle {
            id: rect2Id
            y: -height
            x: -width / 2 + index * privateData.smallCellWidth * 2
            radius: 3
            Text {
                text: index * privateData.meterPerCell / 2
                anchors.centerIn: parent

                onTextChanged:  {
                    rect2Id.width = width;
                    rect2Id.height = height;
                }

                QQ.Component.onCompleted: {
                    rect2Id.width = width;
                    rect2Id.height = height;
                }
            }
        }
    }

    QQ.Repeater {
        model: 5

        QQ.Rectangle {
            id: rect1Id
            y: -height
            x: -width / 2 + (index + 1) * privateData.cellWidth
            radius: 3
            Text {
                text: {
                    var text = (index + 1) * privateData.meterPerCell;
                    if(index == 4) {
                        return text + "m";
                    }
                    return text
                }
                onTextChanged:  {
                    rect1Id.width = width;
                    rect1Id.height = height;
                }

                anchors.centerIn: parent
            }
        }
    }

   QQ.Grid {
        id: smallGrid
        x: 1
        y: 1

        rows: 2
        columns: 4

        QQ.Repeater {
            model: smallGrid.rows * smallGrid.columns
            QQ.Rectangle {
                id: smallRectangle
                width: privateData.smallCellWidth
                height: privateData.cellHeight
                color: {
                    if(index >= smallGrid.columns) {
                        return index % 2 == 0 ? "black" : "white";
                    }
                    return index % 2 == 1 ? "black" : "white";
                }
            }
        }
    }

   QQ.Grid {
        id: largeGrid

        anchors.left: smallGrid.right
        anchors.top:  smallGrid.top

        rows: 2
        columns: smallGrid.columns

        QQ.Repeater {
            model: largeGrid.rows * largeGrid.columns
            QQ.Rectangle {
                width: privateData.cellWidth
                height: privateData.cellHeight
                color: {
                    if(index >= largeGrid.columns) {
                        return index % 2 == 0 ? "black" : "white";
                    }
                    return index % 2 == 1 ? "black" : "white";
                }
            }
        }
    }

    QQ.Rectangle {

        width: smallGrid.width + largeGrid.width + 2
        height: privateData.cellHeight * 2.0 + 2

        border.width: 1
        color: "#00000000"
    }
}
