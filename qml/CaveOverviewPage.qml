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
    onCurrentCaveChanged: {
        instantiatorId.model = cavePageArea.currentCave
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

                    LinkText {
                        visible: styleData.column === 0
                        text: styleData.value.name
                        onClicked: {
                            rootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                                       tripPageName(styleData.value));
                        }
                    }


                    Text {
                        visible: styleData.column === 1
                        text: Qt.formatDateTime(styleData.value.date, "yyyy-MM-dd")
                    }

                    Text {
                        visible: styleData.column === 2
                        text: {
                            if(usedStationTaskManager.usedStations.length > 0) {
                                return usedStationTaskManager.usedStations[0]
                            }
                            return ""
                        }
                    }

                    Text {
                        visible: styleData.column === 3
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
            currentCave.removeTrip(index)
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
                                                                surveyEditorComponent, //Function
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
        id: surveyEditorComponent
        SurveyEditor {
            anchors.fill: parent
        }
    }
}
