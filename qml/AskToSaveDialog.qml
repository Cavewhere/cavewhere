import QtQuick 2.0
import QtQuick.Controls 2.12

Loader {
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

    Component {
        id: dialogComponent
        Item {
            id: itemId
            property alias askToSaveDialog: askToSaveDialogId

            anchors.centerIn: parent

            Connections {
                target: rootData.project
                onFileSaved: _privateAfterSave();
            }

            Connections {
                target: rootData.project.errorModel
                onCountChanged: {
                    if(rootData.project.errorModel.count > 0) {
                        closeDialog();
                    }
                }
            }

            Dialog {
                id: askToSaveDialogId
                anchors.centerIn: parent
                modal: true
                standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
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

