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

    // The name the entrance marker falls back to when the label is empty, as
    // it read when openFor() ran.
    property string caveName: ""

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

        popupId.caveName = popupId.currentCaveName()
        const label = popupId.entrances.data(popupId.entrances.index(popupId.row),
                                             EntranceModel.LabelRole)
        labelFieldId.text = popupId.shownLabel(label)
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

    // What the marker shows for a stored label: the label, else the cave name.
    function shownLabel(label: string): string {
        return label !== "" ? label : popupId.caveName
    }

    function currentCaveName(): string {
        return popupId.cave !== null ? popupId.cave.caveNode().name : ""
    }

    // The cave name in any capitalization stores an empty label, so the marker
    // follows a cave rename. The name shown at open counts too, in case the
    // cave was renamed while this was up.
    function storedLabel(text: string): string {
        const trimmed = text.trim()
        const lowered = trimmed.toLowerCase()
        const matchesCaveName = lowered === popupId.caveName.trim().toLowerCase()
                              || lowered === popupId.currentCaveName().trim().toLowerCase()
        return trimmed === "" || matchesCaveName ? "" : trimmed
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

        const label = popupId.storedLabel(text)
        const modelIndex = model.index(popupId.row)
        if (model.data(modelIndex, EntranceModel.LabelRole) !== label) {
            model.setData(modelIndex, label, EntranceModel.LabelRole)
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
            placeholderText: popupId.caveName

            onEditingFinished: {
                popupId.commitLabel(labelFieldId.text)
                labelFieldId.text = popupId.shownLabel(popupId.storedLabel(labelFieldId.text))
            }
        }

        QC.Button {
            objectName: "entrancePopupDone"
            Layout.alignment: Qt.AlignRight
            text: qsTr("Done")
            onClicked: popupId.close()
        }
    }
}
