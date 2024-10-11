/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib
import QtQml 2.2
import QtQuick as QQ
import QtQuick.Controls as Controls;
import QtQuick.Layouts 1.1

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
                currentRegion: rootData.region
                importVisible: true
                page: pageId.PageView.page
            }

        }

        ColumnLayout {

            AddAndSearchBar {
                Layout.fillWidth: true
                addButtonText: "Add Cave"
                onAdd: {
                    region.addCave();

                    var lastIndex = rootData.region.rowCount() - 1;
                    var lastModelIndex = rootData.region.index(lastIndex);
                    var lastCave = rootData.region.data(lastModelIndex, Cave.TripObjectRole);

                    rootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                               cavePageName(lastCave));
                }
            }

            QQ.TableView {
                id: tableViewId
                model: rootData.region

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
            //                         rootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
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
            rootData.region.removeCave(indexToRemove);
        }
    }

    Instantiator {
        model: rootData.region
        delegate: QQ.QtObject {
            id: delegateObjectId
            property Cave cave: caveObjectRole
            property Page page
        }

        onObjectAdded: {
            //In-ables the link
            var linkId = rootData.pageSelectionModel.registerPage(pageId.PageView.page, //From
                                                                  cavePageName(object.cave), //Name
                                                                  caveOverviewPageComponent,
                                                                  {currentCave:object.cave}
                                                                  )

            object.page = linkId
            object.page.setNamingFunction(object.cave,
                                          "nameChanged()",
                                          pageId,
                                          "cavePageName",
                                          object.cave)
        }

        onObjectRemoved: {
            rootData.pageSelectionModel.unregisterPage(object.page);
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
