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
import "Utils.js" as Utils

// A trip's declination as the survey tree's Decl cell and the trip page both
// read it: the angle, then a caption naming where it comes from, or an em dash
// when the scan cannot tell. The one place that text is composed, so the two
// surfaces always agree.
QQ.Item {
    id: root

    property real declination: 0.0
    property int origin: Trip.DeclinationUnknown
    property QQ.color valueColor: Theme.text

    // Names the inner labels for tests to find; a host whose delegates are
    // pooled turns it off so a parked delegate is never found.
    property bool named: true

    readonly property bool known: origin !== Trip.DeclinationUnknown

    implicitWidth: rowLayoutId.implicitWidth
    implicitHeight: rowLayoutId.implicitHeight

    function labelName(name: string) : string {
        return root.named ? name : "";
    }

    RowLayout {
        id: rowLayoutId

        anchors.fill: parent
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: root.labelName("value")
            visible: root.known
            text: Utils.fixed(root.declination, 2) + "°"
            color: root.valueColor
        }

        QC.Label {
            objectName: root.labelName("declinationMode")
            visible: root.known
            text: {
                switch (root.origin) {
                case Trip.DeclinationAuto:
                    return qsTr("auto")
                case Trip.DeclinationManual:
                    return qsTr("manual")
                case Trip.DeclinationFile:
                    return qsTr("file")
                case Trip.DeclinationFileAuto:
                    return qsTr("file auto")
                case Trip.DeclinationNone:
                    return qsTr("none")
                default:
                    return ""
                }
            }
            color: Theme.textSubtle
            font.pixelSize: Theme.fontSizeCaption
        }

        QC.Label {
            objectName: root.labelName("declinationEmDash")
            visible: !root.known
            text: "—"
            color: Theme.textSubtle
        }
    }
}
