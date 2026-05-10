import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: pageId

    required property AskToSaveDialog askToSaveDialog

    ColumnLayout {
        width: listViewId.implicitWidth
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Theme.pageMargin

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
                    text: "New Project"
                    onTriggered: {
                        function newProject() {
                            RootData.newProject();
                            RootData.pageSelectionModel.gotoPageByName(null, "View");
                        }

                        pageId.askToSaveDialog.taskName = "creating a new project"
                        pageId.askToSaveDialog.afterSaveFunc = newProject
                        pageId.askToSaveDialog.askToSave()
                    }
                }

                QC.MenuItem {
                    objectName: "addMenuOpen"
                    text: "Open"
                    onTriggered: {
                        function openDialog() {
                            RootData.pageSelectionModel.currentPageAddress = "Source";
                            loadProjectDialogId.loadFileDialog.open();
                        }

                        pageId.askToSaveDialog.taskName = "opening a project"
                        pageId.askToSaveDialog.afterSaveFunc = openDialog
                        pageId.askToSaveDialog.askToSave()
                    }
                }

                QC.MenuSeparator {}

                QC.MenuItem {
                    objectName: "addMenuRemote"
                    text: "Online Project"
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

            model: RootData.recentProjectModel

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
                        objectName: "repoLinkText_" + delegateId.index
                        Layout.fillWidth: true
                        text: delegateId.nameRole
                        elide: Text.ElideRight

                        onClicked: {
                            const fileResult = RootData.recentProjectModel.repositoryProjectFile(index)
                            if (fileResult.hasError) {
                                console.warn("Failed to open repository:", fileResult.errorMessage)
                                return;
                            }
                            function loadAndView() {
                                RootData.loadProject(fileResult.value)
                            }
                            pageId.askToSaveDialog.taskName = "opening a recent project"
                            pageId.askToSaveDialog.afterSaveFunc = loadAndView
                            pageId.askToSaveDialog.askToSave()
                        }
                    }

                    QC.Label {
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

        QC.Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            visible: listViewId.count === 0
            text: qsTr("No caving areas created or opened yet.")
        }
    }

    LoadProjectDialog {
        id: loadProjectDialogId
        objectName: "loadProjectDialog"
    }
}
