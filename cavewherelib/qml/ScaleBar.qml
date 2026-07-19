/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
QQ.Item {
    id: itemId

    property Camera camera;

    width: childrenRect.width
    height: childrenRect.height

    property real maxTotalWidth: itemId.parent.width * 0.75
    property real minTotalWidth: itemId.parent.width * 0.2

    // A session-only, in-memory unit override for THIS bar, chosen from the
    // right-click menu. It never writes the project or app default.
    property bool hasSessionOverride: false
    property int sessionUnitSystem: Units.Metric

    // The unit system the bar reads in: the session override when set, else the
    // project's. The concrete unit (m/km or ft/mi) is then chosen by magnitude.
    readonly property int unitSystem: itemId.hasSessionOverride
        ? itemId.sessionUnitSystem
        : ProjectUnits.unitSystem

    QC.Menu {
        id: unitMenuId

        QC.MenuItem {
            text: "Follow Project Default"
            enabled: itemId.hasSessionOverride
            onTriggered: itemId.hasSessionOverride = false
        }

        QC.MenuSeparator {}

        QC.MenuItem {
            text: Units.unitSystemName(Units.Metric)
            onTriggered: {
                itemId.sessionUnitSystem = Units.Metric;
                itemId.hasSessionOverride = true;
            }
        }

        QC.MenuItem {
            text: Units.unitSystemName(Units.Imperial)
            onTriggered: {
                itemId.sessionUnitSystem = Units.Imperial;
                itemId.hasSessionOverride = true;
            }
        }
    }

    QQ.QtObject {
        id: privateData

        // Choose the display unit from the bar's target span, so a large scale
        // reads in km/mi and a small one in m/ft (magnitude-aware).
        property int displayUnit: {
            var ppm = itemId.camera.pixelsPerMeter;
            var target = itemId.maxTotalWidth - itemId.minTotalWidth / 2.0;
            var metersAcross = ppm > 0.0 ? target / ppm : 0.0;
            return Units.lengthDisplayUnit(metersAcross, itemId.unitSystem);
        }
        property real metersPerUnit: Units.convertLength(1.0, displayUnit, Units.Meters)
        property real pixelsPerUnit: itemId.camera.pixelsPerMeter * metersPerUnit
        property string unitName: Units.lengthUnitName(displayUnit)

        property real unitsPerCell: {
            if(itemId.visible) {
                //Go through the exponents, working in the display unit so the
                //cell labels land on round numbers of that unit.
                var labelIncrement = 0.0 //In displayUnit
                var nearestToBest = 10000.0 //In pixels

                var numberOfLargeCells = 5; //Number of cells in the scale bar
                var increments = [1.0, 2.5, 5.0];

                var bestWidth = itemId.maxTotalWidth - itemId.minTotalWidth / 2.0;

                //Search for the best label incement
                for(var i = 0; i < increments.length; i++) {
                    for(var ii = -5; ii < 10; ii++) {
                        var currentLabelIncrement = increments[i] * Math.pow(10.0, ii); //In displayUnit
                        var currentCellWidth = currentLabelIncrement * pixelsPerUnit;
                        var currentTotalWidth = currentCellWidth * numberOfLargeCells;

                        if(currentTotalWidth >= itemId.minTotalWidth && currentTotalWidth <= itemId.maxTotalWidth) {
                            var currentNearest = Math.abs(bestWidth - currentTotalWidth);
                            if(currentNearest < nearestToBest) {
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
        property real cellWidth: pixelsPerUnit * unitsPerCell
        property real cellHeight: 8
    }


    QQ.Repeater {
        model: 2
        QQ.Rectangle {
            id: rect2Id
            color: Theme.background

            required property int index

            y: -height
            x: -width / 2 + index * privateData.smallCellWidth * 2
            radius: 3

            QC.Label {
                text: rect2Id.index * privateData.unitsPerCell / 2
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

            required property int index

            y: -height
            x: -width / 2 + (index + 1) * privateData.cellWidth
            radius: 3
            color: Theme.background

            QC.Label {
                text: {
                    var text = (rect1Id.index + 1) * privateData.unitsPerCell;
                    if(rect1Id.index == 4) {
                        return text + privateData.unitName;
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

                required property int index

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
                required property int index

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
        color: Theme.transparent

        QQ.TapHandler {
            objectName: "scaleBarUnitTapHandler"
            acceptedButtons: Qt.RightButton
            onTapped: unitMenuId.popup()
        }
    }
}
