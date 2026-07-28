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

// "This survey is not tied in" — commit 6b of
// plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html. Read-only for now; the
// suggester that offers a one-click connect lands here next.
//
// A dumb presenter: whoever shows it does the lookup against
// FloatingSurveyModel and binds the two properties. That keeps one banner
// serving both triggers and every surface — the panel here, the editor cell
// and the Health/Ties page later — none of which agree on where the answer
// comes from.
QQ.Rectangle {
    id: root
    objectName: "floatingSurveyBanner"

    // Whether the survey floats at all. Not derivable from stations: cavern
    // drops a survey nothing fixes and nothing ties in, and that one floats
    // with no stations to show.
    required property bool floating

    // The floating stations, in the survey's own namespace.
    property list<string> stations: []

    visible: floating
    color: Theme.warning
    radius: 5
    implicitHeight: contentLayoutId.implicitHeight + Theme.sectionSpacing * 2

    ColumnLayout {
        id: contentLayoutId
        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "floatingSurveyTitle"
            Layout.fillWidth: true
            font.bold: true
            wrapMode: QC.Label.WordWrap
            text: qsTr("This survey is not tied in")
        }

        BodyText {
            objectName: "floatingSurveyDetail"
            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            text: root.stations.length > 0
                  ? qsTr("Nothing joins it to the rest of the cave, so its stations were placed in a frame of their own.")
                  : qsTr("Nothing joins it to the rest of the cave and nothing fixes it, so none of its stations were placed.")
        }

        QC.Label {
            objectName: "floatingSurveyStations"
            Layout.fillWidth: true
            visible: root.stations.length > 0
            elide: QC.Label.ElideRight
            font.pixelSize: Theme.fontSizeSmall
            text: qsTr("Floating: %1").arg(root.stations.join(", "))
        }
    }
}
