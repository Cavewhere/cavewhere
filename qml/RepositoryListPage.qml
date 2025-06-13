import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import cavewherelib
import CaveWhereSketch
import QQuickGit

StandardPage {
    id: pageId


    ColumnLayout {
        anchors.fill: parent

        ListView {
            id: listViewId

            Layout.fillHeight: true

            model: RootDataSketch.repositoryModel

            delegate: Item {
                required property string nameRole

                LinkText {
                    text: nameRole
                    elide: Text.ElideRight
                }
            }
        }

        QC.Button {
            text: "Add Caving Area"
            icon.source: "qrc:/twbs-icons/icons/plus.svg"

            onClicked: {
                whereDialogId.open()
            }
        }
    }

    QC.Dialog {
        id: whereDialogId

        anchors.centerIn: parent

        ColumnLayout {
            implicitWidth: 200

            TextFieldWithError {
                textField.placeholderText: "Caving Area Name"
            }

            LinkText {
                text: folderDialogId.currentFolder
                onClicked: {
                    folderDialogId.open();
                }
            }
        }

        standardButtons: QC.Dialog.Cancel | QC.Dialog.Open
    }

    FolderDialog {
        id: folderDialogId
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        selectedFolder: currentFolder
    }
}
