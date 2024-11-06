/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//This cause a crash when creating the context for sub trip pages
//cwPageView: "qrc:/cavewherelib/cavewherelib/DataMainPage.qml: Cannot instantiate bound component outside its creation context"
// pragma ComponentBehavior: Bound

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

                component CaveDelegate: QQ.Item {
                    id: delegateId
                    required property Cave caveObjectRole;
                    required property int index

                    implicitHeight: layoutId.height
                    implicitWidth: layoutId.width

                    QQ.Rectangle {
                        anchors.fill: parent
                        color: {
                            if(tableViewId.currentIndex == delegateId.index) {
                                //Selected
                                return "#d6e6ff"
                            } else {
                                //Alternating color background
                                if(delegateId.index % 2 == 1) {
                                    return "#f2f2f2"
                                } else {
                                    return "white"
                                }
                            }
                        }
                    }

                    QQ.MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton

                        onClicked: {
                            tableViewId.currentIndex = delegateId.index
                        }
                    }

                    RowLayout {
                        id: layoutId

                        ErrorIconBar {
                            errorModel: delegateId.caveObjectRole.errorModel
                        }

                        LinkText {
                            text: delegateId.caveObjectRole.name
                            onClicked: {
                                RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                                           pageId.cavePageName(delegateId.caveObjectRole));
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

                delegate: CaveDelegate {}
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
