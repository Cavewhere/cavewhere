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
// seeds the date, and the user owns it from there: it stays editable so a
// trip whose file writes no date, or writes the wrong one, can be
// corrected by hand (§5 Q6). The team stays the file's, so it is
// presented read-only. The declination is editable only while CaveWhere
// supplies it (an auto or manual origin); otherwise the page reads the
// value cavern applies, in the words the survey tree's Decl cell uses.
ColumnLayout {
    id: root
    objectName: "tripMetadata"

    property Trip trip: null

    readonly property bool caveWhereOwnsDeclination: trip !== null
                                                     && (trip.declinationOrigin === Trip.DeclinationAuto
                                                         || trip.declinationOrigin === Trip.DeclinationManual)

    spacing: Theme.tightSpacing

    RowLayout {
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "tripMetadataDateLabel"
            font.bold: true
            text: qsTr("Date")
        }

        DoubleClickTextInput {
            objectName: "tripMetadataDate"

            text: root.trip !== null ? Qt.formatDate(root.trip.date, "yyyy-MM-dd") : ""

            onFinishedEditting: function(newText) {
                if(root.trip === null) {
                    return
                }
                root.trip.date = Date.fromLocaleString(Qt.locale(),
                                                       newText + " 00:00:00",
                                                       "yyyy-MM-dd HH:mm:ss")
            }
        }
    }

    DeclainationEditor {
        objectName: "tripMetadataDeclination"
        Layout.fillWidth: true
        visible: root.caveWhereOwnsDeclination
        calibration: root.trip !== null ? root.trip.calibration : null
    }

    RowLayout {
        visible: !root.caveWhereOwnsDeclination
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "tripMetadataDeclinationLabel"
            font.bold: true
            text: qsTr("Declination")
        }

        DeclinationReadout {
            objectName: "tripMetadataDeclinationReadout"
            declination: root.trip !== null ? root.trip.appliedDeclination : 0.0
            origin: root.trip !== null ? root.trip.declinationOrigin : Trip.DeclinationUnknown
        }
    }

    TeamTable {
        objectName: "tripMetadataTeam"
        Layout.fillWidth: true
        editable: false
        model: root.trip !== null ? root.trip.team : null
    }
}
