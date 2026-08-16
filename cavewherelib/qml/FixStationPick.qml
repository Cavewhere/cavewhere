/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma Singleton

import QtQuick as QQ
import cavewherelib

// Which fix station the next click on the terrain places. The one piece of
// state the pick spans two pages with: the user starts it on the Fix Stations
// page or in the fixed-station popup, answers it in the 3D view, and lands back
// where they started.
//
// The trip itself goes through ViewTools, which carries no arguments — so the
// target lives here, typed, and the tool that reads it is the only thing that
// needs to know what a fix-station pick is about.
//
// The fix travels as its own id rather than as a row: the user is free to add
// and remove rows before coming back, and an index would quietly follow that.
// It is spelled as a string because that is what QML can hold — see
// cwFixStationModel::setPickedPoint(), which takes the same spelling.
QQ.QtObject {
    id: fixStationPick

    property Cave cave: null
    property string fixId: ""

    // The station the pick will place, for the tool's banner to name. Recorded
    // at begin() rather than looked up later: it is what the user asked for.
    property string stationName: ""

    // A pick is the fix it names, so there is no separate flag to keep in step.
    readonly property bool active: fixStationPick.cave !== null && fixStationPick.fixId !== ""

    // Ask for a coordinate for row \a row of \a cave's fixes, and head for the
    // view. A second begin() replaces the first: there is one pick, and the last
    // button pressed is the one the user is waiting on.
    function begin(cave: Cave, row: int): void {
        if (cave === null || cave.fixStations === null
                || row < 0 || row >= cave.fixStations.count) {
            console.warn("FixStationPick.begin() was given no fix to place")
            return
        }

        let model = cave.fixStations
        let index = model.index(row)

        fixStationPick.cave = cave
        fixStationPick.fixId = model.data(index, FixStationModel.IdRole)
        fixStationPick.stationName = model.data(index, FixStationModel.StationNameRole)

        ViewTools.arm(ViewTools.fixStationPick)
    }

    // Drop the pick, whether it was answered or not. A no-op when nothing is
    // pending, so every path that ends one — Escape, leaving the view, the pick
    // itself — can call it without checking first.
    function cancel(): void {
        fixStationPick.cave = null
        fixStationPick.fixId = ""
        fixStationPick.stationName = ""

        // Only our own ask: another surface may have armed its tool in the
        // meantime, and that one is still wanted.
        if (ViewTools.pendingTool === ViewTools.fixStationPick) {
            ViewTools.cancelArm()
        }
    }
}
