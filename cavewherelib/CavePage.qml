/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQml
import QtQuick.Layouts
import QtQuick.Controls as QC

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
                currentTrip: {
                    let row = (tableViewId.currentItem as RowDelegate);
                    return row ? row.tripObjectRole : null
                }
            }

            QC.Button {
                text: "Leads"
                // icon.source: "qrc:icons/question.png"
                onClicked: {
                    RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
                }
            }
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

                    console.log("LastTrip:" + lastTrip + "CavePage:" + cavePageArea.PageView.page);

                    RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                               cavePageArea.tripPageName(lastTrip));
                }
            }


            ColumnLayout {
                Layout.fillHeight: true
                spacing: 0


                HorizontalHeaderStaticView {
                    view: tableViewId
                    Layout.fillWidth: true
                }


                QC.ScrollView {
                    id: scrollViewId
                    implicitWidth: tableViewId.implicitWidth +
                                   QC.ScrollBar.vertical.implicitWidth
                    Layout.fillHeight: true

                    TableStaticView {
                        id: tableViewId
                        model:  cavePageArea.currentCave

                        //This will populate the HorizontalHeader
                        columnModel.children: [
                            TableStaticColumn { id: nameColumnId; columnWidth: 200; text: "Name" },
                            TableStaticColumn { id: dateColumnId; columnWidth: 75; text: "Date" },
                            TableStaticColumn { id: stationsColumnId; columnWidth: 75; text: "Stations" },
                            TableStaticColumn { id: lengthColumnId; columnWidth: 50; text: "Length" }
                        ]

                        Layout.fillHeight: true

                        component RowDelegate : QQ.Item {
                            id: rowDelegateId
                            required property Trip tripObjectRole
                            required property int index

                            implicitWidth: layoutId.width
                            implicitHeight: layoutId.height

                            TripLengthTask {
                                id: tripLengthTask
                                trip: rowDelegateId.tripObjectRole
                            }

                            UsedStationTaskManager {
                                id: usedStationTaskManager
                                trip: rowDelegateId.tripObjectRole
                                bold: false
                                abbreviated: true
                                onlyLargestRange: true
                            }

                            DataRightClickMouseMenu {
                                anchors.fill: parent
                                removeChallenge: removeChallengeId
                                row: rowDelegateId.index
                                name: rowDelegateId.tripObjectRole.name
                            }

                            TableRowBackground {
                                isSelected: tableViewId.currentIndex == rowDelegateId.index
                                rowIndex: rowDelegateId.index
                            }

                            QQ.MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton

                                onClicked: {
                                    tableViewId.currentIndex = rowDelegateId.index
                                }
                            }

                            RowLayout {
                                id: layoutId

                                spacing: 0

                                // QQ.Item {
                                // anchors.fill: parent

                                QQ.Item {
                                    implicitWidth: nameColumnId.width
                                    implicitHeight: rowLayout
                                    clip: true

                                    RowLayout {
                                        id: rowLayout
                                        spacing: 1

                                        ErrorIconBar {
                                            errorModel: rowDelegateId.tripObjectRole.errorModel
                                        }

                                        LinkText {
                                            text: tripObjectRole.name
                                            elide: Text.ElideRight

                                            onClicked: {
                                                RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                                                           tripPageName(rowDelegateId.tripObjectRole));
                                            }
                                        }
                                    }
                                }

                                QQ.Item {
                                    implicitWidth: dateColumnId.width
                                    implicitHeight: dateId.implicitHeight
                                    clip: true
                                    Text {
                                        id: dateId
                                        elide: Text.ElideRight
                                        // anchors.fill: parent
                                        text: Qt.formatDateTime(rowDelegateId.tripObjectRole.date, "yyyy-MM-dd")
                                    }
                                }

                                QQ.Item {
                                    implicitWidth: stationsColumnId.width
                                    implicitHeight: usedStationsId.implicitHeight
                                    clip: true

                                    Text {
                                        id: usedStationsId
                                        elide: Text.ElideRight
                                        // anchors.fill: parent
                                        text: {
                                            if(usedStationTaskManager.usedStations.length > 0) {
                                                return usedStationTaskManager.usedStations[0]
                                            }
                                            return ""
                                        }
                                    }
                                }

                                QQ.Item {
                                    implicitWidth: lengthColumnId.width
                                    implicitHeight: lengthId.implicitHeight
                                    clip: true

                                    Text {
                                        id: lengthId
                                        elide: Text.ElideRight
                                        // anchors.fill: parent
                                        text: {
                                            var unit = ""
                                            switch(rowDelegateId.tripObjectRole.calibration.distanceUnit) {
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
                                }
                            }
                        }

                        delegate: RowDelegate {}
                    }
                }
            }
        }

        UsedStationsWidget {
            id: usedStationWidgetId
            cave: cavePageArea.currentCave

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
            required property Trip tripObjectRole
            property Page page
        }

        delegate: Delegate {
        }

        onObjectAdded: (index, object) => {
                           console.log("Add trip page!" + index + " " + object)


                           //In-ables the link
                           let trip = (object as Delegate).tripObjectRole
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
