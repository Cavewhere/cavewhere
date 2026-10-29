/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Inline label entry for one entrance, opened from a survey-table station
// cell's caret. Edits go straight to the cave's cwEntranceModel.
//
// Hosted at page level (SurveyEditor) for the same reason FixStationPopup is:
// the survey table destroys cell delegates as they scroll out of view.
QC.Popup {
    id: popupId
    objectName: "entrancePopup"

    property Cave cave

    readonly property EntranceModel entrances: popupId.cave !== null
                                               ? popupId.cave.entrances
                                               : null

    // Which station is being labeled, and its row. Both are set by openFor().
    property string stationName: ""
    property int row: -1

    focus: true
    padding: Theme.statsPadding
    closePolicy: QC.Popup.CloseOnEscape

    // Opens the editor for `name`, anchored under `anchorItem`, marking an
    // unmarked station an entrance first: creating the entrance is the point
    // of opening this.
    function openFor(name: string, anchorItem: QQ.Item): void {
        if (popupId.entrances === null) {
            return
        }

        popupId.stationName = name
        popupId.row = popupId.entrances.addEntrance(name)
        if (popupId.row < 0) {
            return
        }

        labelFieldId.text = popupId.entrances.data(popupId.entrances.index(popupId.row),
                                                   EntranceModel.LabelRole)
        popupId.moveTo(anchorItem)
        popupId.open()
    }

    // Drops the station's entrance. Closes first when it is the station on
    // screen, so the close's commit runs while the row still exists.
    function removeEntranceFor(name: string): void {
        if (popupId.entrances === null) {
            return
        }

        if (popupId.opened && popupId.stationName === name) {
            popupId.close()
        }
        popupId.entrances.removeEntrance(name)
    }

    // The row is looked up again by name, since an entrance removed elsewhere
    // while this was open shifts the rows below it.
    function commitLabel(text: string): void {
        const model = popupId.entrances
        if (model === null || popupId.stationName === "") {
            return
        }

        popupId.row = model.indexOf(popupId.stationName)
        if (popupId.row < 0) {
            return
        }

        const modelIndex = model.index(popupId.row)
        if (model.data(modelIndex, EntranceModel.LabelRole) !== text) {
            model.setData(modelIndex, text, EntranceModel.LabelRole)
        }
    }

    // Positioned by hand rather than with a parent anchor: the cell that opens
    // this can be destroyed while the popup is still up.
    function moveTo(anchorItem: QQ.Item): void {
        if (anchorItem === null || popupId.parent === null) {
            return
        }
        const corner = popupId.parent.mapFromItem(anchorItem, 0, anchorItem.height)
        popupId.x = Math.max(0, Math.min(corner.x, popupId.parent.width - popupId.width))
        popupId.y = Math.max(0, Math.min(corner.y, popupId.parent.height - popupId.height))
    }

    // A trip switch rebinds the cave under us. The open station belongs to the
    // old cave, so the close that follows has no row to write to.
    onCaveChanged: {
        popupId.stationName = ""
        popupId.row = -1
        popupId.close()
    }

    // Escape and Done both keep what was typed.
    onAboutToHide: popupId.commitLabel(labelFieldId.text)

    contentItem: ColumnLayout {
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "entrancePopupTitle"
            Layout.fillWidth: true
            font.bold: true
            elide: QC.Label.ElideRight
            text: qsTr("Entrance %1").arg(popupId.stationName)
        }

        QC.Label {
            text: qsTr("Label")
        }

        QC.TextField {
            id: labelFieldId
            objectName: "entrancePopupLabel"
            Layout.preferredWidth: Theme.entrancePopupLabelWidth
            placeholderText: qsTr("Cave name")

            onEditingFinished: popupId.commitLabel(labelFieldId.text)
        }

        QC.Button {
            objectName: "entrancePopupDone"
            Layout.alignment: Qt.AlignRight
            text: qsTr("Done")
            onClicked: popupId.close()
        }
    }
}
