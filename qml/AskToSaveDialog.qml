import QtQuick 2.0
import QtQuick.Dialogs 1.2

Loader {
    id: loaderId

    property SaveAsDialog saveAsDialog

    function askToSave() {
        loaderId.sourceComponent = null;
        loaderId.sourceComponent = dialogComponent;
        loaderId.item.askToSaveDialog.open();
    }

    Component {
        id: dialogComponent
        Item {
            id: itemId
            property alias askToSaveDialog: askToSaveDialogId

            Connections {
                target: loaderId.saveAsDialog
                onAccepted: {
                    Qt.quit();
                }
            }

            Dialog {
                id: askToSaveDialogId
                modality: Qt.ApplicationModal
                standardButtons: StandardButton.Save | StandardButton.Discard | StandardButton.Cancel
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

                onDiscard: {
                    //Close
                    Qt.quit();
                }

                Text {
                    text: askToSaveDialogId.title
                }
            }
        }
    }
}

