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
            // Layout.leftMargin: 10
            // Layout.rightMargin: 10
            // Layout.topMargin: 10

            model: RootDataSketch.repositoryModel

            delegate: Rectangle {
                id: delegateId

                required property string nameRole
                required property int index

                width: 150
                height: Math.max(30, linkTextId.height)

                color: index % 2 === 0 ? "#ffffff" : "#eeeeee"

                // MouseArea {
                //     anchors.fill: parent
                //     onClicked: {

                //     }
                // }

                LinkText {
                    id: linkTextId
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    text: delegateId.nameRole
                    elide: Text.ElideRight

                    onClicked: {
                        //Load the cwCavingRegion, go to
                        console.log("Clicked area:");
                        RootData.pageSelectionModel.gotoPageByName(null, "Area")
                    }
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
                id: repositoryNameId
                textField.placeholderText: "Caving Area Name"
            }

            RowLayout {
                Text {
                    text: "Location:"
                }

                LinkText {
                    text: folderDialogId.currentFolder
                    onClicked: {
                        folderDialogId.open();
                    }
                }
            }
        }

        onAccepted: {
            RootDataSketch.repositoryModel.addRepository(folderDialogId.currentFolder, repositoryNameId.textField.text)
        }

        standardButtons: QC.Dialog.Cancel | QC.Dialog.Open
    }

    FolderDialog {
        id: folderDialogId
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        selectedFolder: currentFolder
    }
}
