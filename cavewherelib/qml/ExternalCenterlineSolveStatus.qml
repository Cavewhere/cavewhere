/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC
import cavewherelib

// Solve status for a source root's card: the station count, a dash while the
// count waits on the first solve, or "Solve failed".
QC.Label {
    id: root
    objectName: "solveStatusLabel"

    property bool hasError: false
    property int stationCount: 0
    property bool stationCountKnown: true

    readonly property bool placeholder: !root.hasError && !root.stationCountKnown

    text: {
        if(root.hasError) {
            return qsTr("Solve failed");
        }
        return root.placeholder ? qsTr("— stations")
                                : qsTr("%1 stations").arg(root.stationCount);
    }
    color: root.placeholder ? Theme.textSubtle : root.palette.windowText
}
