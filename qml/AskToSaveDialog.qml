import QtQuick 2.0
import QtQuick.Controls 2.12

Loader {
    id: loaderId

    property SaveAsDialog saveAsDialog

    function askToSave() {
        loaderId.sourceComponent = null;
        loaderId.sourceComponent = dialogComponent;
        loaderId.item.askToSaveDialog.open();
    }

    anchors.centerIn: parent

    Component {
        id: dialogComponent
        Item {
            id: itemId
            property alias askToSaveDialog: askToSaveDialogId

            anchors.centerIn: parent

            Connections {
                target: loaderId.saveAsDialog
                onAccepted: {
                    Qt.quit();
                }
            }

            Dialog {
                id: askToSaveDialogId
                anchors.centerIn: parent
                modal: true
                standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
                title: "Save before quiting?"
                onAccepted: {
                    //Save an close
                    if(!project.temporaryProject) {
                        project.save();
                        Qt.quit();
                    } else {
                        saveAsDialog.open()
                    }
                }

                onDiscarded:  {
                    //Close
                    Qt.quit();
                }
            }
        }
    }
}

