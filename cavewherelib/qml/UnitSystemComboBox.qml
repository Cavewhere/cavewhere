/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
import QtQuick.Controls as QC
import cavewherelib

// A Metric/Imperial drop-down that single-sources its labels from the cwUnits
// enum. Callers bind currentIndex and handle onActivated. By default the index
// maps directly to Units.Metric (0) / Units.Imperial (1); see includeFollowProject.
QC.ComboBox {
    id: controlId

    // Prepend a "Project Default" entry at index 0, shifting Metric/Imperial to
    // 1/2 — the index layout of Units.ScaleBarUnitMode.
    property bool includeFollowProject: false

    model: controlId.includeFollowProject
        ? [qsTr("Project Default"),
           Units.unitSystemName(Units.Metric),
           Units.unitSystemName(Units.Imperial)]
        : [Units.unitSystemName(Units.Metric),
           Units.unitSystemName(Units.Imperial)]
}
