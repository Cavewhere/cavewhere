import QtQuick 2.0 as QQ
import QtQuick.Controls 2.12 as QC

QQ.Loader {
    id: loaderId

    property SaveAsDialog saveAsDialog
    property var afterSaveFunc
    property string taskName

    function askToSave() {
        if(rootData.project.isModified()) {
            loaderId.sourceComponent = null;
            loaderId.sourceComponent = dialogComponent;
            loaderId.item.askToSaveDialog.open();
        } else {
            afterSaveFunc();
            closeDialog();
        }
    }

    function closeDialog() {
        if(loaderId.item) {
            loaderId.item.askToSaveDialog.close();
        }
        loaderId.sourceComponent = null;
    }

    function _privateAfterSave() {
        afterSaveFunc();
        closeDialog();
    }

    anchors.centerIn: parent

    QQ.Component {
        id: dialogComponent
        QQ.Item {
            id: itemId
            property alias askToSaveDialog: askToSaveDialogId

            anchors.centerIn: parent

            QQ.Connections {
                target: rootData.project
                onFileSaved: _privateAfterSave();
            }

            QQ.Connections {
                target: rootData.project.errorModel
                onCountChanged: {
                    if(rootData.project.errorModel.count > 0) {
                        closeDialog();
                    }
                }
            }

            QC.Dialog {
                id: askToSaveDialogId
                anchors.centerIn: parent
                modal: true
                standardButtons: QC.Dialog.Save | QC.Dialog.Discard | QC.Dialog.Cancel
                title: "Save before " + taskName + "?"
                onAccepted: {
                    //Save an close
                    if(project.canSaveDirectly) {
                        project.save();
                    } else {
                        saveAsDialog.open()
                    }
                }

                onDiscarded: _privateAfterSave();
            }
        }
    }
}

