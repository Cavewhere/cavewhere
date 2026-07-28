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

    // Edits go to fixStations; the warnings come from the diagnostics proxy
    // over it, the same rows FixStationPage reads (U12). Row indices pass
    // through unchanged, so popupId.row addresses both.
    readonly property FixStationDiagnosticsModel diagnostics: popupId.cave !== null
                                                              ? popupId.cave.fixStationDiagnostics
                                                              : null

    // What's wrong with this fix right now, worst first: text that wouldn't
    // parse beats a coordinate outside its CS, which beats a station name the
    // cave doesn't have. Before U12 the popup showed none of these, so a fix
    // typed here looked identical whether or not it was usable.
    property string parseError: ""
    property string domainError: ""
    property string stationError: ""

    // Which axis the field leads with. A geographic CS is written latitude
    // first, a projected one easting first, and the numbers alone can't say
    // which — so the label below tells the user. It is only the label: the
    // model reads and writes the coordinate under the row's own CS, so nothing
    // that decides what a number means comes from this property.
    readonly property int axisOrder: CoordinateText.axisOrderFor(csPickerId.value)

    readonly property string errorMessage: popupId.parseError !== ""
                                           ? popupId.parseError
                                           : (popupId.domainError !== ""
                                              ? popupId.domainError
                                              : popupId.stationError)

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
        popupId.parseError = ""
        popupId.reloadCoordinate()
        popupId.reloadErrors()
    }

    // Re-renders the field from the model. Kept apart from reload() because a
    // CS change must not run it while a parse error is pending: the model never
    // received that text, so re-rendering would replace what the user still has
    // to correct with the coordinate they were trying to replace.
    function reloadCoordinate(): void {
        const model = popupId.fixStations
        if (model === null || popupId.row < 0) {
            return
        }

        const modelIndex = model.index(popupId.row)
        //Read the CS into a local rather than off the picker, so the order this
        //renders in comes from the same read as the numbers. A commit doesn't
        //reuse this read at all — setCoordinateText() takes the order from the
        //row itself, which is what stops a stale picker from deciding what a
        //coordinate means.
        const inputCS = model.data(modelIndex, FixStationModel.InputCSRole)
        csPickerId.value = inputCS

        //This field is always an editor — there is no display half to keep in
        //project units — so it shows the coordinate as it was written. A row
        //with none has nothing entered yet, and renders as three zeros.
        const typed = model.data(modelIndex, FixStationModel.CoordinateTextRole)
        coordinateFieldId.text = typed !== ""
                ? typed
                : CoordinateText.format(
                      model.data(modelIndex, FixStationModel.EastingRole),
                      model.data(modelIndex, FixStationModel.NorthingRole),
                      model.data(modelIndex, FixStationModel.ElevationRole),
                      ProjectUnits.unitSystem,
                      CoordinateText.axisOrderFor(inputCS))
    }

    // The derived warnings, unlike the coordinate, change without anyone
    // touching this popup — a re-solve or a CS change moves them — so they are
    // re-read whenever the proxy says so, not only on open.
    function reloadErrors(): void {
        const model = popupId.diagnostics
        if (model === null || popupId.row < 0) {
            popupId.domainError = ""
            popupId.stationError = ""
            return
        }

        const modelIndex = model.index(popupId.row)
        popupId.domainError = model.data(modelIndex, FixStationDiagnosticsModel.DomainErrorRole)
        popupId.stationError = model.data(modelIndex, FixStationDiagnosticsModel.StationErrorRole)
    }

    function commit(role: int, value: var): void {
        const model = popupId.fixStations
        if (model === null || popupId.row < 0) {
            return
        }
        model.setData(model.index(popupId.row), value, role)
    }

    // Writes the whole coordinate in one edit, or leaves the row alone and
    // reports why. No QValidator here on purpose: QC.TextField withholds
    // editingFinished while a validator reports unacceptable input, which would
    // swallow the very message this is here to show (see cwCoordinateText).
    function commitCoordinate(text: string): void {
        const model = popupId.fixStations
        if (model === null || popupId.row < 0) {
            return
        }
        popupId.parseError = model.setCoordinateText(
                    popupId.row, text, ProjectUnits.unitSystem)
    }

    QQ.Connections {
        target: popupId.diagnostics

        function onDataChanged(topLeft: var, bottomRight: var): void {
            if (popupId.row >= topLeft.row && popupId.row <= bottomRight.row) {
                popupId.reloadErrors()
            }
        }
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
                    //The CS write re-read the coordinate under the other axis
                    //order, which moves the numbers and leaves the string
                    //alone, so this re-render is really about the picker and
                    //the label. It is still guarded: only text the model
                    //actually took can be re-rendered, and changing the CS is
                    //the natural next move after a refusal, so it must not eat
                    //the coordinate that was refused.
                    if (popupId.parseError === "") {
                        popupId.reloadCoordinate()
                    }
                    popupId.reloadErrors()
                }
            }

            QC.Label {
                objectName: "fixStationPopupCoordinateLabel"
                text: popupId.axisOrder === CoordinateText.LatitudeLongitude
                      ? qsTr("Lat, Long, Elev")
                      : qsTr("East, North, Elev")
            }

            QC.TextField {
                id: coordinateFieldId
                objectName: "fixStationPopupCoordinate"

                Layout.preferredWidth: Theme.fixPopupCoordinateWidth

                color: popupId.parseError !== "" ? Theme.errorText : Theme.text

                onEditingFinished: popupId.commitCoordinate(coordinateFieldId.text)
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

        // U12 — the one place this editor admits something is wrong. It sits
        // below the CS name so the layout doesn't jump when it appears.
        RowLayout {
            objectName: "fixStationPopupError"

            Layout.fillWidth: true
            Layout.maximumWidth: Theme.fixPopupCoordinateWidth
            spacing: Theme.tightSpacing

            visible: popupId.errorMessage !== ""

            QQ.Image {
                Layout.alignment: Qt.AlignTop
                source: "qrc:icons/svg/warning.svg"
                sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            }

            QC.Label {
                objectName: "fixStationPopupErrorText"
                Layout.fillWidth: true
                color: Theme.errorText
                wrapMode: QC.Label.WordWrap
                // Every message here quotes something the user typed — the
                // refused text, or a station name — and AutoText would render
                // "<b>" in it as markup rather than as characters.
                textFormat: QC.Label.PlainText
                text: popupId.errorMessage
            }
        }

        QC.Button {
            objectName: "fixStationPopupDone"
            Layout.alignment: Qt.AlignRight
            text: qsTr("Done")
            onClicked: popupId.close()
        }
    }
}
