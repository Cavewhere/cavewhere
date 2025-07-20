import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import cavewherelib
import QQuickGit

StandardPage {
    id: pageId

    ColumnLayout {
        anchors.fill: parent

        QC.Button {
            objectName: "newCavingAreaButton"
            text: "New Caving Area"
            icon.source: "qrc:/twbs-icons/icons/plus.svg"

            onClicked: {
                whereDialogId.active = true
                whereDialogId.item.open()
            }
        }

        QC.Button {

        }

        ListView {
            id: listViewId
            objectName: "repositoryListView"

            Layout.fillHeight: true
            // Layout.leftMargin: 10
            // Layout.rightMargin: 10
            // Layout.topMargin: 10

            model: RootData.repositoryModel

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
    }

    Loader {
        id: whereDialogId
        objectName: "whereDialogLoader"

        active: false
        anchors.fill: parent

        function close() {
            item.close();
            active = false;
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

                TextFieldWithError {
                    id: repositoryNameId
                    objectName: "cavingAreaName"
                    Layout.fillWidth: true
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
                                    //Succes
                                    whereDialogId.close();
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
            currentFolder: RootData.repositoryModel.defaultRepositoryDir
            selectedFolder: currentFolder
        }
    }
}
