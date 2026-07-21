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

    // A session-only, in-memory unit choice for THIS bar, set from the right-click
    // menu. FollowProject tracks the project (or app default) live;
    // ForceMetric/ForceImperial pin just this bar without writing the project.
    // Units.ScaleBarUnitMode is the shared enum the export bar (cwCaptureViewport)
    // uses too.
    property int unitMode: Units.FollowProject

    // The unit system the bar reads in, resolved from unitMode. The concrete
    // unit (m/km or ft/mi) is then chosen by magnitude.
    readonly property int unitSystem: {
        switch (itemId.unitMode) {
        case Units.ForceMetric:
            return Units.Metric;
        case Units.ForceImperial:
            return Units.Imperial;
        default:
            return ProjectUnits.unitSystem;
        }
    }

    QC.Menu {
        id: unitMenuId

        // One item per unit system. The item matching the project default is
        // annotated and, when chosen, returns the bar to following the project
        // (live) rather than pinning; the other item pins its system for this
        // bar. The item for the unit already shown is disabled (a no-op).
        QC.MenuItem {
            objectName: "scaleBarMetricMenuItem"
            readonly property bool isProjectDefault: ProjectUnits.unitSystem === Units.Metric

            text: Units.unitSystemName(Units.Metric)
                  + (isProjectDefault ? " - " + qsTr("Project Default") : "")
            enabled: itemId.unitSystem !== Units.Metric
            onTriggered: itemId.unitMode = isProjectDefault
                ? Units.FollowProject
                : Units.ForceMetric
        }

        QC.MenuItem {
            objectName: "scaleBarImperialMenuItem"
            readonly property bool isProjectDefault: ProjectUnits.unitSystem === Units.Imperial

            text: Units.unitSystemName(Units.Imperial)
                  + (isProjectDefault ? " - " + qsTr("Project Default") : "")
            enabled: itemId.unitSystem !== Units.Imperial
            onTriggered: itemId.unitMode = isProjectDefault
                ? Units.FollowProject
                : Units.ForceImperial
        }
    }

    QQ.QtObject {
        id: privateData

        // The round per-cell increment and its unit, chosen by the shared C++
        // producer so this on-screen bar rounds to the same nice numbers as the
        // printed export bar (cwScaleBarItem). The whole bar spans one
        // subdivided small cell plus largeGrid.columns large cells.
        readonly property scaleBarSelection selection: ScaleBarSelector.selectViewScale(
            itemId.camera.pixelsPerMeter,
            itemId.minTotalWidth,
            itemId.maxTotalWidth,
            1 + largeGrid.columns,
            itemId.unitSystem)

        property int displayUnit: selection.unit
        property real metersPerUnit: Units.convertLength(1.0, displayUnit, Units.Meters)
        property real pixelsPerUnit: itemId.camera.pixelsPerMeter * metersPerUnit
        property string unitName: Units.lengthUnitName(displayUnit)

        property real unitsPerCell: selection.value

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
