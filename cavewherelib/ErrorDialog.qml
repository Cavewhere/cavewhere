import QtQuick as QQ
//import QtQuick.Dialogs
//import QtQuick.Layouts
import QtQuick.Controls as Controls
import cavewherelib

QQ.Loader {
    id: loadedId
    anchors.centerIn: parent

    property ErrorListModel model

    function openDialog() {
        loadedId.sourceComponent = null;
        loadedId.sourceComponent = dialogComponent;
        let loadedItem = loadedId.item as ErrorDialogContent;

        loadedItem.errorDialog.open();
        loadedItem.model = model;
    }

    QQ.Connections {
        ignoreUnknownSignals: true
        target: loadedId.model
        function onCountChanged() {
            if(loadedId.model.count === 0) {
                loadedId.sourceComponent = null;
            } else {
                loadedId.openDialog();
            }
        }
    }

    component ErrorDialogContent: QQ.Item {
        id: itemId
        property alias errorDialog: errorDialogId
        property ErrorListModel model

        anchors.centerIn: parent

        Controls.Dialog {
            id: errorDialogId

            anchors.centerIn: parent
            modal: true
            width: 600

            standardButtons: Controls.Dialog.Ok
            title: itemId.model.count + " issue" + ((itemId.model.count > 1) ? "s" : "") + " has occurred"

            onAccepted: {
                itemId.model.clear()
            }

            onRejected:  {
                itemId.model.clear();
            }

            GroupBox {
                anchors.fill: parent
                implicitHeight: 150

                Controls.ScrollView {
                    clip: true
                    anchors.fill: parent

                    ErrorListView {
                        id: view
                        anchors.fill: parent
                        model: itemId.model
                    }
                }
            }
        }
    }

    QQ.Component {
        id: dialogComponent
        ErrorDialogContent {}
    }
}
