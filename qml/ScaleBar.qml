import QtQuick 2.0
import Cavewhere 1.0

Item {
    id: itemId

    property Camera camera;

    width: childrenRect.width
    height: childrenRect.height

    property real maxTotalWidth: itemId.parent.width * 0.75
    property real minTotalWidth: itemId.parent.width * 0.2

    QtObject {
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


    Repeater {
        model: 2
        Rectangle {
            width: childrenRect.width
            height: childrenRect.height
            y: -height
            x: -width / 2 + index * privateData.smallCellWidth * 2
            radius: 3
            Text {
                text: index * privateData.meterPerCell / 2
                anchors.centerIn: parent
            }
        }
    }

    Repeater {
        model: 5

        Rectangle {
            width: childrenRect.width
            height: childrenRect.height
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
                anchors.centerIn: parent
            }
        }
    }

    Grid {
        id: smallGrid
        x: 1
        y: 1

        rows: 2
        columns: 4

        Repeater {
            model: smallGrid.rows * smallGrid.columns
            Rectangle {
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

    Grid {
        id: largeGrid

        anchors.left: smallGrid.right
        anchors.top:  smallGrid.top

        rows: 2
        columns: smallGrid.columns

        Repeater {
            model: largeGrid.rows * largeGrid.columns
            Rectangle {
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

    Rectangle {

        width: smallGrid.width + largeGrid.width + 2
        height: privateData.cellHeight * 2.0 + 2

        border.width: 1
        color: "#00000000"
    }
}
