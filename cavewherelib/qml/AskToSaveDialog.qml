pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// Unqualified access: Set "pragma ComponentBehavior: Bound" in order to use IDs from outer components in nested components. [unqualified]

QQ.Loader {
    id: loaderId

    required property SaveAsDialog saveAsDialog
    property var afterSaveFunc: function() {}
    required property string taskName

    property AskToSaveInteralDialog _dialog: null

    function askToSave() {
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

    component AskToSaveInteralDialog :
    // QQ.Component {
        // id: dialogComponent
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
                standardButtons: itemId.isTemporaryProject ? QC.DialogButtonBox.NoButton : (QC.DialogButtonBox.Save | QC.DialogButtonBox.Discard | QC.DialogButtonBox.Cancel)
                title: (itemId.isTemporaryProject ? "Save temporary project before " : "Save before ") + loaderId.taskName + "?"

                contentItem: QQ.Column {
                    width: Math.max(implicitWidth, 360)
                    spacing: 8

                    QC.Label {
                        text: itemId.isTemporaryProject
                              ? "This project lives in a <b>temporary folder</b>.<br>Save to move it somewhere permanent"
                              : "Do you want to save your changes before " + loaderId.taskName + "?"
                        wrapMode: QQ.Text.WordWrap
                    }
                }

                footer: QQ.Loader {
                    active: itemId.isTemporaryProject
                    sourceComponent: temporaryFooterComponent
                }

                onAccepted: {
                    if(RootData.project.canSaveDirectly) {
                        RootData.project.save();
                    } else {
                        loaderId.saveAsDialog.open()
                    }
                }

                onDiscarded: {
                    if(!itemId.isTemporaryProject) {
                        loaderId._privateAfterSave();
                    }
                }
            }
        }
    // }

    QQ.Component {
        id: askToSaveDialogComponent
        AskToSaveInteralDialog {}
    }

    QQ.Component {
        id: temporaryFooterComponent
        QC.DialogButtonBox {
            alignment: Qt.AlignRight

            QC.Button {
                text: "Delete"
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.DestructiveRole
                onClicked: loaderId.handleTemporaryDeleteRequest()
                // contentItem.color "#D32F2F"

                contentItem: Text {
                    text: parent.text
                    color: "#D32F2F"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            QC.Button {
                text: "Cancel"
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.RejectRole
                onClicked: loaderId.closeDialog()
            }

            QC.Button {
                text: "Save"
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
                onClicked: loaderId.handleTemporarySaveRequest()
                font.bold: true
            }
        }
    }
}
