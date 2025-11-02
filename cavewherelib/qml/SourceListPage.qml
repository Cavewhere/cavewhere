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
        anchors.margins: 10

        QC.Button {
            objectName: "newCavingAreaButton"
            text: "New Caving Area"
            icon.source: "qrc:/twbs-icons/icons/plus.svg"

            onClicked: {
                whereDialogId.open()
            }
        }

        QC.Button {
            objectName: "openCavingAreaButton"
            text: "Open"
        }

        ListView {
            id: listViewId
            objectName: "repositoryListView"

            width: Math.max(parent.width, 300)
            Layout.fillHeight: true
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
                height: Math.max(30, linkTextId.height)

                color: index % 2 === 0 ? "#ffffff" : "#eeeeee"

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 12

                    LinkText {
                        id: linkTextId
                        text: delegateId.nameRole
                        elide: Text.ElideRight

                        onClicked: {
                            const result = RootData.repositoryModel.openRepository(index, RootData.project)
                            if (result.hasError) {
                                console.warn("Failed to open repository:", result.errorMessage)
                                return;
                            }
                            RootData.pageSelectionModel.gotoPageByName(null, "Area")
                        }
                    }

                    Text {
                        id: pathTextId
                        text: delegateId.pathRole
                        elide: Text.ElideRight
                        Layout.fillWidth: true

                        QC.Menu {
                            id: contextMenu

                            RevealInFileManagerMenuItem {
                                filePath: delegateId.pathRole
                            }
                        }

                    }
                }


                TapHandler {
                    acceptedDevices: PointerDevice.Mouse
                    acceptedButtons: Qt.RightButton
                    onTapped: (eventPoint) => {
                                  contextMenu.popup(pathTextId)
                              }
                }

            }
        }
    }

    SourceLocationDialog {
        id: whereDialogId
        anchors.fill: parent
    }

    LoadProjectDialog {
        id: loadProjectDialogId
        objectName: "loadProjectDialog"
    }
}
