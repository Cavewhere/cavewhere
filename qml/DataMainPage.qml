/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import Cavewhere 1.0
import QtQml 2.2
import QtQuick 2.0
import QtQuick.Controls 1.2 as Controls;
import QtQuick.Layouts 1.1

Rectangle {
    id: pageId

    anchors.fill: parent

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
                font.pointSize: 20
                text: "All Caves"
            }

            ExportImportButtons {
                id: exportButton
                currentRegion: rootData.region
                importVisible: true
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

            Controls.TableView {
                id: tableViewId
                model: rootData.region

                Layout.fillHeight: true
                implicitWidth: 450

                Controls.TableViewColumn{ role: "caveObjectRole" ; title: "Cave" ; width: 200 }
                Controls.TableViewColumn{ role: "caveObjectRole" ; title: "Length" ; width: 100 }
                Controls.TableViewColumn{ role: "caveObjectRole" ; title: "Depth" ; width: 100 }

                itemDelegate:
                    Item {

                    Connections {
                        target: tableViewId
                        onCurrentRowChanged: {
                            exportButton.currentCave = styleData.value
                        }
                    }

                    LinkText {
                        visible: styleData.column === 0
                        text: styleData.value.name
                        onClicked: {
                            rootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                                       cavePageName(styleData.value));
                        }
                    }

                    UnitValueInput {
                        visible: styleData.column === 1 || styleData.column === 2
                        unitValue: {
                            switch(styleData.column) {
                            case 1:
                                return styleData.value.length
                            case 2:
                                return styleData.value.depth
                            default:
                                return null
                            }
                        }
                        valueReadOnly: true
                    }

                    DataRightClickMouseMenu {
                        anchors.fill: parent
                        removeChallenge: removeChallengeId
                    }
                }
            }
        }
    }

    RemoveAskBox {
        id: removeChallengeId
        onRemove: {
            rootData.region.removeCave(index);
        }
    }

    Instantiator {
        model: rootData.region
        delegate: QtObject {
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
    Component {
        id: caveOverviewPageComponent
        CaveOverviewPage {
            anchors.fill: parent
        }
    }

    //    /**
    //      This is used to test if the current Index in the dataSideBar is of type type.

    //      If the type is equal to the current index's type, this returns true, otherwise false.
    //      */
    //    function currentIndexIsType(type) {
    //        var index = dataSideBar.caveSidebar.currentIndex
    //        var indexsType = regionModel.data(index, RegionTreeModel.TypeRole);
    //        return (indexsType === type);
    //    }

    //    /**
    //      This takes the currently selected index in the dataSideBar and gets the object

    //      This function returns null if the select index is not of 'type'.  If it is,
    //      it returns the object
    //      */
    //    function getObject(type) {
    //        if(currentIndexIsType(type)) {
    //            var index = dataSideBar.caveSidebar.currentIndex
    //            return regionModel.data(index, RegionTreeModel.ObjectRole);
    //        }
    //        return null;
    //    }

    //    function resetSideBar() {
    //        dataSideBar.caveSidebar.reset();
    //    }


    //    DataSideBar {
    //        id: dataSideBar
    //        width: 300;
    //        anchors.bottom: parent.bottom
    //        anchors.top: parent.top
    //        anchors.left: parent.left
    //        anchors.topMargin: 5
    //    }


    //    Image {
    //        fillMode: Image.TileVertically
    //        source: "qrc:icons/verticalLine.png"
    //        height: dataSideBar.anchors.topMargin
    //        anchors.left: dataSideBar.right
    //        anchors.leftMargin: -4
    //        anchors.top: parent.top
    //       // width: 5
    //        z:2
    //    }


    //    Splitter {
    //        id: splitter
    //        anchors.bottom: parent.bottom
    //        anchors.top: parent.top
    //        anchors.left: dataSideBar.right
    //        resizeObject: dataSideBar
    //    }


    //    Rectangle {
    //        id: caveTabs
    //        anchors.top: parent.top
    //        anchors.bottom: parent.bottom
    //        anchors.left: dataSideBar.right
    //        anchors.right: parent.right
    //        anchors.leftMargin: -1


    //        AllCavesTabWidget {
    //            anchors.fill: parent
    //            visible: currentIndexIsType(RegionTreeModel.CaveType)
    //        }

    //        CaveTabWidget {
    //            anchors.fill: parent
    //            visible: currentIndexIsType(RegionTreeModel.CaveType)
    //            currentCave: getObject(RegionTreeModel.CaveType)
    //        }

    //        TripTabWidget {
    //            anchors.fill: parent
    //            visible: currentIndexIsType(RegionTreeModel.TripType)
    //            currentTrip: getObject(RegionTreeModel.TripType)

    //        }
    //    }
}
