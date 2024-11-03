/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib
import QtQml
import QtQuick as QQ
import QtQuick.Layouts

StandardPage {
    id: pageId

    function cavePageName(cave) {
        return "Cave=" + cave.name;
    }

    RowLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 5

        ColumnLayout {
            Layout.alignment: Qt.AlignTop

            Text {
                font.bold: true
                font.pixelSize: 20
                text: "All Caves"
            }

            ExportImportButtons {
                id: exportButton
                currentRegion: RootData.region
                importVisible: true
                page: pageId.PageView.page
            }

        }

        ColumnLayout {

            AddAndSearchBar {
                Layout.fillWidth: true
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

            QQ.ListView {
                id: tableViewId
                model: RootData.region

                Layout.fillHeight: true
                implicitWidth: 450

                delegate:
                    QQ.Item {
                    id: delegateId
                    required property Cave caveObjectRole;
                    required property int index

                    implicitHeight: layoutId.height
                    implicitWidth: layoutId.width

                    RowLayout {
                        id: layoutId

                        QQ.Connections {
                            target: tableViewId
                            function onCurrentItemChanged() {
                                exportButton.currentCave = tableViewId.currentItem.caveObjectRole
                            }
                        }

                        // RowLayout {
                        //     id: rowLayout
                        //     spacing: 1

                        ErrorIconBar {
                            errorModel: delegateId.caveObjectRole.errorModel
                        }

                        LinkText {
                            text: delegateId.caveObjectRole.name
                            onClicked: {
                                RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                                           cavePageName(delegateId.caveObjectRole));
                            }
                        }
                        // }

                        Text {
                            text: "is"
                        }

                        UnitValueInput {
                            unitValue: delegateId.caveObjectRole.length
                            unitModel: UnitDefaults.lengthModel
                            valueReadOnly: true
                        }

                        Text {
                            text: "long and"
                        }

                        UnitValueInput {
                            unitValue: delegateId.caveObjectRole.depth
                            unitModel: UnitDefaults.depthModel
                            valueReadOnly: true
                        }

                        Text {
                            text: "deep"
                        }
                    }

                    DataRightClickMouseMenu {
                        anchors.fill: parent
                        removeChallenge: removeChallengeId
                        caveName: delegateId.caveObjectRole.name
                        row: delegateId.index
                    }
                }

            }
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
            console.log(`Cave page added! ${index} ${object.caveObjectRole} ${delegate.caveObjectRole}`)
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
