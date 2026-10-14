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

// Solve-status strip for a source root's card: colored status dot, station
// count and warning pill. Dumb data properties so the component tests can
// drive all three states; the card binds the error and warnings from the
// solve and the count from the root's own stations.
RowLayout {
    id: root
    objectName: "solveStatus"

    property bool hasError: false
    property int warningCount: 0
    property int stationCount: 0

    spacing: Theme.tightSpacing

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
}
