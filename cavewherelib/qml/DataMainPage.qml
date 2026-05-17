/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//This cause a crash when creating the context for sub trip pages
//cwPageView: "qrc:/qt/qml/cavewherelib/qml/DataMainPage.qml: Cannot instantiate bound component outside its creation context"
// pragma ComponentBehavior: Bound

import cavewherelib
import QtQml
import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
StandardPage {
    id: pageId

    objectName: "dataMainPage"

    function cavePageName(cave) {
        return "Cave=" + cave.name;
    }

    function registerGeospatialPage() {
        if (PageView.page === null) return
        if (PageView.page.childPage("Geospatial Layers") !== null) return
        RootData.pageSelectionModel.registerPage(PageView.page,
                                                 "Geospatial Layers",
                                                 geospatialLayerPageComponent);
    }

    PageView.onPageChanged: registerGeospatialPage()

    // cwPageSelectionModel::clearHistory() (invoked by newProject/loadProject)
    // wipes "Data"'s child pages but keeps DataMainPage cached, so
    // PageView.onPageChanged won't re-fire. Listen on the global signal so
    // the link is reachable again after a project reset.
    Connections {
        target: RootData.pageSelectionModel
        function onCurrentPageChanged() {
            pageId.registerGeospatialPage()
        }
    }

    QQ.Component {
        id: geospatialLayerPageComponent
        GeospatialLayerPage {
            anchors.fill: parent
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            DoubleClickTextInput {
                objectName: "regionNameInput"
                font.bold: true
                font.pixelSize: Theme.fontSizeTitle
                text: RootData.region.name

                onFinishedEditting: (newText) => {
                    RootData.region.name = newText
                }
            }

            QQ.Item { Layout.fillWidth: true }

            ContextMenuButton {
                objectName: "regionContextMenu"
                menu: regionContextMenuComponent
            }
        }

        QC.GroupBox {
            objectName: "geospatialGroupBox"
            title: "Geospatial"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.tightSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    QC.Label {
                        text: "Coordinate system:"
                    }

                    CSComboBox {
                        objectName: "globalCoordinateSystemComboBox"
                        value: RootData.region.globalCoordinateSystem
                        allowGeographic: false
                        onCommitted: (newCS) => {
                            RootData.region.globalCoordinateSystem = newCS
                        }
                    }

                    QQ.Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    QC.Label {
                        text: "Layers:"
                    }

                    LinkText {
                        objectName: "geospatialLayersLink"
                        text: RootData.region.lazLayers.count
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                                       "Geospatial Layers");
                        }
                    }

                    QQ.Item { Layout.fillWidth: true }
                }
            }
        }

        QQ.Component {
            id: regionContextMenuComponent
            QC.Menu {
                QC.MenuItem {
                    objectName: "recenterWorldOriginAction"
                    text: qsTr("Recenter world origin")
                    enabled: RootData.region.globalCoordinateSystem !== ""
                    onTriggered: RootData.region.recomputeWorldOrigin()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            AddAndSearchBar {
                addButtonText: "Add Cave"
                onAdd: {
                    RootData.region.addCave();

                    var lastIndex = RootData.region.rowCount() - 1;
                    var lastModelIndex = RootData.region.index(lastIndex);
                    var lastCave = RootData.region.data(lastModelIndex, CavingRegion.CaveObjectRole);

                    RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                               pageId.cavePageName(lastCave));
                }
            }

            ExportImportButtons {
                id: exportButton
                visible: RootData.desktopBuild
                currentRegion: RootData.region
                importVisible: true
                page: pageId.PageView.page
                currentCave: {
                    let currentItem = tableViewId.currentItem as CaveDelegate
                    if(currentItem) {
                        return currentItem.caveObjectRole
                    } else {
                        return null;
                    }
                }
            }
        }

        QQ.ListView {
            id: tableViewId
            model: RootData.region

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            component CaveDelegate: QQ.Item {
                id: delegateId
                objectName: "caveDelegate" + index

                required property Cave caveObjectRole;
                required property int index

                implicitHeight: flowId.implicitHeight + Theme.delegatePadding
                width: QQ.ListView.view ? QQ.ListView.view.width : 0

                TableRowBackground {
                    isSelected: tableViewId.currentIndex == delegateId.index
                    rowIndex: delegateId.index
                    anchors.fill: parent
                }

                QQ.MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton

                    onClicked: {
                        tableViewId.currentIndex = delegateId.index
                    }
                }

                QQ.Flow {
                    id: flowId
                    width: parent.width
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter

                    ErrorIconBar {
                        errorModel: delegateId.caveObjectRole.errorModel
                    }

                    LinkText {
                        objectName: "caveLink"
                        text: delegateId.caveObjectRole.name
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                                       pageId.cavePageName(delegateId.caveObjectRole));
                        }
                    }

                    QC.Label {
                        text: "is"
                    }

                    UnitValueInput {
                        objectName: "length"
                        unitValue: delegateId.caveObjectRole.length
                        unitModel: UnitDefaults.lengthModel
                        valueReadOnly: true
                    }

                    QC.Label {
                        text: "long and"
                    }

                    UnitValueInput {
                        objectName: "depth"
                        unitValue: delegateId.caveObjectRole.depth
                        unitModel: UnitDefaults.depthModel
                        valueReadOnly: true
                    }

                    QC.Label {
                        text: "deep"
                    }
                }

                DataRightClickMouseMenu {
                    anchors.fill: parent
                    removeChallenge: removeChallengeId
                    name: delegateId.caveObjectRole.name
                    row: delegateId.index
                }
            }

            delegate: CaveDelegate {}
        }
    }


    RemoveAskBox {
        id: removeChallengeId
        onRemove: {
            RootData.region.removeCave(indexToRemove);
        }
    }

    Instantiator {
        model: RootData.region
        component PageDelegate : QQ.QtObject {
            id: delegateObjectId
            required property Cave caveObjectRole
            property Page page
        }

        delegate: PageDelegate {}


        onObjectAdded: function (index, object) {
            //In-ables the link

            let delegate = object as PageDelegate
            // console.log(`Cave page added! ${index} ${object.caveObjectRole} ${delegate.caveObjectRole}`)
            var linkId = RootData.pageSelectionModel.registerPage(pageId.PageView.page, //From
                                                                  pageId.cavePageName(delegate.caveObjectRole), //Name
                                                                  caveOverviewPageComponent,
                                                                  {currentCave:delegate.caveObjectRole}
                                                                  )

            delegate.page = linkId
            delegate.page.setNamingFunction(delegate.caveObjectRole,
                                            "nameChanged()",
                                            pageId,
                                            "cavePageName",
                                            delegate.caveObjectRole)
        }

        onObjectRemoved: function (index, object) {
            let delegate = object as PageDelegate
            RootData.pageSelectionModel.unregisterPage(delegate.page);
        }
    }

    //Child page
    QQ.Component {
        id: caveOverviewPageComponent
        CavePage {
            anchors.fill: parent
        }
    }
}
