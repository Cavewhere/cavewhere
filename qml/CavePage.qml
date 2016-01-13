/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import QtQml 2.2
import QtQuick.Controls 1.2 as Controls;
import QtQuick.Layouts 1.1
import "Utils.js" as Utils

Rectangle {
    id: cavePageArea
    anchors.fill: parent;

    property Cave currentCave

    function tripPageName(trip) {
        return "Trip=" + trip.name;
    }

    function registerSubPages() {
        var oldCarpetPage = PageView.page.childPage("Leads")
        if(oldCarpetPage !== rootData.pageSelectionModel.currentPage) {
            if(oldCarpetPage !== null) {
                rootData.pageSelectionModel.unregisterPage(oldCarpetPage)
            }

            if(PageView.page.name !== "Leads") {
                var page = rootData.pageSelectionModel.registerPage(PageView.page,
                                                                    "Leads",
                                                                    caveLeadsPage,
                                                                    {"cave":currentCave});
            }
        }
    }

    PageView.onPageChanged: registerSubPages()

    onCurrentCaveChanged: {
        instantiatorId.model = cavePageArea.currentCave
        registerSubPages()
    }

    Component {
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
                text: currentCave.name
                font.bold: true
                font.pointSize: 20

                onFinishedEditting: {
                    currentCave.name = newText
                }
            }

            Rectangle {
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
                currentRegion: rootData.region
                currentCave: cavePageArea.currentCave
            }

            CWButton {
                text: "Leads"
                iconSource: "qrc:icons/question.png"
                onClicked: {
                    rootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
                }
            }


//            Rectangle {
//                color: "gray"

//                width: 100
//                height: 40

//                Text {
//                    anchors.centerIn: parent
//                    text: "Leads"
//                }

//                MouseArea {
//                    anchors.fill: parent
//                    onClicked:  {
//            rootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
//                    }
//                }
//            }
        }

        ColumnLayout {

            AddAndSearchBar {
                Layout.fillWidth: true
                addButtonText: "Add Trip"
                onAdd: {
                    currentCave.addTrip()

                    var lastIndex = currentCave.rowCount() - 1;
                    var lastModelIndex = currentCave.index(lastIndex);
                    var lastTrip = currentCave.data(lastModelIndex, Cave.TripObjectRole);

                    rootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                               tripPageName(lastTrip));
                }
            }

            Controls.TableView {
                id: tableViewId
                model: currentCave

                implicitWidth: 600

                Layout.fillHeight: true

                //            anchors.top: parent.top
                //            anchors.bottom: parent.bottom

                Controls.TableViewColumn{ role: "tripObjectRole"; title: "Trip"; }
                Controls.TableViewColumn{ role: "tripObjectRole"; title: "Date"; }
                Controls.TableViewColumn{ role: "tripObjectRole"; title: "Survey"; width: 100 }
                Controls.TableViewColumn{ role: "tripObjectRole"; title: "Length"; width: 100 }

                itemDelegate:
                    Item {
                    clip: true

                    Connections {
                        target: tableViewId
                        onCurrentRowChanged: {
                            if(tableViewId.currentRow === styleData.row) {
                                exportButton.currentTrip = styleData.value
                            }
                        }
                    }

                    TripLengthTask {
                        id: tripLengthTask
                        trip: styleData.value
                    }

                    UsedStationTaskManager {
                        id: usedStationTaskManager
                        trip: styleData.value
                        bold: false
                        abbreviated: true
                        onlyLargestRange: true
                    }

                    Item {
                        visible: styleData.column === 0

                        anchors.fill: parent

                        RowLayout {
                            id: rowLayout
                            spacing: 1

                            ErrorIconBar {
                                errorModel: styleData.value.errorModel
                            }

                            LinkText {
                                text: styleData.value.name
                                elide: Text.ElideRight

                                onClicked: {
                                    rootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                                               tripPageName(styleData.value));
                                }
                            }
                        }
                    }

                    Text {
                        visible: styleData.column === 1
                        elide: Text.ElideRight
                        anchors.fill: parent
                        text: Qt.formatDateTime(styleData.value.date, "yyyy-MM-dd")
                    }

                    Text {
                        visible: styleData.column === 2
                        elide: Text.ElideRight
                        anchors.fill: parent
                        text: {
                            if(usedStationTaskManager.usedStations.length > 0) {
                                return usedStationTaskManager.usedStations[0]
                            }
                            return ""
                        }
                    }

                    Text {
                        visible: styleData.column === 3
                        elide: Text.ElideRight
                        anchors.fill: parent
                        text: {
                            var unit = ""
                            switch(styleData.value.calibration.distanceUnit) {
                            case Units.Meters:
                                unit = "m"
                                break;
                            case Units.Feet:
                                unit = "ft"
                                break;
                            }

                            return Utils.fixed(tripLengthTask.length, 2) + " " + unit;
                        }
                    }

                    DataRightClickMouseMenu {
                        anchors.fill: parent
                        removeChallenge: removeChallengeId
                    }
                }
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
            currentCave.removeTrip(indexToRemove)
        }
    }

    Instantiator {
        id: instantiatorId

        delegate: QtObject {
            id: delegateObjectId
            property Trip trip: tripObjectRole
            property Page page
        }

        onObjectAdded: {
            //In-ables the link
            var page = rootData.pageSelectionModel.registerPage(cavePageArea.PageView.page, //From
                                                                tripPageName(object.trip), //Name
                                                                tripPageComponent, //component
                                                                {"currentTrip":object.trip}
                                                                )
            object.page = page;
            page.setNamingFunction(object.trip, //The trip that's signaling
                                   "nameChanged()", //Signal
                                   cavePageArea, //The object that has renaming function
                                   "tripPageName", //The function that will generate the name
                                   object.trip) //The paramaters to tripPageName() function
        }

        onObjectRemoved: {
            rootData.pageSelectionModel.unregisterPage(object.page);
        }
    }

    Component {
        id: tripPageComponent
        TripPage {
            anchors.fill: parent
        }
    }
}
