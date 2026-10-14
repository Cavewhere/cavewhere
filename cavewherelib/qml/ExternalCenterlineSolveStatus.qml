/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC

// Solve status for a source root's card: the station count, or "Solve failed".
QC.Label {
    id: root
    objectName: "solveStatusLabel"

    property bool hasError: false
    property int stationCount: 0

    text: root.hasError ? qsTr("Solve failed")
                        : qsTr("%1 stations").arg(root.stationCount)
}
