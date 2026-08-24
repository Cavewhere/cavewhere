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

// Cave-level state for an externally-backed cave, shown on CavePage
// (plans/EXTERNAL_FILE_PHASE3.html P3.8): the attached header (entry
// file, format, Replace…, and where the copy came from), the solve
// status of the last cavern run, and Detach…, which drops the whole
// attachment — the copied files and every trip the attach created —
// and leaves the cave behind as an empty native one.
QQ.Item {
    id: root
    objectName: "externalCaveSummary"

    property Cave cave: null
    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager
    property ExternalSourceSettings externalSourceSettings: RootData.externalSourceSettings
    property LinePlotManager linePlotManager: RootData.linePlotManager

    implicitWidth: contentColumnId.implicitWidth
    implicitHeight: contentColumnId.implicitHeight

    function openReplaceDialog() {
        replaceDialogLoaderId.active = true
        replaceDialogLoaderId.item.open()
    }

    // The trips the detach cascade removes: the ones the attach created
    // to window a block, which carry a station prefix and hold no chunks
    // of their own. Counted at the moment the prompt opens, so the number
    // names exactly what this click is about to remove.
    function autoCreatedTripCount() {
        if (cave === null) {
            return 0
        }
        let count = 0
        for (let i = 0; i < cave.rowCount(); i++) {
            const trip = cave.trip(i)
            if (trip.stationPrefix.length > 0 && trip.chunkCount === 0) {
                count++
            }
        }
        return count
    }

    function askToDetach() {
        const count = root.autoCreatedTripCount()
        const trips = count === 1
                ? qsTr("1 auto-created trip")
                : qsTr("%1 auto-created trips").arg(count)
        detachChallengeId.message =
                qsTr("Detach external centerline from <b>%1</b>? Removes %2 "
                     + "and their notes and scraps, and deletes the copied "
                     + "files.")
                .arg(root.cave !== null ? root.cave.name : "")
                .arg(trips)
        detachChallengeId.show()
    }

    // What the manager knows about this cave's attachment, all of it
    // read imperatively.
    ExternalCenterlineOwnerState {
        id: ownerStateId

        owner: root.cave
        manager: root.externalCenterlineManager
    }

    ColumnLayout {
        id: contentColumnId

        anchors.fill: parent
        spacing: Theme.tightSpacing

        ExternalCenterlineAttachedHeader {
            id: attachedHeaderId

            Layout.fillWidth: true

            owner: root.cave
            externalSourceSettings: root.externalSourceSettings
            actionsEnabled: !ownerStateId.ownerBusy
            entryFilePath: ownerStateId.entryFilePath
            sourceChangedSinceCopy: ownerStateId.sourceChangedSinceCopy

            onReplaceRequested: root.openReplaceDialog()

            onReloadFromSourceRequested: {
                root.externalCenterlineManager.reloadFromSource(root.cave)
            }

            // canReloadFromSource follows the disk and has no change
            // signal, so the header asks at the moment its menu opens.
            onSourceEligibilityRefreshRequested: {
                attachedHeaderId.sourceReloadable =
                        root.externalCenterlineManager.canReloadFromSource(root.cave)
            }
        }

        ExternalCenterlineSolveStatus {
            Layout.fillWidth: true

            hasError: root.linePlotManager.hasSolveError
            warningCount: root.linePlotManager.lastSolveWarningCount
            stationCount: root.linePlotManager.lastSolveStationCount

            onViewCavernOutputRequested: {
                RootData.pageSelectionModel.gotoPageByName(null, "Cavern")
            }
        }

        QC.Button {
            id: detachButtonId
            objectName: "detachButton"

            Layout.alignment: Qt.AlignRight

            text: qsTr("Detach…")
            enabled: !ownerStateId.ownerBusy

            onClicked: root.askToDetach()
        }
    }

    // The page already hosts a RemoveAskBox for removing a trip, so this
    // one answers to a name of its own.
    RemoveAskBox {
        id: detachChallengeId
        objectName: "detachChallenge"

        confirmText: qsTr("Detach")

        // Hangs under the button it belongs to, pulled back inside the
        // card so a sentence-long prompt stays on the screen. A binding,
        // because the box only reaches its full width once its message
        // is laid out.
        x: Math.max(0, Math.min(detachButtonId.x + detachButtonId.width - width,
                                root.width - width))
        y: detachButtonId.y + detachButtonId.height + Theme.tightSpacing

        onRemove: RootData.detachCaveCenterline(root.cave)
    }

    // Built on the first Replace click: the dialog carries a file dialog
    // and a scan preview that most cave pages never open.
    QQ.Loader {
        id: replaceDialogLoaderId

        active: false
        sourceComponent: QQ.Component {
            ReplaceCenterlineDialog {
                owner: root.cave

                // Freed once it is off the screen, for the same reason it is
                // built late. Deferred out of the close handler so the popup
                // is not destroyed from inside its own emission.
                onClosed: Qt.callLater(() => replaceDialogLoaderId.active = false)
            }
        }
    }
}
