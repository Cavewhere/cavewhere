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
// sub-components, top to bottom: the header, solve status, trip metadata,
// and the scoped station list. The whole column scrolls.
//
// A trip reaches this panel in one of two modes, and the header Loader is
// where they part: Attached, where the trip owns a survey file of its own
// (file name, format, Replace… and where the copy came from), and Scope,
// where the trip windows one block of its cave's file (the cave it belongs
// to, its station prefix, and Remove trip). Everything that acts on a file
// of the trip's own — the missing-copy and file-error banners, Replace,
// the provenance line — belongs to Attached alone.
QQ.Item {
    id: root
    objectName: "externalCenterlineTripPanel"

    required property Trip trip
    property LinePlotManager linePlotManager: RootData.linePlotManager
    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager
    property ExternalSourceSettings externalSourceSettings: RootData.externalSourceSettings

    // What the manager knows about this trip, all of it read
    // imperatively; see ExternalCenterlineOwnerState.
    readonly property bool ownerBusy: ownerStateId.ownerBusy
    readonly property bool fileOwnsDeclination: ownerStateId.fileOwnsDeclination
    readonly property string missingCopyPath: ownerStateId.missingCopyPath
    readonly property string entryFilePath: ownerStateId.entryFilePath
    readonly property bool sourceChangedSinceCopy: ownerStateId.sourceChangedSinceCopy

    // The two modes, discriminated on the fields that hold them apart:
    // scopePrefix() answers non-empty for both, so it cannot tell them
    // apart. Attached wins when a trip somehow carries both, matching
    // cwTrip::isScoped's own field pair.
    readonly property bool isAttached: trip !== null
                                       && trip.externalCenterline.entryFile.length > 0
    readonly property bool isScope: !isAttached && trip !== null
                                    && trip.stationPrefix.length > 0

    signal stationClicked(cwStationHandle stationHandle)

    function openReplaceDialog() {
        replaceDialogLoaderId.active = true
        replaceDialogLoaderId.item.open()
    }

    ExternalCenterlineOwnerState {
        id: ownerStateId

        owner: root.trip
        manager: root.externalCenterlineManager
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
                // A Scope trip has no copy of its own to be missing, and
                // the banner already hides itself on an empty path.
                missingPath: root.isAttached ? root.missingCopyPath : ""
                onReplaceRequested: root.openReplaceDialog()
            }

            ExternalCenterlineFileErrorBanner {
                id: fileErrorBannerId
                Layout.fillWidth: true
                // Likewise: a Scope trip owns no file to have failed. The
                // cave's error is reported on the cave's own page.
                errorMessage: root.isAttached && root.trip !== null
                              ? root.trip.externalStationsError : ""
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
                sourceComponent: root.isAttached ? attachedHeaderComp : scopeHeaderComp
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
                caveOwnsDeclination: !root.isAttached
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
                owner: root.trip

                // Freed once it is off the screen, for the same reason it is
                // built late. Deferred out of the close handler so the popup
                // is not destroyed from inside its own emission.
                onClosed: Qt.callLater(() => replaceDialogLoaderId.active = false)
            }
        }
    }

    QQ.Component {
        id: scopeHeaderComp
        ExternalCenterlineScopeHeader {
            trip: root.trip
        }
    }

    QQ.Component {
        id: attachedHeaderComp
        ExternalCenterlineAttachedHeader {
            id: attachedHeaderItemId

            owner: root.trip
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
