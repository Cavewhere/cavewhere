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

    property url selectedDestinationFolder: ""

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
            let folderUrl = whereDialogId.selectedDestinationFolder.toString() !== "" ? whereDialogId.selectedDestinationFolder : RootData.recentProjectModel.defaultRepositoryDir
            let resultDir = RootData.project.repositoryDir(folderUrl, repositoryNameId.textField.text);
            return resultDir;
        }

        anchors.centerIn: parent
        modal: true

        contentItem: ColumnLayout {
            id: whereColumnId
            // width: Math.min(pageId.width - 50, 400)

            QC.Label {
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
                QC.Label {
                    text: "Path:"
                }

                LinkText {
                    // Layout.maximumWidth: 250
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    objectName: "repositoryParentDir"
                    text: whereDialogId.selectedDestinationFolder.toString() !== "" ? RootData.urlToLocal(whereDialogId.selectedDestinationFolder) : RootData.urlToLocal(RootData.recentProjectModel.defaultRepositoryDir)
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
                        let folderUrl = whereDialogId.selectedDestinationFolder.toString() !== "" ? whereDialogId.selectedDestinationFolder : RootData.recentProjectModel.defaultRepositoryDir
                        let resultDir = RootData.project.repositoryDir(folderUrl, repositoryNameId.textField.text);
                        if(popupId.resultDir.hasError) {
                            repositoryNameId.ignoreErrorUntilNextFocus = false;
                            shake();
                            return;
                        } else {
                            //Good
                            let gitResult = RootData.recentProjectModel.addRepository(resultDir);

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
        //     RootData.recentProjectModel.addRepository(folderDialogId.currentFolder, repositoryNameId.textField.text)
        // }

        // standardButtons: QC.Dialog.Cancel | QC.Dialog.Open
    }


    FolderDialog {
        id: folderDialogId
        objectName: "folderDialog"
        currentFolder: RootData.recentProjectModel.defaultRepositoryDir
        selectedFolder: currentFolder

        onAccepted: {
            whereDialogId.selectedDestinationFolder = folderDialogId.selectedFolder
        }
    }
}
