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
// sub-components, top to bottom: attached header (file name, format and
// Replace…), solve status, trip metadata, and the scoped station list.
// The whole column scrolls. The Scope mode is Phase 3 — the header
// Loader is hard-coded to the attached header.
QQ.Item {
    id: root
    objectName: "externalCenterlineTripPanel"

    required property Trip trip
    property LinePlotManager linePlotManager: RootData.linePlotManager
    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager
    property ExternalSourceSettings externalSourceSettings: RootData.externalSourceSettings

    // isOwnerBusy has no property NOTIFY — ownerBusyChanged drives the
    // imperative refresh below.
    property bool ownerBusy: false

    // Defaults to true (file-owned) until the manager's scan says
    // otherwise; refreshed on solveNeeded, which the scan apply emits
    // whenever the declination flags change.
    property bool fileOwnsDeclination: true

    // Project-relative path of this trip's in-project copy while that file
    // is gone from disk; empty otherwise. missingCopiesChanged drives the
    // refresh, the same imperative shape as ownerBusy.
    property string missingCopyPath: ""

    // Absolute on-disk path of the trip's entry file — the attachment
    // directory the manager holds, joined with the project-relative
    // entryFile. Refreshed imperatively: attachmentDir has no NOTIFY, and
    // an attach that fills it lands with solveNeeded.
    property string entryFilePath: ""

    // Whether the file this trip's copy came from has moved on since the
    // copy. Read from the manager's status model, whose statusFor has no
    // NOTIFY — statusesChanged drives the refresh, the same imperative
    // shape as ownerBusy.
    property bool sourceChangedSinceCopy: false

    signal stationClicked(cwStationHandle stationHandle)

    function updateSourceChangedSinceCopy() {
        sourceChangedSinceCopy = trip !== null
                && externalCenterlineManager.sourceStatusModel.statusFor(trip.id)
                   === ExternalSourceStatusModel.Changed
    }

    function updateOwnerBusy() {
        ownerBusy = trip !== null && externalCenterlineManager.isOwnerBusy(trip.id)
    }

    function updateMissingCopyPath() {
        missingCopyPath = trip === null
                ? "" : externalCenterlineManager.missingCopyPath(trip.id)
    }

    function updateEntryFilePath() {
        entryFilePath = trip === null
                ? "" : FileRevealer.resolvedPath(
                           externalCenterlineManager.attachmentDir(trip.id),
                           trip.externalCenterline.entryFile)
    }

    function openReplaceDialog() {
        replaceDialogLoaderId.active = true
        replaceDialogLoaderId.item.open()
    }

    function updateFileOwnsDeclination() {
        fileOwnsDeclination = trip === null
                || externalCenterlineManager.fileOwnsDeclination(trip.id)
    }

    // Everything the manager answers imperatively, read in one go — for a
    // trip that just arrived, and for this panel's own first frame.
    function refreshFromManager() {
        updateOwnerBusy()
        updateFileOwnsDeclination()
        updateMissingCopyPath()
        updateEntryFilePath()
        updateSourceChangedSinceCopy()
    }

    onTripChanged: refreshFromManager()

    QQ.Component.onCompleted: refreshFromManager()

    QQ.Connections {
        target: root.externalCenterlineManager.sourceStatusModel

        function onStatusesChanged() {
            root.updateSourceChangedSinceCopy()
        }
    }

    QQ.Connections {
        target: root.externalCenterlineManager

        function onOwnerBusyChanged(ownerId) {
            root.updateOwnerBusy()
        }

        function onSolveNeeded() {
            root.updateFileOwnsDeclination()
            root.updateEntryFilePath()
        }

        function onMissingCopiesChanged() {
            root.updateMissingCopyPath()
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

    // Every block stays reachable on a short viewport: the whole column
    // scrolls here (§16 B2d), and the station list caps its own height
    // rather than filling, so this is the only vertical scroll that
    // matters. contentWidth follows the viewport, so the page never
    // scrolls sideways.
    QC.ScrollView {
        id: panelScrollId
        objectName: "panelScrollView"

        anchors.fill: parent
        padding: Theme.sectionSpacing
        clip: true
        contentWidth: panelScrollId.availableWidth

        ColumnLayout {
            id: contentLayoutId
            width: panelScrollId.availableWidth
            spacing: Theme.sectionSpacing

            MissingCenterlineCopyBanner {
                id: missingCopyBannerId
                Layout.fillWidth: true
                missingPath: root.missingCopyPath
                onReplaceRequested: root.openReplaceDialog()
            }

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

            ExternalCenterlineTripMetadata {
                id: tripMetadataId
                Layout.fillWidth: true
                trip: root.trip
                fileOwnsDeclination: root.fileOwnsDeclination
            }

            ExternalCenterlineStationsList {
                id: stationsListId
                Layout.fillWidth: true
                stationModel: scopeStationModelId
                onStationClicked: (stationHandle) => root.stationClicked(stationHandle)
            }
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

                // Freed once it is off the screen, for the same reason it is
                // built late. Deferred out of the close handler so the popup
                // is not destroyed from inside its own emission.
                onClosed: Qt.callLater(() => replaceDialogLoaderId.active = false)
            }
        }
    }

    QQ.Component {
        id: attachedHeaderComp
        ExternalCenterlineAttachedHeader {
            id: attachedHeaderItemId

            trip: root.trip
            externalSourceSettings: root.externalSourceSettings
            actionsEnabled: !root.ownerBusy
            entryFilePath: root.entryFilePath
            sourceChangedSinceCopy: root.sourceChangedSinceCopy

            onReplaceRequested: root.openReplaceDialog()

            onReloadFromSourceRequested: {
                root.externalCenterlineManager.reloadFromSource(root.trip)
            }

            // canReloadFromSource follows the disk and has no change
            // signal, so the header asks at the moment its menu opens.
            onSourceEligibilityRefreshRequested: {
                attachedHeaderItemId.sourceReloadable =
                        root.externalCenterlineManager.canReloadFromSource(root.trip)
            }
        }
    }
}
