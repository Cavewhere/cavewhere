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
        width: listViewId.implicitWidth
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 10

        RowLayout {
            id: rowLayoutId
            implicitWidth: listViewId.implicitWidth
            spacing: 12

            QC.Button {
                id: addButton
                objectName: "addRepositoryButton"
                text: "Add"
                icon.source: "qrc:/twbs-icons/icons/plus.svg"
                onClicked: addMenu.popup(addButton)
            }

            Item { Layout.fillWidth: true }

            QC.Menu {
                id: addMenu

                QC.MenuItem {
                    objectName: "addMenuNew"
                    text: "Create New Caving Area"
                    onTriggered: {
                        whereDialogId.repositoryName = ""
                        whereDialogId.description = ""
                        whereDialogId.open()
                    }
                }

                QC.MenuItem {
                    objectName: "addMenuOpen"
                    text: "Open Existing Project"
                    onTriggered: {
                        const openDialog = function() {
                            RootData.pageSelectionModel.currentPageAddress = "Source";
                            loadProjectDialogId.loadFileDialog.open();
                        }

                        askToSaveDialogId.taskName = "opening a project"
                        askToSaveDialogId.afterSaveFunc = openDialog
                        askToSaveDialogId.askToSave()
                    }
                }

                QC.MenuSeparator {}

                QC.MenuItem {
                    objectName: "addMenuRemote"
                    text: "Connect to Remote Project"
                    onTriggered: {
                        RootData.pageSelectionModel.gotoPageByName(null, "Remote");
                    }
                }
            }
        }

        ListView {
            id: listViewId
            objectName: "repositoryListView"

            implicitWidth: Math.min(pageId.width, 500)
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            QC.ScrollBar.vertical: QC.ScrollBar {
                policy: QC.ScrollBar.AsNeeded
            }
            // Layout.leftMargin: 10
            // Layout.rightMargin: 10
            // Layout.topMargin: 10

            model: RootData.repositoryModel

            delegate: TableRowBackground {
                id: delegateId

                required property string nameRole
                required property string pathRole
                required property int index

                width: listViewId.width
                implicitHeight: contentColumn.implicitHeight + 12
                rowIndex: index
                isSelected: false
                // color: index % 2 === 0 ? "#ffffff" : "#eeeeee"

                ColumnLayout {
                    id: contentColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    LinkText {
                        id: linkTextId
                        Layout.fillWidth: true
                        text: delegateId.nameRole
                        elide: Text.ElideRight

                        onClicked: {
                            const result = RootData.repositoryModel.openRepository(index, RootData.project)
                            if (result.hasError) {
                                console.warn("Failed to open repository:", result.errorMessage)
                                return;
                            }
                            RootData.pageSelectionModel.gotoPageByName(null, "View")
                        }
                    }

                    Text {
                        id: pathTextId
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        text: delegateId.pathRole
                        elide: Text.ElideMiddle

                        QC.Menu {
                            id: contextMenu

                            RevealInFileManagerMenuItem {
                                filePath: delegateId.pathRole
                            }
                        }

                        TapHandler {
                            acceptedDevices: PointerDevice.Mouse
                            acceptedButtons: Qt.RightButton
                            gesturePolicy: TapHandler.WithinBounds
                            onTapped: {
                                contextMenu.popup(pathTextId)
                            }
                        }
                    }
                }

            }
        }

        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            visible: listViewId.count === 0
            text: qsTr("No caving areas created or opened yet.")
            color: Qt.darkGray
        }
    }

    SourceLocationDialog {
        id: whereDialogId
        anchors.fill: parent
    }

    SaveAsDialog {
        id: saveAsDialogId
    }

    AskToSaveDialog {
        id: askToSaveDialogId
        saveAsDialog: saveAsDialogId
        taskName: "opening a project"
    }

    LoadProjectDialog {
        id: loadProjectDialogId
        objectName: "loadProjectDialog"
    }
}
