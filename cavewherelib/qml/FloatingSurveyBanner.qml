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

// "This survey is not tied in", and the ties that would fix it — commits 6b
// and 6c of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html.
//
// A dumb presenter: whoever shows it does the lookup against
// FloatingSurveyModel and binds the properties. That keeps one banner serving
// both triggers and every surface — the panel here, the editor cell and the
// Health/Ties page later — none of which agree on where the answer comes from.
NoticeBanner {
    id: root
    objectName: "floatingSurveyBanner"

    // Whether the survey floats at all. Not derivable from stations: a survey
    // that is tied in has none to show here, and one whose file cannot be read
    // has none to show anywhere.
    required property bool floating

    // The floating stations, in the survey's own namespace.
    property list<string> stations: []

    // The ties that would end the float, ranked. Optional: a surface that only
    // reports the float leaves it null and the banner stays read-only.
    property TieSuggestionModel suggestions: null

    visible: floating
    color: Theme.warning
    title: qsTr("This survey is not tied in")
    titleObjectName: "floatingSurveyTitle"

    BodyText {
        objectName: "floatingSurveyDetail"
        Layout.fillWidth: true
        wrapMode: QC.Label.WordWrap
        // Says nothing about where the stations ended up: a floating
        // survey's names now arrive from the scan's harvest as well as from
        // the solve, and a harvested one was placed nowhere at all.
        text: root.stations.length > 0
              ? qsTr("Nothing joins it to the rest of the cave, so it can't be placed with the others.")
              : qsTr("Nothing joins it to the rest of the cave, and its stations couldn't be read from its file.")
    }

    QC.Label {
        objectName: "floatingSurveyStations"
        Layout.fillWidth: true
        visible: root.stations.length > 0
        elide: QC.Label.ElideRight
        font.pixelSize: Theme.fontSizeSmall
        text: qsTr("Floating: %1").arg(root.stations.join(", "))
    }

    QC.Label {
        objectName: "tieSuggestionsTitle"
        Layout.fillWidth: true
        Layout.topMargin: Theme.tightSpacing
        visible: suggestionsRepeaterId.count > 0
        font.bold: true
        wrapMode: QC.Label.WordWrap
        text: qsTr("Stations that look like the same point")
    }

    QQ.Repeater {
        id: suggestionsRepeaterId
        model: root.suggestions

        RowLayout {
            id: suggestionRowId
            objectName: "tieSuggestionRow"

            required property int index
            required property string station
            required property string candidateStation
            required property string candidateTripName
            required property int match

            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            QC.Label {
                objectName: "tieSuggestionText"
                Layout.fillWidth: true
                elide: QC.Label.ElideRight
                font.pixelSize: Theme.fontSizeSmall
                // The candidate is named with its trip because a bare tail
                // names no place: the same "a1" lives in every trip that
                // ever started at a1.
                text: suggestionRowId.match === TieSuggestionModel.SameName
                      ? qsTr("%1 and %2 in %3 are named alike")
                        .arg(suggestionRowId.station)
                        .arg(suggestionRowId.candidateStation)
                        .arg(suggestionRowId.candidateTripName)
                      : qsTr("%1 and %2 in %3 start alike")
                        .arg(suggestionRowId.station)
                        .arg(suggestionRowId.candidateStation)
                        .arg(suggestionRowId.candidateTripName)
            }

            QC.Button {
                objectName: "tieSuggestionConnectButton"
                text: qsTr("Connect")
                onClicked: root.suggestions.tieAt(suggestionRowId.index)
            }
        }
    }

    QC.Label {
        objectName: "tieSuggestionsTruncated"
        Layout.fillWidth: true
        visible: root.suggestions !== null && root.suggestions.truncated
        wrapMode: QC.Label.WordWrap
        font.pixelSize: Theme.fontSizeCaption
        text: qsTr("Only the first %1 matches are shown.")
              .arg(suggestionsRepeaterId.count)
    }

    BodyText {
        objectName: "tieSuggestionsEmpty"
        Layout.fillWidth: true
        Layout.topMargin: Theme.tightSpacing
        visible: root.suggestions !== null && suggestionsRepeaterId.count === 0
                 && root.stations.length > 0
        wrapMode: QC.Label.WordWrap
        // Not "will connect them": this banner only ever describes an
        // attached survey, and an attachment is always scoped, so matching
        // names stay separate stations until a tie is made. What a rename
        // buys is the one-click tie above.
        text: qsTr("No station in the rest of the cave is named like one of these. Renaming one to match will offer a one-click tie here.")
    }
}
