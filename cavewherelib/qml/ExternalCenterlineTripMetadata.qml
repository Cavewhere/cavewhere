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

// Trip metadata block for the external-centerline trip panel: date
// (drives auto-declination), declination, and team. The survey file
// owns the date and the team, so both are presented read-only; the
// declination is the one value CaveWhere may still supply. When the
// survey file carries its own declination directive (fileOwnsDeclination,
// from cwExternalCenterlineManager) the declination editor is
// replaced with a read-only hint — the file's value governs and
// CaveWhere injects nothing. A Scope trip windows a block of its cave's
// file, so its declination comes from there instead
// (caveOwnsDeclination); that state reads a sentence of its own.
ColumnLayout {
    id: root
    objectName: "tripMetadata"

    property Trip trip: null
    property bool fileOwnsDeclination: false

    // Whether the trip's declination comes from the cave's survey file
    // rather than from a file of the trip's own — the Scope case, where
    // the trip windows a block of the cave's file (§5 Q6). It reads its
    // own sentence, so it is a state apart from fileOwnsDeclination,
    // which an untracked owner defaults to true anyway.
    property bool caveOwnsDeclination: false

    spacing: Theme.tightSpacing

    RowLayout {
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "tripMetadataDateLabel"
            font.bold: true
            text: qsTr("Date")
        }

        QC.Label {
            objectName: "tripMetadataDate"
            text: root.trip !== null ? Qt.formatDate(root.trip.date, "yyyy-MM-dd") : ""
        }
    }

    DeclainationEditor {
        objectName: "tripMetadataDeclination"
        Layout.fillWidth: true
        visible: !root.fileOwnsDeclination && !root.caveOwnsDeclination
        calibration: root.trip !== null ? root.trip.calibration : null
    }

    QC.Label {
        objectName: "fileOwnsDeclinationHint"
        Layout.fillWidth: true
        visible: root.fileOwnsDeclination && !root.caveOwnsDeclination
        wrapMode: QC.Label.WordWrap
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.textSubtle
        text: qsTr("Declination is set by your survey file.")
    }

    QC.Label {
        objectName: "caveOwnsDeclinationHint"
        Layout.fillWidth: true
        visible: root.caveOwnsDeclination
        wrapMode: QC.Label.WordWrap
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.textSubtle
        text: qsTr("Declination comes from the cave's survey file.")
    }

    TeamTable {
        objectName: "tripMetadataTeam"
        Layout.fillWidth: true
        editable: false
        model: root.trip !== null ? root.trip.team : null
    }
}
