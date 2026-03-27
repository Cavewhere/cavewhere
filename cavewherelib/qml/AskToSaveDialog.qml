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

        anchors.centerIn: parent

        QQ.Connections {
            target: RootData.project
            function onFileSaved() { loaderId._privateAfterSave(); }
        }

        QQ.Connections {
            target: RootData.project.errorModel
            function onCountChanged() {
                if(RootData.project.errorModel.count > 0) {
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
                    text: itemId.isTemporaryProject
                          ? "This project lives in a <b>temporary folder</b>.<br>Save to move it somewhere permanent"
                          : "Do you want to save your changes before " + loaderId.taskName + "?"
                    wrapMode: QQ.Text.WordWrap
                }
            }

            footer: QC.DialogButtonBox {
                alignment: Qt.AlignRight

                QC.Button {
                    visible: itemId.isTemporaryProject
                    text: "Delete"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.DestructiveRole
                    onClicked: loaderId.handleTemporaryDeleteRequest()
                    contentItem: DangerButtonContent { text: parent.text; font: parent.font }
                }

                QC.Button {
                    visible: !itemId.isTemporaryProject
                    text: "Discard"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.DestructiveRole
                    contentItem: DangerButtonContent { text: parent.text; font: parent.font }
                }

                QC.Button {
                    text: "Cancel"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.RejectRole
                }

                QC.Button {
                    text: "Save"
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
                    font.bold: true
                }
            }

            onAccepted: {
                if (itemId.isTemporaryProject) {
                    loaderId.handleTemporarySaveRequest();
                } else {
                    loaderId.onSaveConfirmed();
                    if (RootData.project.canSaveDirectly) {
                        RootData.project.save();
                    } else {
                        loaderId.saveAsDialog.open();
                    }
                }
            }

            onDiscarded: {
                if (!itemId.isTemporaryProject) {
                    loaderId.onSaveConfirmed();
                    RootData.project.discardChanges();
                    Qt.callLater(loaderId._privateAfterSave);
                }
            }

            onRejected: loaderId.closeDialog()
        }
    }

    QQ.Component {
        id: askToSaveDialogComponent
        AskToSaveInteralDialog {}
    }


}
