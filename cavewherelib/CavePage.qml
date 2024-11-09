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

                    console.log("LastTrip:" + lastTrip + "CavePage:" + cavePageArea.PageView.page);

                    RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                               cavePageArea.tripPageName(lastTrip));
                }
            }


            QC.HorizontalHeaderView {
                id: horizontalHeader
                Layout.fillWidth: true
                model: ListModel {
                    id: fruitModel

                    ListElement {
                        display: "Name"
                    }
                    ListElement {
                        display: "Date"
                    }
                    ListElement {
                        display: "Stations"
                    }
                    ListElement {
                        display: "Length"
                    }
                }
                columnWidthProvider: function (column) {
                    let columnWidth = explicitColumnWidth(column);
                    if(columnWidth == -1) {
                        //column width hasn't been set.
                        switch(column) {
                        case 0:
                            //name column
                            return tableViewId.nameColumnWidth
                        case 1:
                            return tableViewId.dateColumnWidth
                        case 2:
                            return tableViewId.stationColumnWidth
                        case 3:
                            return tableViewId.lengthColumnWidth
                        }
                    }

                    return Math.min(300, Math.max(50, columnWidth));

                }

                // syncView: tableView
                clip: true

                onLayoutChanged: {
                    // Update each ListElement's columnWidth with the current header width
                    tableViewId.nameColumnWidth = horizontalHeader.columnWidth(0);
                    tableViewId.dateColumnWidth = horizontalHeader.columnWidth(1);
                    tableViewId.stationColumnWidth = horizontalHeader.columnWidth(2);
                    tableViewId.lengthColumnWidth = horizontalHeader.columnWidth(3);
                }
            }

            QC.ScrollView {
                id: scrollViewId
                implicitWidth: tableViewId.implicitWidth +
                               QC.ScrollBar.vertical.implicitWidth
                Layout.fillHeight: true

                QQ.ListView {
                    id: tableViewId
                    model: cavePageArea.currentCave

                    implicitWidth: nameColumnWidth
                                   + dateColumnWidth
                                   + stationColumnWidth
                                   + lengthColumnWidth

                    Layout.fillHeight: true
                    clip: true

                    property int nameColumnWidth: 200
                    property int dateColumnWidth: 75
                    property int stationColumnWidth: 75
                    property int lengthColumnWidth: 50

                    component RowDelegate : QQ.Item {
                        id: rowDelegateId
                        required property Trip tripObjectRole
                        required property int index

                        implicitWidth: layoutId.width
                        implicitHeight: layoutId.height

                        TripLengthTask {
                            id: tripLengthTask
                            trip: tripObjectRole
                        }

                        UsedStationTaskManager {
                            id: usedStationTaskManager
                            trip: tripObjectRole
                            bold: false
                            abbreviated: true
                            onlyLargestRange: true
                        }

                        DataRightClickMouseMenu {
                            anchors.fill: parent
                            removeChallenge: removeChallengeId
                            row: rowDelegateId.index
                            name: tripObjectRole.name
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
                                implicitWidth: tableViewId.nameColumnWidth
                                implicitHeight: rowLayout
                                clip: true

                                RowLayout {
                                    id: rowLayout
                                    spacing: 1

                                    ErrorIconBar {
                                        errorModel: tripObjectRole.errorModel
                                    }

                                    LinkText {
                                        text: tripObjectRole.name
                                        elide: Text.ElideRight

                                        onClicked: {
                                            RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                                                       tripPageName(tripObjectRole));
                                        }
                                    }
                                }
                            }

                            QQ.Item {
                                implicitWidth: tableViewId.dateColumnWidth
                                implicitHeight: dateId.implicitHeight
                                clip: true
                                Text {
                                    id: dateId
                                    elide: Text.ElideRight
                                    // anchors.fill: parent
                                    text: Qt.formatDateTime(tripObjectRole.date, "yyyy-MM-dd")
                                }
                            }

                            QQ.Item {
                                implicitWidth: tableViewId.stationColumnWidth
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
                                implicitWidth: tableViewId.lengthColumnWidth
                                implicitHeight: lengthId.implicitHeight
                                clip: true

                                Text {
                                    id: lengthId
                                    elide: Text.ElideRight
                                    // anchors.fill: parent
                                    text: {
                                        var unit = ""
                                        switch(tripObjectRole.calibration.distanceUnit) {
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
