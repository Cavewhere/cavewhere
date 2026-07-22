/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

// Presentation-only prompt shown when a project has fix stations but no output
// coordinate system, so its caves can't be placed on the map. It carries an
// inline projected-CS picker pre-filled with the suggestion derived from the
// first fix; the host wires the region's fix-station validator to it:
// `suggestedCS` seeds the picker, `coordinateInvalid` grays it out when the
// source coordinate is bad, and `useSuggested(cs)` adopts the picker's choice.
QQ.Rectangle {
    id: rootId

    property string suggestedCS: ""
    // The first fix's coordinate is outside its input CS's valid domain — the
    // suggestion can't be trusted, so the picker grays out and the prompt points
    // the user at the coordinate instead.
    property bool coordinateInvalid: false
    // The picker's live selection, seeded from the suggestion and updated as the
    // user tweaks it, so "Use this" applies exactly what's shown.
    property string pendingCS: rootId.suggestedCS

    signal useSuggested(string cs)

    color: Theme.info
    radius: 5
    clip: true
    implicitHeight: layoutId.implicitHeight + Theme.statsPadding * 2

    onSuggestedCSChanged: rootId.pendingCS = rootId.suggestedCS

    RowLayout {
        id: layoutId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: Theme.statsPadding
        spacing: Theme.flowSpacing

        QQ.Image {
            source: "qrc:/icons/svg/warning.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            Layout.alignment: Qt.AlignTop
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.tightSpacing

            BodyText {
                objectName: "outputCSPromptMessage"
                Layout.fillWidth: true
                wrapMode: QC.Label.WordWrap
                text: qsTr("This project has no output coordinate system, so its caves "
                           + "can't be placed on the map.")
            }

            // Guidance for a bad source coordinate: no suggestion can be derived,
            // so point the user at the fix's coordinate first.
            BodyText {
                objectName: "outputCSCoordinateHelp"
                visible: rootId.coordinateInvalid
                Layout.fillWidth: true
                wrapMode: QC.Label.WordWrap
                text: qsTr("A fix station's coordinate falls outside its input coordinate "
                           + "system's valid range, so no output system can be suggested. "
                           + "Correct the coordinate on the Fix Stations page, then pick an "
                           + "output system here.")
            }

            // A RowLayout, not a Flow: in a slim host (the Data page's info
            // column) the picker must shrink so its own controls wrap onto a
            // second line, rather than keep its one-line width and let the banner
            // clip the trailing hemisphere combo. maximumWidth caps it at that
            // one-line width so a wide host keeps the button right beside it.
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.flowSpacing

                CSPicker {
                    id: pickerId
                    objectName: "outputCSPicker"
                    Layout.fillWidth: true
                    Layout.maximumWidth: pickerId.implicitWidth
                    enabled: !rootId.coordinateInvalid
                    // survex/cavern only emits projected output — no geographic.
                    allowGeographic: false
                    value: rootId.pendingCS
                    onCommitted: (newCS) => rootId.pendingCS = newCS
                }

                QC.Button {
                    id: useButtonId
                    objectName: "outputCSUseButton"
                    Layout.alignment: Qt.AlignVCenter
                    Layout.minimumWidth: useButtonId.implicitWidth
                    enabled: !rootId.coordinateInvalid && rootId.pendingCS !== ""
                    text: qsTr("Use this")
                    onClicked: rootId.useSuggested(rootId.pendingCS)
                }
            }

            // The picker's current selection resolved to a friendly name and its
            // authority code, so the abstract mode/zone controls name a concrete
            // system. Bound to pendingCS, so tweaking the zone updates them.
            QC.Label {
                objectName: "outputCSSuggestionName"
                visible: !rootId.coordinateInvalid && rootId.pendingCS !== ""
                         && CSFormat.hasName(rootId.pendingCS)
                Layout.fillWidth: true
                wrapMode: QC.Label.WordWrap
                text: CSFormat.displayName(rootId.pendingCS)
            }

            QC.Label {
                objectName: "outputCSSuggestionCode"
                visible: !rootId.coordinateInvalid && rootId.pendingCS !== ""
                Layout.fillWidth: true
                color: Theme.textSubtle
                font.family: Theme.fontFamilyMono
                text: rootId.pendingCS
            }
        }
    }
}
