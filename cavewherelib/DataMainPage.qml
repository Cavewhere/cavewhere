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
                    var lastCave = RootData.region.data(lastModelIndex, Cave.TripObjectRole);

                    RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                               pageId.cavePageName(lastCave));
                }
            }

            QQ.TableView {
                id: tableViewId
                model: RootData.region

                Layout.fillHeight: true
                implicitWidth: 450

                // QQ.TableViewColumn { role: "caveObjectRole" ; title: "Cave" ; width: 200 }
                // QQ.TableViewColumn { role: "caveObjectRole" ; title: "Length" ; width: 100 }
                // QQ.TableViewColumn { role: "caveObjectRole" ; title: "Depth" ; width: 100 }

            //     itemDelegate:
            //         QQ.Item {

            //         QQ.Connections {
            //             target: tableViewId
            //             onCurrentRowChanged: {
            //                 exportButton.currentCave = styleData.value
            //             }
            //         }

            //         QQ.Item {
            //             visible: styleData.column === 0

            //             anchors.fill: parent

            //             RowLayout {
            //                 id: rowLayout
            //                 spacing: 1

            //                 ErrorIconBar {
            //                     errorModel: styleData.value.errorModel
            //                 }

            //                 LinkText {
            //                     visible: styleData.column === 0
            //                     text: styleData.value.name
            //                     onClicked: {
            //                         RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
            //                                                                    cavePageName(styleData.value));
            //                     }
            //                 }
            //             }
            //         }

            //         UnitValueInput {
            //             visible: styleData.column === 1 || styleData.column === 2
            //             unitValue: {
            //                 switch(styleData.column) {
            //                 case 1:
            //                     return styleData.value.length
            //                 case 2:
            //                     return styleData.value.depth
            //                 default:
            //                     return null
            //                 }
            //             }
            //             unitModel: {
            //                 switch(styleData.column) {
            //                 case 1:
            //                     return UnitDefaults.lengthModel
            //                 case 2:
            //                     return UnitDefaults.depthModel
            //                 default:
            //                     return null
            //                 }
            //             }

            //             valueReadOnly: true
            //         }

            //         DataRightClickMouseMenu {
            //             anchors.fill: parent
            //             removeChallenge: removeChallengeId
            //         }
            //     }
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
                property Cave caveObjectRole
                property Cave cave: caveObjectRole
                property Page page
        }

        delegate: PageDelegate {}


        onObjectAdded: {
            //In-ables the link
            let delegate = object as PageDelegate
            var linkId = RootData.pageSelectionModel.registerPage(pageId.PageView.page, //From
                                                                  pageId.cavePageName(delegate.cave), //Name
                                                                  caveOverviewPageComponent,
                                                                  {currentCave:delegate.cave}
                                                                  )

            delegate.page = linkId
            delegate.page.setNamingFunction(delegate.cave,
                                          "nameChanged()",
                                          pageId,
                                          "cavePageName",
                                          delegate.cave)
        }

        onObjectRemoved: {
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
