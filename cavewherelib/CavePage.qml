/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQml
import QtQuick.Layouts

StandardPage {
    id: cavePageArea

    property Cave currentCave

    function tripPageName(trip) {
        return "Trip=" + trip.name;
    }

    function registerSubPages() {
        if(currentCave) {
            var oldCarpetPage = PageView.page.childPage("Leads")
            if(oldCarpetPage !== RootData.pageSelectionModel.currentPage) {
                if(oldCarpetPage !== null) {
                    RootData.pageSelectionModel.unregisterPage(oldCarpetPage)
                }

                if(PageView.page.name !== "Leads") {
                    var page = RootData.pageSelectionModel.registerPage(PageView.page,
                                                                        "Leads",
                                                                        caveLeadsPage,
                                                                        {"cave":currentCave});
                }
            }
        }
    }

    PageView.onPageChanged: registerSubPages()

    onCurrentCaveChanged: {
        instantiatorId.model = cavePageArea.currentCave
        registerSubPages()
    }

    QQ.Component {
        id: caveLeadsPage
        CaveLeadPage {
            anchors.fill: parent

        }
    }

    RowLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        ColumnLayout {
            Layout.alignment: Qt.AlignTop

            DoubleClickTextInput {
                id: caveNameText
                text: cavePageArea.currentCave.name
                font.bold: true
                font.pixelSize: 20

                onFinishedEditting: (newText) => {
                    cavePageArea.currentCave.name = newText
                }
            }

            QQ.Rectangle {
                id: lengthDepthContainerId

                color: "lightgray"
                //            anchors.left: parent.left
                //            anchors.top: usedStationWidgetId.top
                //            anchors.margins: 5

                implicitWidth:  caveLengthAndDepthId.width + 10
                implicitHeight: caveLengthAndDepthId.height + 10

                CaveLengthAndDepth {
                    id: caveLengthAndDepthId

                    anchors.centerIn: parent

                    currentCave: cavePageArea.currentCave
                }
            }

            ExportImportButtons {
                id: exportButton
                currentRegion: RootData.region
                currentCave: cavePageArea.currentCave
            }

            CWButton {
                text: "Leads"
                iconSource: "qrc:icons/question.png"
                onClicked: {
                    RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
                }
            }


//            QQ.Rectangle {
//                color: "gray"

//                width: 100
//                height: 40

//                Text {
//                    anchors.centerIn: parent
//                    text: "Leads"
//                }

//                QQ.MouseArea {
//                    anchors.fill: parent
//                    onClicked:  {
//            RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
//                    }
//                }
//            }
        }

        ColumnLayout {

            AddAndSearchBar {
                Layout.fillWidth: true
                addButtonText: "Add Trip"
                onAdd: {
                    cavePageArea.currentCave.addTrip()

                    var lastIndex = cavePageArea.currentCave.rowCount() - 1;
                    var lastModelIndex = cavePageArea.currentCave.index(lastIndex);
                    var lastTrip = cavePageArea.currentCave.data(lastModelIndex, Cave.TripObjectRole);

                    RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                               cavePageArea.tripPageName(lastTrip));
                }
            }

            QQ.TableView {
                id: tableViewId
                model: cavePageArea.currentCave

                implicitWidth: 600

                Layout.fillHeight: true

                //            anchors.top: parent.top
                //            anchors.bottom: parent.bottom

                // Controls.TableViewColumn { role: "tripObjectRole"; title: "Trip"; }
                // Controls.TableViewColumn { role: "tripObjectRole"; title: "Date"; }
                // Controls.TableViewColumn { role: "tripObjectRole"; title: "Survey"; width: 100 }
                // Controls.TableViewColumn { role: "tripObjectRole"; title: "Length"; width: 100 }

                // itemDelegate:
                //     QQ.Item {
                //     clip: true

                //     QQ.Connections {
                //         target: tableViewId
                //         onCurrentRowChanged: {
                //             if(tableViewId.currentRow === styleData.row) {
                //                 exportButton.currentTrip = styleData.value
                //             }
                //         }
                //     }

                //     TripLengthTask {
                //         id: tripLengthTask
                //         trip: styleData.value
                //     }

                //     UsedStationTaskManager {
                //         id: usedStationTaskManager
                //         trip: styleData.value
                //         bold: false
                //         abbreviated: true
                //         onlyLargestRange: true
                //     }

                //     QQ.Item {
                //         visible: styleData.column === 0

                //         anchors.fill: parent

                //         RowLayout {
                //             id: rowLayout
                //             spacing: 1

                //             ErrorIconBar {
                //                 errorModel: styleData.value.errorModel
                //             }

                //             LinkText {
                //                 text: styleData.value.name
                //                 elide: Text.ElideRight

                //                 onClicked: {
                //                     RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                //                                                                tripPageName(styleData.value));
                //                 }
                //             }
                //         }
                //     }

                //     Text {
                //         visible: styleData.column === 1
                //         elide: Text.ElideRight
                //         anchors.fill: parent
                //         text: Qt.formatDateTime(styleData.value.date, "yyyy-MM-dd")
                //     }

                //     Text {
                //         visible: styleData.column === 2
                //         elide: Text.ElideRight
                //         anchors.fill: parent
                //         text: {
                //             if(usedStationTaskManager.usedStations.length > 0) {
                //                 return usedStationTaskManager.usedStations[0]
                //             }
                //             return ""
                //         }
                //     }

                //     Text {
                //         visible: styleData.column === 3
                //         elide: Text.ElideRight
                //         anchors.fill: parent
                //         text: {
                //             var unit = ""
                //             switch(styleData.value.calibration.distanceUnit) {
                //             case Units.Meters:
                //                 unit = "m"
                //                 break;
                //             case Units.Feet:
                //                 unit = "ft"
                //                 break;
                //             }

                //             return Utils.fixed(tripLengthTask.length, 2) + " " + unit;
                //         }
                //     }

                //     DataRightClickMouseMenu {
                //         anchors.fill: parent
                //         removeChallenge: removeChallengeId
                //     }
                // }
            }
        }

        UsedStationsWidget {
            id: usedStationWidgetId

            Layout.fillHeight: true

            implicitWidth: 250
        }
    }


    RemoveAskBox {
        id: removeChallengeId
        onRemove: {
            cavePageArea.currentCave.removeTrip(indexToRemove)
        }
    }

    Instantiator {
        id: instantiatorId

        component Delegate: QQ.QtObject {
            id: delegateObjectId
            property Trip tripObjectRole
            property alias trip: delegateObjectId.tripObjectRole
            property Page page
        }

        delegate: Delegate {
        }

        onObjectAdded: (index, object) => {
            //In-ables the link
            let trip = (object as Delegate).trip
            var page = RootData.pageSelectionModel.registerPage(cavePageArea.PageView.page, //From
                                                                cavePageArea.tripPageName(trip), //Name
                                                                tripPageComponent, //component
                                                                {"currentTrip":trip}
                                                                )
            object.page = page;
            page.setNamingFunction(trip, //The trip that's signaling
                                   "nameChanged()", //Signal
                                   cavePageArea, //The object that has renaming function
                                   "tripPageName", //The function that will generate the name
                                   trip) //The paramaters to tripPageName() function
        }

        onObjectRemoved: (index, object) => {
            RootData.pageSelectionModel.unregisterPage((object as Delegate).page);
        }
    }

    QQ.Component {
        id: tripPageComponent
        TripPage {
            anchors.fill: parent
        }
    }
}
