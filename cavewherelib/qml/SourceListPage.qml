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
                objectName: "newCavingAreaButton"
                text: "New Caving Area"
                icon.source: "qrc:/twbs-icons/icons/plus.svg"

                onClicked: {
                    whereDialogId.open()
                }
            }

            Item { Layout.fillWidth: true }

            QC.Button {
                objectName: "openCavingAreaButton"
                text: "Open"
                onClicked: {
                    const openDialog = function() {
                        RootData.pageSelectionModel.currentPageAddress = "Source";
                        loadProjectDialogId.loadFileDialog.open();
                    }

                    askToSaveDialogId.taskName = "opening a project"
                    askToSaveDialogId.afterSaveFunc = openDialog
                    askToSaveDialogId.askToSave()
                }
            }
        }

        ListView {
            id: listViewId
            objectName: "repositoryListView"

            implicitWidth: Math.min(pageId.width, 500)
            Layout.fillHeight: true
            visible: count > 0
            // Layout.leftMargin: 10
            // Layout.rightMargin: 10
            // Layout.topMargin: 10

            model: RootData.repositoryModel

            delegate: Rectangle {
                id: delegateId

                required property string nameRole
                required property string pathRole
                required property int index

                width: listViewId.width
                implicitHeight: contentColumn.implicitHeight + 12
                color: index % 2 === 0 ? "#ffffff" : "#eeeeee"

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
                            RootData.pageSelectionModel.gotoPageByName(RootData.pageSelectionModel.currentPage, "Data")
                        }
                    }

                    Text {
                        id: pathTextId
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        text: delegateId.pathRole
                        elide: Text.ElideMiddle
                        color: Qt.darkGray

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
