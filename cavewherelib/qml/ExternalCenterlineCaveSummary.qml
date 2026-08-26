/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

// Cave-level state for an externally-backed cave, shown on CavePage
// (plans/EXTERNAL_FILE_PHASE3.html P3.8): the attached header (entry
// file, format, Replace…, and where the copy came from) and the solve
// status of the last cavern run. Dropping the attachment is removing
// the cave itself, which the data page already offers.
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
