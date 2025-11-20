import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import cavewherelib
import QQuickGit

Loader {
    id: whereDialogId
    objectName: "whereDialogLoader"

    property string repositoryName;
    property string description

    signal accepted(var dir)

    active: false

    function close() {
        item.close();
        active = false;
    }

    function open() {
        active = true;
        item.open();
    }

    function _successClose(dir) {
        close()
        accepted(dir)
    }

    sourceComponent: QC.Popup {
        id: popupId

        property cwResultDir resultDir: {
            let folderUrl = folderDialogId.selectedFolder.toString() !== "" ? folderDialogId.selectedFolder : folderDialogId.currentFolder
            let resultDir = RootData.repositoryModel.repositoryDir(folderUrl, repositoryNameId.textField.text);
            return resultDir;
        }

        anchors.centerIn: parent
        modal: true

        contentItem: ColumnLayout {
            id: whereColumnId
            // width: Math.min(pageId.width - 50, 400)

            Text {
                id: textId
                visible: whereDialogId.description.length > 0
                text: whereDialogId.description
            }

            TextFieldWithError {
                id: repositoryNameId
                objectName: "cavingAreaName"
                Layout.fillWidth: true
                textField.text: whereDialogId.repositoryName
                textField.placeholderText: "Caving Area Name"
                textField.KeyNavigation.tab: openId.button

                errorMessage: popupId.resultDir.errorMessage
                ignoreErrorUntilNextFocus: true
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Path:"
                }

                LinkText {
                    // Layout.maximumWidth: 250
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    objectName: "repositoryParentDir"
                    // text: RootData.urlToLocal(folderDialogId.selectedFolder)
                    text: folderDialogId.selectedFolder.toString() !== "" ? RootData.urlToLocal(folderDialogId.selectedFolder) : RootData.urlToLocal(folderDialogId.currentFolder)
                    onClicked: {
                        folderDialogId.open();
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                QC.Button {
                    objectName: "cancelRepoButton"
                    Layout.alignment: Qt.AlignLeft
                    text: "Cancel"
                    onClicked: {
                        whereDialogId.close();
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                ButtonShake {
                    id: openId
                    objectName: "openRepoButton"
                    Layout.alignment: Qt.AlignRight
                    button.text: "Open"
                    button.onClicked: {
                        let folderUrl = folderDialogId.selectedFolder.toString() !== "" ? folderDialogId.selectedFolder : folderDialogId.currentFolder
                        let resultDir = RootData.repositoryModel.repositoryDir(folderUrl, repositoryNameId.textField.text);
                        if(popupId.resultDir.hasError) {
                            repositoryNameId.ignoreErrorUntilNextFocus = false;
                            shake();
                            return;
                        } else {
                            //Good
                            let gitResult = RootData.repositoryModel.addRepository(resultDir);

                            if(gitResult.hasError) {
                                errorLabelId.text = gitResult.errorMessage
                                shake();
                                return;
                            } else {
                                //Success
                                whereDialogId._successClose(resultDir.value);
                            }
                        }
                    }
                }
            }

            ErrorHelpArea {
                id: errorLabelId
                Layout.fillWidth: true

                objectName: "newRepoErrorArea"
                text: ""
                visible: text.length > 0
            }

        }

        // onAccepted: {
        //     RootData.repositoryModel.addRepository(folderDialogId.currentFolder, repositoryNameId.textField.text)
        // }

        // standardButtons: QC.Dialog.Cancel | QC.Dialog.Open
    }


    FolderDialog {
        id: folderDialogId
        objectName: "folderDialog"
        currentFolder: RootData.repositoryModel.defaultRepositoryDir
        selectedFolder: currentFolder
    }
}
