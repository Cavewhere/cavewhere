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

// Inline coordinate entry for one fix station, so marking a station Fixed from
// the survey table doesn't send the user to FixStationPage and back. Edits go
// straight to the cave's cwFixStationModel — the same rows FixStationPage shows,
// so the two surfaces can't disagree.
//
// Hosted at page level (SurveyEditor) rather than inside the StationBox that
// opens it: the survey table destroys cell delegates as they scroll out of view,
// which would take an open popup down with them.
QC.Popup {
    id: popupId
    objectName: "fixStationPopup"

    property Cave cave

    readonly property FixStationModel fixStations: popupId.cave !== null
                                                   ? popupId.cave.fixStations
                                                   : null

    // Which station is being fixed, and the row that anchors it. Both are set by
    // openFor(); the row is re-resolved on every open rather than kept, since a
    // fix added or removed on FixStationPage would shift a stored index.
    property string stationName: ""
    property int row: -1

    focus: true
    padding: Theme.statsPadding
    // Not CloseOnPressOutside: the coordinate-system picker opens popups of its
    // own (the mode combo, the Custom CRS dialog), and a press in one of those is
    // a press outside this. Dismissal is Escape, the Done button, or moving to
    // another cell — see SurveyEditor.
    closePolicy: QC.Popup.CloseOnEscape

    // A trip switch rebinds the cave under us; the open row belongs to the old
    // one, so don't keep editing it.
    onCaveChanged: popupId.close()

    // Opens the editor for `name`, anchored under `anchorItem`, marking the
    // station Fixed first if it isn't already — for an unfixed station, creating
    // the fix is the point of opening this.
    function openFor(name: string, anchorItem: QQ.Item): void {
        const model = popupId.fixStations
        if (model === null) {
            return
        }

        popupId.stationName = name
        popupId.row = model.addFixStation(name)
        if (popupId.row < 0) {
            return
        }

        popupId.reload()
        popupId.moveTo(anchorItem)
        popupId.open()
    }

    // Drops the fix anchoring `name`. The survey table's Remove Fix goes through
    // the editor rather than reaching past it for the model, so a cell only ever
    // needs the one handle. Closes first when it's the row on screen — the popup
    // edits by row index, which the removal invalidates.
    function removeFixFor(name: string): void {
        const model = popupId.fixStations
        if (model === null) {
            return
        }

        if (popupId.opened && popupId.stationName === name) {
            popupId.close()
        }
        model.removeFixStation(name)
    }

    // Positioned by hand rather than with a parent anchor: the caret that opens
    // this lives in a cell that can be destroyed while the popup is still up, so
    // the position is taken once and not tracked.
    function moveTo(anchorItem: QQ.Item): void {
        if (anchorItem === null || popupId.parent === null) {
            return
        }
        const corner = popupId.parent.mapFromItem(anchorItem, 0, anchorItem.height)
        popupId.x = Math.max(0, Math.min(corner.x, popupId.parent.width - popupId.width))
        popupId.y = Math.max(0, Math.min(corner.y, popupId.parent.height - popupId.height))
    }

    // Fields are filled on open instead of bound to the model: the roles are
    // read through data(), which no binding can depend on. Nothing else edits
    // these rows while the popup is up, so one read is enough.
    function reload(): void {
        const model = popupId.fixStations
        if (model === null || popupId.row < 0) {
            return
        }

        const modelIndex = model.index(popupId.row)
        csPickerId.value = model.data(modelIndex, FixStationModel.InputCSRole)
        eastingFieldId.text = model.data(modelIndex, FixStationModel.EastingRole)
        northingFieldId.text = model.data(modelIndex, FixStationModel.NorthingRole)
        elevationFieldId.text = model.data(modelIndex, FixStationModel.ElevationRole)
    }

    function commit(role: int, value: var): void {
        const model = popupId.fixStations
        if (model === null || popupId.row < 0) {
            return
        }
        model.setData(model.index(popupId.row), value, role)
    }

    component CoordinateField : QC.TextField {
        id: fieldId
        required property int role

        Layout.preferredWidth: Theme.fixPopupFieldWidth

        // Validated, so editingFinished can't fire on text Number() would turn
        // into NaN — a NaN easting propagates straight into the line plot.
        validator: QQ.DoubleValidator {}

        onEditingFinished: popupId.commit(fieldId.role, Number(fieldId.text))
    }

    contentItem: ColumnLayout {
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "fixStationPopupTitle"
            Layout.fillWidth: true
            font.bold: true
            elide: QC.Label.ElideRight
            text: qsTr("Fix station %1").arg(popupId.stationName)
        }

        GridLayout {
            columns: 2
            columnSpacing: Theme.flowSpacing
            rowSpacing: Theme.tightSpacing

            QC.Label { text: qsTr("Input CS") }

            CSPicker {
                id: csPickerId
                objectName: "fixStationPopupCS"
                allowGeographic: true
                // CSPicker doesn't own its value — the table rows feed it back from
                // the model role they're bound to. This editor fills its fields by
                // hand, so it has to close that loop itself or the controls would
                // keep showing the CS the fix had before the user changed it.
                onCommitted: (newCS) => {
                    csPickerId.value = newCS
                    popupId.commit(FixStationModel.InputCSRole, newCS)
                }
            }

            QC.Label { text: qsTr("Easting / Long") }

            CoordinateField {
                id: eastingFieldId
                objectName: "fixStationPopupEasting"
                role: FixStationModel.EastingRole
            }

            QC.Label { text: qsTr("Northing / Lat") }

            CoordinateField {
                id: northingFieldId
                objectName: "fixStationPopupNorthing"
                role: FixStationModel.NorthingRole
            }

            QC.Label { text: qsTr("Elevation") }

            CoordinateField {
                id: elevationFieldId
                objectName: "fixStationPopupElevation"
                role: FixStationModel.ElevationRole
            }
        }

        // Resolves the picker's controls to a concrete system, the same way the
        // project CS GroupBox and OutputCSPrompt do, so "UTM zone 16 N" reads as
        // a named CRS before the user commits to it.
        QC.Label {
            objectName: "fixStationPopupCSName"
            visible: csPickerId.value !== "" && CSFormat.hasName(csPickerId.value)
            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            text: CSFormat.displayName(csPickerId.value)
        }

        QC.Button {
            objectName: "fixStationPopupDone"
            Layout.alignment: Qt.AlignRight
            text: qsTr("Done")
            onClicked: popupId.close()
        }
    }
}
