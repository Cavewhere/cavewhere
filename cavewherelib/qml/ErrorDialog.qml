import QtQuick as QQ
//import QtQuick.Dialogs
//import QtQuick.Layouts
import QtQuick.Controls as QC
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

        QC.Dialog {
            id: errorDialogId
            objectName: "errorDialog"

            property int issueCount: itemId.model ? itemId.model.count : 0

            anchors.centerIn: parent
            modal: true
            width: 600

            title: issueCount + " issue" + ((issueCount > 1) ? "s" : "") + " has occurred"

            onAccepted: {
                itemId.model.clear()
            }

            onRejected:  {
                itemId.model.clear();
            }

            footer: QC.DialogButtonBox {
                QC.Button {
                    objectName: "errorDialogCopyAllButton"
                    text: qsTr("Copy All")
                    QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.ActionRole
                    enabled: errorDialogId.issueCount > 0
                    onClicked: {
                        if (itemId.model) {
                            RootData.copyText(itemId.model.allMessagesAsText())
                        }
                    }
                }
                standardButtons: QC.DialogButtonBox.Ok
            }

            GroupBox {
                anchors.fill: parent
                implicitHeight: 150

                QC.ScrollView {
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
