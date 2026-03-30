pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// Unqualified access: Set "pragma ComponentBehavior: Bound" in order to use IDs from outer components in nested components. [unqualified]

QQ.Loader {
    id: loaderId

    required property SaveAsDialog saveAsDialog
    property var afterSaveFunc: function() {}
    property var onSaveConfirmed: function() {}
    required property string taskName

    property AskToSaveInteralDialog _dialog: null

    // True when the project has a remote and there is something to push (or status is stale/unknown).
    // Controls visibility of the "Save & Sync" button.
    readonly property bool offerSync:
        !RootData.project.syncHealth.status.noRemote
        && (RootData.project.syncHealth.status.aheadCount > 0
            || RootData.project.modified
            || RootData.project.syncHealth.status.stale)

    function askToSave() {
        if (RootData.project.isNewEmptyProject()) {
            afterSaveFunc();
            closeDialog();
            return;
        }

        if(RootData.project.isModified()) {
            loaderId.sourceComponent = null;
            loaderId._dialog = null;
            loaderId.sourceComponent = askToSaveDialogComponent;
            loaderId._dialog = loaderId.item; //Typecast to AskToSaveInteralDialog
            loaderId._dialog.askToSaveDialog.open();
        } else {
            afterSaveFunc();
            closeDialog();
        }
    }

    function closeDialog() {
        if(loaderId.item) {
            loaderId._dialog.askToSaveDialog.close();
        }
        loaderId.sourceComponent = null;
    }

    function _privateAfterSave() {
        afterSaveFunc();
        closeDialog();
    }

    function handleTemporarySaveRequest() {
        if (loaderId.saveAsDialog) {
            loaderId.saveAsDialog.open();
        }
    }

    function handleTemporaryDeleteRequest() {
        if (RootData.project.deleteTemporaryProject()) {
            loaderId._privateAfterSave();
        }
    }

    anchors.centerIn: parent

    component DangerButtonContent: Text {
        color: Theme.danger
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component AskToSaveInteralDialog :
    QQ.Item {
        id: itemId
        property alias askToSaveDialog: askToSaveDialogId
        property bool isTemporaryProject: RootData.project.isTemporaryProject

        // idle    — waiting for user input
        // saving  — save in progress (spinner shown, buttons hidden)
        // syncing — sync in progress after save (spinner shown, buttons hidden)
        // syncError — sync finished with an error; offer "Close anyway" or "Stay open"
        property string dialogState: "idle"
        property bool syncAfterSave: false
        property bool authFailed: false
        property string syncErrorText: ""

        anchors.centerIn: parent

        QQ.Component.onCompleted: {
            // Kick off a remote status check so offerSync updates reactively.
            RootData.project.syncHealth.refresh();
            // If a background sync is already running, wait for it instead of starting a new one.
            if (RootData.project.syncInProgress) {
                itemId.dialogState = "syncing";
            }
        }

        QQ.Connections {
            target: RootData.project

            function onFileSaved() {
                if (itemId.syncAfterSave) {
                    itemId.dialogState = "syncing";
                    RootData.project.sync();
                } else {
                    loaderId._privateAfterSave();
                }
            }

            function onSyncAuthFailed() {
                itemId.authFailed = true;
            }

            function onSyncFinished() {
                if (itemId.dialogState !== "syncing") { return; }

                if (itemId.authFailed) {
                    itemId.syncErrorText = "GitHub access has expired. Your changes are saved locally.";
                    itemId.dialogState = "syncError";
                } else if (RootData.project.errorModel.count > 0) {
                    itemId.syncErrorText = "Sync failed: "
                        + RootData.project.errorModel.last().message
                        + "\nYour changes are saved locally.";
                    itemId.dialogState = "syncError";
                } else if (RootData.project.isModified()) {
                    // An in-flight sync finished but the project still has unsaved changes;
                    // save them and sync again so they are also pushed.
                    itemId.syncAfterSave = true;
                    itemId.dialogState = "saving";
                    if (RootData.project.canSaveDirectly) {
                        RootData.project.save();
                    } else {
                        loaderId.saveAsDialog.open();
                    }
                } else {
                    loaderId._privateAfterSave();
                }
            }
        }

        QQ.Connections {
            target: RootData.project.errorModel
            function onCountChanged() {
                // Only react to errorModel changes during save — sync errors are handled
                // via onSyncFinished so they can show the syncError state instead of just closing.
                if ((itemId.dialogState === "idle" || itemId.dialogState === "saving")
                        && RootData.project.errorModel.count > 0) {
                    loaderId.closeDialog();
                }
            }
        }

        QC.Dialog {
            id: askToSaveDialogId
            anchors.centerIn: parent
            modal: true
            implicitWidth: 400
            title: (itemId.isTemporaryProject ? "Save temporary project before " : "Save before ") + loaderId.taskName + "?"

            contentItem: QQ.Column {
                spacing: 8

                QC.Label {
                    visible: itemId.dialogState === "idle"
                    width: parent.width
                    text: itemId.isTemporaryProject
                          ? "This project lives in a <b>temporary folder</b>.<br>Save to move it somewhere permanent"
                          : "Do you want to save your changes before " + loaderId.taskName + "?"
                    wrapMode: QQ.Text.WordWrap
                }

                QC.BusyIndicator {
                    visible: itemId.dialogState === "saving" || itemId.dialogState === "syncing"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                QC.Label {
                    visible: itemId.dialogState === "saving" || itemId.dialogState === "syncing"
                    width: parent.width
                    text: itemId.dialogState === "saving" ? "Saving\u2026" : "Syncing\u2026"
                    horizontalAlignment: QQ.Text.AlignHCenter
                }

                QC.Label {
                    visible: itemId.dialogState === "syncError"
                    width: parent.width
                    text: itemId.syncErrorText
                    wrapMode: QQ.Text.WordWrap
                }
            }

            footer: QC.DialogButtonBox {
                alignment: Qt.AlignRight

                // --- idle-state buttons ---

                QC.Button {
                    visible: itemId.dialogState === "idle" && itemId.isTemporaryProject
                    text: "Delete"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.DestructiveRole
                    onClicked: loaderId.handleTemporaryDeleteRequest()
                    contentItem: DangerButtonContent { text: parent.text; font: parent.font }
                }

                QC.Button {
                    visible: itemId.dialogState === "idle" && !itemId.isTemporaryProject
                    text: "Discard"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.DestructiveRole
                    contentItem: DangerButtonContent { text: parent.text; font: parent.font }
                }

                QC.Button {
                    visible: itemId.dialogState === "idle"
                    text: "Cancel"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.RejectRole
                }

                QC.Button {
                    visible: itemId.dialogState === "idle"
                             && loaderId.offerSync
                             && !itemId.isTemporaryProject
                    text: "Save && Sync"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.ApplyRole
                    font.bold: true
                }

                QC.Button {
                    visible: itemId.dialogState === "idle"
                    text: "Save"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
                    // Bold only when "Save & Sync" is not available (Save is the primary action)
                    font.bold: !loaderId.offerSync || itemId.isTemporaryProject
                }

                // --- syncError-state buttons ---

                QC.Button {
                    visible: itemId.dialogState === "syncError"
                    text: "Close anyway"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
                }

                QC.Button {
                    visible: itemId.dialogState === "syncError"
                    text: "Stay open"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.RejectRole
                }
            }

            // "Save" (idle) or "Close anyway" (syncError)
            onAccepted: {
                if (itemId.dialogState === "syncError") {
                    loaderId._privateAfterSave();
                    return;
                }
                if (itemId.dialogState !== "idle") { return; }
                if (itemId.isTemporaryProject) {
                    loaderId.handleTemporarySaveRequest();
                } else {
                    itemId.dialogState = "saving";
                    loaderId.onSaveConfirmed();
                    if (RootData.project.canSaveDirectly) {
                        RootData.project.save();
                    } else {
                        loaderId.saveAsDialog.open();
                    }
                }
            }

            // "Save & Sync"
            onApplied: {
                if (itemId.dialogState !== "idle") { return; }
                itemId.syncAfterSave = true;
                itemId.authFailed = false;
                itemId.dialogState = "saving";
                loaderId.onSaveConfirmed();
                if (RootData.project.canSaveDirectly) {
                    RootData.project.save();
                } else {
                    loaderId.saveAsDialog.open();
                }
            }

            // "Discard" (idle, non-temporary)
            onDiscarded: {
                if (itemId.dialogState !== "idle" || itemId.isTemporaryProject) { return; }
                loaderId.onSaveConfirmed();
                RootData.project.discardChanges();
                Qt.callLater(loaderId._privateAfterSave);
            }

            // "Cancel" (idle) or "Stay open" (syncError)
            onRejected: {
                if (itemId.dialogState === "syncError") {
                    loaderId.closeDialog();
                    return;
                }
                if (itemId.dialogState === "idle") {
                    loaderId.closeDialog();
                }
            }
        }
    }

    QQ.Component {
        id: askToSaveDialogComponent
        AskToSaveInteralDialog {}
    }


}
