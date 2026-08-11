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

// Replaces SurveyEditor on TripPage for an externally-backed trip
// (master plan §8.5.1 + Phase-2 §9 deltas). Assembles the commit-10
// sub-components: attached header, solve status, scoped station list,
// and trip metadata. The Scope mode is Phase 3 — the header Loader is
// hard-coded to the attached header.
QQ.Item {
    id: root
    objectName: "externalCenterlineTripPanel"

    required property Trip trip
    property LinePlotManager linePlotManager: RootData.linePlotManager
    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager
    property ExternalSourceSettings externalSourceSettings: RootData.externalSourceSettings

    readonly property bool isAttached: trip !== null
                                       && trip.externalCenterline.entryFile.length > 0

    // isOwnerBusy has no property NOTIFY — ownerBusyChanged drives the
    // imperative refresh below.
    property bool ownerBusy: false

    // Defaults to true (file-owned) until the manager's scan says
    // otherwise; refreshed on solveNeeded, which the scan apply emits
    // whenever the declination flags change.
    property bool fileOwnsDeclination: true

    signal stationClicked(cwStationHandle stationHandle)

    function updateOwnerBusy() {
        ownerBusy = trip !== null && externalCenterlineManager.isOwnerBusy(trip.id)
    }

    function updateFileOwnsDeclination() {
        fileOwnsDeclination = trip === null
                || externalCenterlineManager.fileOwnsDeclination(trip.id)
    }

    onTripChanged: {
        updateOwnerBusy()
        updateFileOwnsDeclination()
    }

    QQ.Component.onCompleted: {
        updateOwnerBusy()
        updateFileOwnsDeclination()
    }

    QQ.Connections {
        target: root.externalCenterlineManager

        function onOwnerBusyChanged(ownerId) {
            root.updateOwnerBusy()
        }

        function onSolveNeeded() {
            root.updateFileOwnsDeclination()
        }
    }

    // The floating answer for this trip, as bindings. stationHandles is the
    // identity the suggester ties with; stations is only ever shown.
    FloatingSurveyStatus {
        id: floatingStatusId
        objectName: "floatingSurveyStatus"

        trip: root.trip
        model: root.linePlotManager.floatingSurveyModel
    }

    // The ties that would end the float. Live whether or not the trip floats —
    // the banner is what decides to show them, and asking only once it floats
    // would make the suggester's arrival a second thing to wait for.
    TieSuggestionModel {
        id: tieSuggestionsId
        objectName: "tieSuggestions"

        trip: root.trip
    }

    ScopeStationListModel {
        id: scopeStationModelId

        trip: root.trip
    }

    ColumnLayout {
        id: contentLayoutId
        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.sectionSpacing

        ExternalCenterlineFileErrorBanner {
            id: fileErrorBannerId
            Layout.fillWidth: true
            errorMessage: root.trip !== null ? root.trip.externalStationsError : ""
        }

        FloatingSurveyBanner {
            id: floatingBannerId
            Layout.fillWidth: true
            floating: floatingStatusId.floating
            stations: floatingStatusId.stations
            suggestions: tieSuggestionsId
        }

        QQ.Loader {
            id: headerLoaderId
            Layout.fillWidth: true
            // Phase 3 swaps in a scope header for prefix-scoped trips.
            sourceComponent: attachedHeaderComp
        }

        ExternalCenterlineSolveStatus {
            id: solveStatusId
            Layout.fillWidth: true
            hasError: root.linePlotManager.hasSolveError
            warningCount: root.linePlotManager.lastSolveWarningCount
            stationCount: root.linePlotManager.lastSolveStationCount
            onViewCavernOutputRequested: {
                RootData.pageSelectionModel.gotoPageByName(null, "Cavern")
            }
        }

        ExternalCenterlineStationsList {
            id: stationsListId
            Layout.fillWidth: true
            Layout.fillHeight: true
            stationModel: scopeStationModelId
            onStationClicked: (stationHandle) => root.stationClicked(stationHandle)
        }

        RowLayout {
            id: actionsRowId
            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            QC.Button {
                id: reloadButtonId
                objectName: "reloadButton"
                text: qsTr("Reload now")
                visible: root.isAttached
                enabled: !root.ownerBusy
                onClicked: root.linePlotManager.rerunSurvex()
            }

            QC.Button {
                id: replaceButtonId
                objectName: "replaceButton"
                text: qsTr("Replace…")
                visible: root.isAttached
                enabled: !root.ownerBusy
                onClicked: {
                    replaceDialogLoaderId.active = true
                    replaceDialogLoaderId.item.open()
                }
            }

            QC.Button {
                id: detachButtonId
                objectName: "detachButton"
                text: qsTr("Detach…")
                visible: root.isAttached
                enabled: !root.ownerBusy
                onClicked: {
                    const pos = detachButtonId.mapToItem(root, 0, detachButtonId.height)
                    detachAskBoxId.x = pos.x
                    detachAskBoxId.y = pos.y
                    detachAskBoxId.show()
                }
            }

            QQ.Item {
                Layout.fillWidth: true
            }
        }

        ExternalCenterlineTripMetadata {
            id: tripMetadataId
            Layout.fillWidth: true
            trip: root.trip
            fileOwnsDeclination: root.fileOwnsDeclination
        }
    }

    // Built on the first Replace click: every external trip page keeps a
    // panel alive, and the dialog carries a file dialog and a scan
    // preview that most of them never open.
    QQ.Loader {
        id: replaceDialogLoaderId
        active: false
        sourceComponent: QQ.Component {
            ReplaceCenterlineDialog {
                trip: root.trip
            }
        }
    }

    RemoveAskBox {
        id: detachAskBoxId
        objectName: "detachAskBox"
        message: qsTr("Detach this trip's centerline? The in-project copy will be removed.")
        confirmText: qsTr("Detach")
        onRemove: RootData.detachTripCenterline(root.trip)
    }

    QQ.Component {
        id: attachedHeaderComp
        ExternalCenterlineAttachedHeader {
            trip: root.trip
            externalSourceSettings: root.externalSourceSettings
        }
    }
}
