pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: pageId

    required property AskToSaveDialog askToSaveDialog
    property string filterText: ""

    SortFilterProxyModel {
        id: recentProxyId
        source: RootData.recentProjectModel
        sortRole: RecentProjectModel.LastOpenedRole
        sortCaseSensitivity: Qt.CaseInsensitive
        filterRole: "pathRole"
        filterCaseSensitivity: Qt.CaseInsensitive
        filterString: "*" + pageId.filterText + "*"

        Component.onCompleted: sort(Qt.DescendingOrder)
    }

    TableStaticColumnModel {
        id: columnModelId
        columns: [
            TableStaticColumn {
                id: nameColumnId
                columnWidth: listViewId.width - lastOpenedColumnId.columnWidth
                text: qsTr("Name")
                sortRole: RecentProjectModel.NameRole
            },
            TableStaticColumn {
                id: lastOpenedColumnId
                columnWidth: Theme.recentProjectDateColumnWidth
                text: qsTr("Last opened")
                sortRole: RecentProjectModel.LastOpenedRole
            }
        ]
    }

    ColumnLayout {
        width: listViewId.implicitWidth
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Theme.pageMargin

        RowLayout {
            id: rowLayoutId
            Layout.fillWidth: true
            spacing: Theme.columnGap

            QC.Button {
                id: addButton
                objectName: "addRepositoryButton"
                text: "Add"
                icon.source: "qrc:/twbs-icons/icons/plus.svg"
                onClicked: addMenu.popup(addButton)
            }

            Item { Layout.fillWidth: true }

            QC.TextField {
                id: filterFieldId
                objectName: "recentProjectFilterField"
                Layout.preferredWidth: Theme.fieldWidth
                placeholderText: qsTr("Filter…")
                inputMethodHints: Qt.ImhNoPredictiveText
                text: pageId.filterText
                onTextEdited: pageId.filterText = text
            }

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

        HorizontalHeaderStaticView {
            view: listViewId
            visible: listViewId.visible

            delegate: TableStaticHeaderColumn {
                objectName: sortRole === RecentProjectModel.NameRole ? "recentHeaderName" : "recentHeaderLastOpened"
                model: recentProxyId

                // Match the indicator to the proxy's default Last opened, newest-first order.
                Component.onCompleted: {
                    if (sortRole === recentProxyId.sortRole) {
                        sortMode = TableStaticHeaderColumn.Sort.Descending
                    }
                }
            }
        }

        TableStaticView {
            id: listViewId
            objectName: "repositoryListView"

            columnModel: columnModelId
            implicitWidth: Math.min(pageId.width, 500)
            Layout.fillHeight: true
            visible: count > 0
            QC.ScrollBar.vertical: QC.ScrollBar {
                policy: QC.ScrollBar.AsNeeded
            }

            model: recentProxyId

            delegate: TableRowBackground {
                id: delegateId

                required property string nameRole
                required property string pathRole
                required property date lastOpenedRole
                required property int index

                width: listViewId.width
                implicitHeight: contentRowId.implicitHeight + Theme.pageMargin * 2
                rowIndex: index
                isSelected: false

                RowLayout {
                    id: contentRowId
                    anchors.fill: parent
                    anchors.leftMargin: Theme.pageMargin
                    anchors.topMargin: Theme.pageMargin
                    anchors.bottomMargin: Theme.pageMargin
                    spacing: 0

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.delegatePadding

                        LinkText {
                            id: linkTextId
                            objectName: "repoLinkText_" + delegateId.index
                            Layout.fillWidth: true
                            text: delegateId.nameRole
                            elide: Text.ElideRight

                            onClicked: {
                                const sourceRow = recentProxyId.mapToSource(recentProxyId.index(delegateId.index, 0)).row
                                const fileResult = RootData.recentProjectModel.repositoryProjectFile(sourceRow)
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
                            Layout.leftMargin: Theme.actionBarSpacing
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

                    QC.Label {
                        objectName: "repoLastOpened_" + delegateId.index
                        Layout.preferredWidth: lastOpenedColumnId.columnWidth
                        Layout.alignment: Qt.AlignTop
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.textSecondary
                        text: isNaN(delegateId.lastOpenedRole.getTime())
                              ? "—"
                              : Qt.formatDateTime(delegateId.lastOpenedRole, "yyyy-MM-dd HH:mm")
                    }
                }
            }
        }

        QC.Label {
            objectName: "recentProjectsEmptyLabel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            visible: listViewId.count === 0
            text: RootData.recentProjectModel.count > 0
                  ? qsTr("No projects match the filter.")
                  : qsTr("No caving areas created or opened yet.")
        }
    }

    LoadProjectDialog {
        id: loadProjectDialogId
        objectName: "loadProjectDialog"
    }
}
