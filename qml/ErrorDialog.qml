import QtQuick as QQ
//import QtQuick.Dialogs
//import QtQuick.Layouts
import QtQuick.Controls as Controls
import Cavewhere 1.0

QQ.Loader {
    id: loadedId
    anchors.centerIn: parent

    property ErrorListModel model

    function openDialog() {
        loadedId.sourceComponent = null;
        loadedId.sourceComponent = dialogComponent;
        loadedId.item.errorDialog.open();
        loadedId.item.model = model;
    }

    QQ.Connections {
        ignoreUnknownSignals: true
        target: loadedId.model
        onCountChanged: {
            if(loadedId.model.count === 0) {
                loadedId.sourceComponent = null;
            } else {
                openDialog();
            }
        }
    }

    QQ.Component {
        id: dialogComponent

        QQ.Item {
            id: itemId
            property alias errorDialog: errorDialogId
            property ErrorListModel model

            anchors.centerIn: parent

            Controls.Dialog {
                id: errorDialogId

                anchors.centerIn: parent
                modal: true
                width: 600

                standardButtons: Dialog.Ok
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
    }
}
