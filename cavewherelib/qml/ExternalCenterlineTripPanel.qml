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
// sub-components: missing-source banner, attached header, solve
// status, scoped station list, and trip metadata. The Scope mode is
// Phase 3 — the header Loader is hard-coded to the attached header.
QQ.Item {
    id: root
    objectName: "externalCenterlineTripPanel"

    required property Trip trip
    property LinePlotManager linePlotManager: RootData.linePlotManager
    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager
    property ExternalSourceSettings externalSourceSettings: RootData.externalSourceSettings

    readonly property bool isAttached: trip !== null
                                       && trip.externalCenterline.entryFile.length > 0
    readonly property bool sourceMissing: {
        if (!isAttached) {
            return false
        }
        // Dependency for re-evaluation; membership needs the invokable.
        externalCenterlineManager.missingSourceOwners
        return externalCenterlineManager.isSourceMissing(trip.id)
    }

    // isOwnerBusy has no property NOTIFY — ownerBusyChanged drives the
    // imperative refresh below.
    property bool ownerBusy: false

    // Defaults to true (file-owned) until the manager's scan says
    // otherwise; refreshed on solveNeeded, which the scan apply emits
    // whenever the declination flags change.
    property bool fileOwnsDeclination: true

    // sourcePathFor has no property backing (§15 observable
    // remembered-source follow-up), so a declarative binding would go
    // stale when the settings store changes under a live panel — e.g.
    // the attach writes the source path right after entryFile flips
    // the panel in. Same imperative idiom as
    // ExternalCenterlineAttachedHeader.
    property string missingSourcePath: ""

    signal relinkRequested()
    signal stationClicked(string qualifiedName)

    function updateOwnerBusy() {
        ownerBusy = trip !== null && externalCenterlineManager.isOwnerBusy(trip.id)
    }

    function updateFileOwnsDeclination() {
        fileOwnsDeclination = trip === null
                || externalCenterlineManager.fileOwnsDeclination(trip.id)
    }

    function updateMissingSourcePath() {
        missingSourcePath = trip !== null
                ? externalSourceSettings.sourcePathFor(trip.id)
                : ""
    }

    onTripChanged: {
        updateOwnerBusy()
        updateFileOwnsDeclination()
        updateMissingSourcePath()
    }

    QQ.Component.onCompleted: {
        updateOwnerBusy()
        updateFileOwnsDeclination()
        updateMissingSourcePath()
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

    QQ.Connections {
        target: root.externalSourceSettings

        function onExternalCenterlineSourcesChanged() {
            root.updateMissingSourcePath()
        }
    }

    ScopeStationListModel {
        id: scopeStationModelId
        scopePrefix: root.trip !== null
                     ? root.externalCenterlineManager.scopePrefixForTrip(root.trip)
                     : ""
        network: root.linePlotManager.regionNetwork
    }

    ColumnLayout {
        id: contentLayoutId
        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.sectionSpacing

        ExternalCenterlineMissingSourceBanner {
            id: missingSourceBannerId
            Layout.fillWidth: true
            visible: root.sourceMissing
            sourcePath: root.missingSourcePath
            onRelinkRequested: root.relinkRequested()
            onForgetSourceRequested: root.externalSourceSettings.clearSourcePath(root.trip.id)
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
            onStationClicked: (qualifiedName) => root.stationClicked(qualifiedName)
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
