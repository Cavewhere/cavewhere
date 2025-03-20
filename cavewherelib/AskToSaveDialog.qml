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

    anchors.centerIn: parent

    component AskToSaveInteralDialog :
    // QQ.Component {
        // id: dialogComponent
        QQ.Item {
            id: itemId
            property alias askToSaveDialog: askToSaveDialogId

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
                standardButtons: QC.Dialog.Save | QC.Dialog.Discard | QC.Dialog.Cancel
                title: "Save before " + loaderId.taskName + "?"
                onAccepted: {
                    //Save an close
                    if(RootData.project.canSaveDirectly) {
                        RootData.project.save();
                    } else {
                        loaderId.saveAsDialog.open()
                    }
                }

                onDiscarded: loaderId._privateAfterSave();
            }
        }
    // }

    QQ.Component {
        id: askToSaveDialogComponent
        AskToSaveInteralDialog {}
    }
}

