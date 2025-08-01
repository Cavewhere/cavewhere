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

    SourceLocationDialog {
        id: whereDialogId
        anchors.fill: parent
    }

    LoadProjectDialog {
        id: loadProjectDialogId
        objectName: "loadProjectDialog"
    }
}
