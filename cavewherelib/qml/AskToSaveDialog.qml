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
    property bool _saveCompleted: false

    // True when the project has a remote and there is something to push (or status is stale/unknown).
    // Controls visibility of the "Save & Sync" button.
    readonly property bool offerSync:
        RootData.project.syncHealth.status.hasRemote
        && (RootData.project.syncHealth.status.aheadCount > 0
            || RootData.project.modified
            || RootData.project.syncHealth.status.stale)

    function askToSave() {
        loaderId._saveCompleted = false;
        if (RootData.project.isNewEmptyProject()
                || RootData.project.saveWillCauseDataLoss) {
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
        loaderId.sourceComponent = null;
    }

    function _privateAfterSave() {
        if (loaderId._saveCompleted) { return; }
        loaderId._saveCompleted = true;
        closeDialog();
        afterSaveFunc();
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

    component DangerButtonContent: QC.Label {
        color: Theme.danger
        horizontalAlignment: QC.Label.AlignHCenter
        verticalAlignment: QC.Label.AlignVCenter
        elide: QC.Label.ElideRight
    }

    component AskToSaveInteralDialog :
    QQ.Item {
        id: itemId
        property alias askToSaveDialog: askToSaveDialogId
        property bool isTemporaryProject: RootData.project.isTemporaryProject

        property bool syncAfterSave: false
        property string syncErrorText: ""

        // Logical phase driven by JS; QML state is derived from phase + context via when: conditions.
        property string _phase: "idle"

        anchors.centerIn: parent

        states: [
            // idle sub-states: each loads the exact buttons needed — no invisible delegates in the ListView.
            QQ.State {
                name: "idle-temp"
                when: itemId._phase === "idle" && itemId.isTemporaryProject
                QQ.PropertyChanges { footerLoaderId.sourceComponent: tempIdleButtonsComponent }
            },
            QQ.State {
                name: "idle-nosync"
                when: itemId._phase === "idle" && !itemId.isTemporaryProject && !loaderId.offerSync
                QQ.PropertyChanges { footerLoaderId.sourceComponent: nonTempNoSyncButtonsComponent }
            },
            QQ.State {
                name: "idle-sync"
                when: itemId._phase === "idle" && !itemId.isTemporaryProject && loaderId.offerSync
                QQ.PropertyChanges { footerLoaderId.sourceComponent: nonTempSyncButtonsComponent }
            },
            QQ.State {
                name: "saving"
                when: itemId._phase === "saving"
                QQ.PropertyChanges {
                    contentLabelId.visible: false
                    busyIndicatorId.visible: true
                    busyLabelId.visible: true
                    busyLabelId.text: "Saving\u2026"
                    footerLoaderId.sourceComponent: null
                }
            },
            QQ.State {
                name: "syncing"
                when: itemId._phase === "syncing"
                extend: "saving"
                QQ.PropertyChanges { busyLabelId.text: "Syncing\u2026" }
            },
            QQ.State {
                name: "syncError"
                when: itemId._phase === "syncError"
                QQ.PropertyChanges {
                    contentLabelId.visible: false
                    syncErrorLabelId.visible: true
                    footerLoaderId.sourceComponent: syncErrorButtonsComponent
                }
            }
        ]

        QQ.Component.onCompleted: {
            // Kick off a remote status check so offerSync updates reactively.
            RootData.project.syncHealth.refresh();
            // If a background sync is already running, wait for it instead of starting a new one.
            if (RootData.project.syncInProgress) {
                itemId._phase = "syncing";
            }
        }

        QQ.Connections {
            target: RootData.project

            function onFileSaved() {
                if (itemId.syncAfterSave) {
                    itemId._phase = "syncing";
                    RootData.project.sync();
                } else {
                    Qt.callLater(loaderId._privateAfterSave);
                }
            }

            function onSyncAuthFailed() {
                itemId.syncErrorText = "GitHub access has expired. Your changes are saved locally.";
            }

            function onSyncFinished() {
                if (itemId._phase !== "syncing") { return; }

                if (itemId.syncErrorText !== "") {
                    itemId._phase = "syncError";
                } else if (RootData.project.errorModel.count > 0) {
                    itemId.syncErrorText = "Sync failed: "
                        + RootData.project.errorModel.last().message
                        + "\nYour changes are saved locally.";
                    itemId._phase = "syncError";
                } else if (RootData.project.isModified()) {
                    // An in-flight sync finished but the project still has unsaved changes;
                    // save them and sync again so they are also pushed.
                    itemId.syncAfterSave = true;
                    itemId._phase = "saving";
                    if (RootData.project.canSaveDirectly) {
                        RootData.project.save();
                    } else {
                        loaderId.saveAsDialog.open();
                    }
                } else {
                    Qt.callLater(loaderId._privateAfterSave);
                }
            }
        }

        QQ.Connections {
            target: RootData.project.errorModel
            function onCountChanged() {
                // Only react to errorModel changes during save — sync errors are handled
                // via onSyncFinished so they can show the syncError state instead of just closing.
                if ((itemId._phase === "idle" || itemId._phase === "saving")
                        && RootData.project.errorModel.count > 0) {
                    loaderId.closeDialog();
                }
            }
        }

        QQ.Connections {
            target: loaderId.saveAsDialog
            // Cancelling the SaveAsDialog while in idle (temp-project Save flow) should
            // dismiss the AskToSaveDialog too — there is no meaningful state to return to.
            function onRejected() {
                if (itemId._phase === "idle") {
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
                    id: contentLabelId
                    width: parent.width
                    text: itemId.isTemporaryProject
                          ? "This project lives in a <b>temporary folder</b>.<br>Save to move it somewhere permanent"
                          : "Do you want to save your changes before " + loaderId.taskName + "?"
                    wrapMode: QC.Label.WordWrap
                }

                QC.BusyIndicator {
                    id: busyIndicatorId
                    visible: false
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                QC.Label {
                    id: busyLabelId
                    visible: false
                    width: parent.width
                    text: "Saving\u2026"
                    horizontalAlignment: QC.Label.AlignHCenter
                }

                QC.Label {
                    id: syncErrorLabelId
                    visible: false
                    width: parent.width
                    text: itemId.syncErrorText
                    wrapMode: QC.Label.WordWrap
                }
            }

            footer: QQ.Loader { id: footerLoaderId }

            onAccepted: {
                if (itemId._phase !== "idle") { return; }
                if (itemId.isTemporaryProject) {
                    loaderId.handleTemporarySaveRequest();
                } else {
                    itemId._phase = "saving";
                    loaderId.onSaveConfirmed();
                    if (RootData.project.canSaveDirectly) {
                        RootData.project.save();
                    } else {
                        loaderId.saveAsDialog.open();
                    }
                }
            }

            onApplied: {
                if (itemId._phase !== "idle") { return; }
                itemId.syncAfterSave = true;
                itemId.syncErrorText = "";
                itemId._phase = "saving";
                loaderId.onSaveConfirmed();
                if (RootData.project.canSaveDirectly) {
                    RootData.project.save();
                } else {
                    loaderId.saveAsDialog.open();
                }
            }

            onDiscarded: {
                if (itemId._phase !== "idle" || itemId.isTemporaryProject) { return; }
                loaderId.onSaveConfirmed();
                RootData.project.discardChanges();
                Qt.callLater(loaderId._privateAfterSave);
            }

            onRejected: {
                if (itemId._phase === "idle") {
                    loaderId.closeDialog();
                }
            }
        }

        // idle + temporary project: Delete, Cancel, Save
        // All buttons use NoRole + onClicked because QC.Dialog only wires role-based signals
        // to a DialogButtonBox found directly as the footer, not one inside a Loader.
        QQ.Component {
            id: tempIdleButtonsComponent
            QC.DialogButtonBox {
                alignment: Qt.AlignRight
                QC.Button {
                    text: "Delete"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: loaderId.handleTemporaryDeleteRequest()
                    contentItem: DangerButtonContent { text: parent.text; font: parent.font }
                }
                QC.Button {
                    text: "Cancel"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: loaderId.closeDialog()
                }
                QC.Button {
                    text: "Save"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    font.bold: true
                    onClicked: itemId.askToSaveDialog.accepted()
                }
            }
        }

        // idle + non-temporary, no remote sync available: Discard, Cancel, Save
        QQ.Component {
            id: nonTempNoSyncButtonsComponent
            QC.DialogButtonBox {
                alignment: Qt.AlignRight
                QC.Button {
                    text: "Discard"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: itemId.askToSaveDialog.discarded()
                    contentItem: DangerButtonContent { text: parent.text; font: parent.font }
                }
                QC.Button {
                    text: "Cancel"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: loaderId.closeDialog()
                }
                QC.Button {
                    text: "Save"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    font.bold: true
                    onClicked: itemId.askToSaveDialog.accepted()
                }
            }
        }

        // idle + non-temporary, remote sync available: Discard, Cancel, Save && Sync, Save
        QQ.Component {
            id: nonTempSyncButtonsComponent
            QC.DialogButtonBox {
                alignment: Qt.AlignRight
                QC.Button {
                    text: "Discard"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: itemId.askToSaveDialog.discarded()
                    contentItem: DangerButtonContent { text: parent.text; font: parent.font }
                }
                QC.Button {
                    text: "Cancel"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: loaderId.closeDialog()
                }
                QC.Button {
                    text: "Save && Sync"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    font.bold: true
                    onClicked: itemId.askToSaveDialog.applied()
                }
                QC.Button {
                    text: "Save"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: itemId.askToSaveDialog.accepted()
                }
            }
        }

        // syncError state: Close anyway, Stay open
        QQ.Component {
            id: syncErrorButtonsComponent
            QC.DialogButtonBox {
                alignment: Qt.AlignRight
                QC.Button {
                    text: "Close anyway"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: loaderId._privateAfterSave()
                }
                QC.Button {
                    text: "Stay open"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.NoRole
                    onClicked: loaderId.closeDialog()
                }
            }
        }
    }

    QQ.Component {
        id: askToSaveDialogComponent
        AskToSaveInteralDialog {}
    }


}
