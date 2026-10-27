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
// (drives auto-declination), declination, and team. The survey file owns
// the date and the team, so both are read-only. The declination is
// editable only while CaveWhere supplies it (an auto or manual origin);
// otherwise the page reads the value cavern applies, in the words the
// survey tree's Decl cell uses.
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

        QC.Label {
            objectName: "tripMetadataDate"
            text: root.trip !== null ? Qt.formatDate(root.trip.date, "yyyy-MM-dd") : ""
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
