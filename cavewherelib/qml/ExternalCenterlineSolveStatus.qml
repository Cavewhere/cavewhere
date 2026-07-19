/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Solve-status strip for the external-centerline trip panel: colored
// status dot, station count, warning pill, and a "View cavern output"
// deep link. Dumb data properties so the component tests can drive all
// three states; the panel binds them from cwLinePlotManager
// (hasSolveError / lastSolveStationCount / lastSolveWarningCount).
RowLayout {
    id: root
    objectName: "solveStatus"

    property bool hasError: false
    property int warningCount: 0
    property int stationCount: 0

    spacing: Theme.tightSpacing

    signal viewCavernOutputRequested()

    QQ.Rectangle {
        id: statusDotId
        objectName: "solveStatusDot"

        readonly property int dotSize: Theme.fontSizeCaption

        implicitWidth: dotSize
        implicitHeight: dotSize
        radius: dotSize / 2
        color: root.hasError ? Theme.danger
                             : root.warningCount > 0 ? Theme.warning
                                                     : Theme.success
    }

    QC.Label {
        objectName: "solveStatusLabel"
        text: root.hasError ? qsTr("Solve failed")
                            : qsTr("%1 stations").arg(root.stationCount)
    }

    QC.Label {
        objectName: "solveWarningPill"
        visible: !root.hasError && root.warningCount > 0
        color: Theme.textSecondary
        text: qsTr("%1 warnings").arg(root.warningCount)
    }

    QQ.Item { Layout.fillWidth: true }

    LinkText {
        objectName: "viewCavernOutputLink"
        text: qsTr("View cavern output")
        onClicked: root.viewCavernOutputRequested()
    }
}
