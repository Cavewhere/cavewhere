import QtQuick 2.0 as QQ
//import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import Cavewhere 1.0

QQ.Loader {
    id: loadedId
    anchors.centerIn: parent

    property ErrorListModel model

    function openDialog() {
        loadedId.sourceComponent = null;
        loadedId.sourceComponent = dialogComponent;
        loadedId.item.errorDialog.open();
    }

    QQ.Connections {
        ignoreUnknownSignals: true
        target: model
        onCountChanged: {
            if(model.count === 0) {
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

            anchors.centerIn: parent

            Dialog {
                id: errorDialogId

                anchors.centerIn: parent
                modal: true
                width: 600

                standardButtons: Dialog.Ok
                title: model.count + " issue" + ((model.count > 1) ? "s" : "") + " has occurred"

                onAccepted: {
                    model.clear()
                }

                onRejected:  {
                    model.clear();
                }

                GroupBox {
                    anchors.fill: parent
                    implicitHeight: 150

                    ScrollView {
                        clip: true
                        anchors.fill: parent

                        ErrorListView {
                            id: view
                            anchors.fill: parent
                            model: loadedId.model
                        }
                    }
                }
            }
        }
    }
}
