pragma ComponentBehavior: Bound


import QtQuick as QQ
import QtQml
// import QtQuick.Controls as QC1
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: leadPage

    property Cave cave;

    LeadModel {
        id: leadModel
        regionModel: RootData.regionTreeModel
        cave: leadPage.cave
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 3

        RowLayout {
            QC.TextField {
                id: searchBox

                placeholderText: "Filter by description..."
                inputMethodHints: Qt.ImhNoPredictiveText

                Layout.alignment: Qt.AlignRight

                implicitWidth: 150
            }

            QQ.Item { implicitWidth: 50 }

            Text {
                text: "Lead Distance from:"
            }

            ClickTextInput {
                text: leadModel.referanceStation.length === 0 ? "No Station" : leadModel.referanceStation
                onFinishedEditting: (newText) => {
                    leadModel.referanceStation = newText
                }
            }

            InformationButton {
                onClicked: distanceStationHelpArea.visible = !distanceStationHelpArea.visible
            }

            HelpArea {
                id: distanceStationHelpArea
                implicitWidth: 200
                text: "Lead distance from a station, calculates the <b>line of sight</b> distance from the station to all the leads."
            }
        }

        LinkGenerator {
            id: linkGenerator
            pageSelectionModel: RootData.pageSelectionModel
        }

        HorizontalHeaderStaticView {
            view: tableView
            Layout.fillWidth: true
        }

        QC.ScrollView {
            id: scrollViewId
            implicitWidth: tableView.implicitWidth +
                           QC.ScrollBar.vertical.implicitWidth
            Layout.fillHeight: true

            TableStaticView {
                id: tableView

                property bool blockChanges: false

                Layout.fillHeight: true
                model: LeadsSortFilterProxyModel {
                    //We check if the leadPage is visible to improve preformance where leadModel changes
                    source: leadModel //leadPage.visible ? leadModel : null

                    sortCaseSensitivity: Qt.CaseInsensitive
                    filterString: "*" + searchBox.text + "*"
                    filterKeyColumn: 0 //All columns
                    filterCaseSensitivity: Qt.CaseInsensitive
                    filterRole: LeadModel.LeadDesciption
                }

                //This will populate the HorizontalHeader
                columnModel.children: [
                    TableStaticColumn { id: doneColumnId; columnWidth: 50; text: "Done" },
                    TableStaticColumn { id: gotoColumnId; columnWidth: 50; text: "Goto" },
                    TableStaticColumn { id: nearestColumnId; columnWidth: 75; text: "Nearest"; sortRole: LeadModel.LeadNearestStation },
                    TableStaticColumn { id: sizeColumnId; columnWidth: 75; text: "Size"; sortRole: LeadModel.LeadSizeAsString},
                    TableStaticColumn { id: distanceColumnId; columnWidth: 75; text: "Distance"; sortRole: LeadModel.LeadDistanceToReferanceStation },
                    TableStaticColumn { id: tripColumnId; columnWidth: 120; text: "Trip"; sortRole: LeadModel.LeadTrip },
                    TableStaticColumn { id: descriptionColumnId; columnWidth: 200; text: "Description"; sortRole: LeadModel.LeadDesciption }
                ]

                component RowDelegate : QQ.Item {
                    id: delegateId

                    required property int index
                    required property bool leadCompleted
                    required property double leadDistanceToReferanceStation
                    required property string leadNearestStation
                    required property string leadSizeAsString
                    required property string leadDescription
                    required property QQ.vector3d leadPosition
                    required property string leadTrip

                    implicitHeight: rowLayoutId.height
                    implicitWidth: rowLayoutId.width

                    TableRowBackground {
                        isSelected: false //tableView.currentIndex == rowDelegate.index
                        rowIndex: delegateId.index
                    }

                    RowLayout {
                        id: rowLayoutId
                        spacing: 0

                        QQ.Item {
                            implicitWidth: doneColumnId.columnWidth
                            implicitHeight: checkbox.implicitHeight
                            clip: true
                            QC.CheckBox {
                                id: checkbox
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.horizontalCenter: parent.horizontalCenter

                                enabled: false
                            }
                        }


                        QQ.Item {
                            implicitWidth: gotoColumnId.columnWidth
                            implicitHeight: gotoId.implicitHeight
                            clip: true
                            LinkText {
                                id: gotoId
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Goto"
                                onClicked: {
                                    var index = tableView.model.index(delegateId.index);
                                    var scrap = tableView.model.data(index, LeadModel.LeadScrap);
                                    var leadIndex = tableView.model.data(index, LeadModel.LeadIndexInScrap);

                                    //This is a work around, just passing styleData.value to TurnTableInteraction.centerOn()
                                    //doesn't work, and it centerOn (0, 0, 0)
                                    // var pos = Qt.vector3d(styleData.value.x, styleData.value.y, styleData.value.z )

                                    //Change to the view page, animate to the lead position, and select it
                                    RootData.pageSelectionModel.currentPageAddress = "View"
                                    let viewpage = RootData.pageView.currentPageItem as RenderingView;

                                    viewpage.turnTableInteraction.centerOn(delegateId.leadPosition, true);
                                    viewpage.leadView.select(scrap, leadIndex);
                                }
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: nearestColumnId.columnWidth
                            implicitHeight: nearestText.implicitHeight
                            Text {
                                id: nearestText
                                text: delegateId.leadNearestStation
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: sizeColumnId.columnWidth
                            implicitHeight: leadSize.implicitHeight
                            Text {
                                id: leadSize
                                text: delegateId.leadSizeAsString
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: distanceColumnId.columnWidth
                            implicitHeight: distanceId.implicitHeight
                            Text {
                                id: distanceId
                                text: Math.round(delegateId.leadDistanceToReferanceStation) + " m"
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: tripColumnId.columnWidth
                            implicitHeight: tripId.implicitHeight
                            LinkText {
                                id: tripId
                                text: delegateId.leadTrip
                                onClicked: {
                                    var modelIndex = tableView.model.index(delegateId.index);
                                    var scrap = tableView.model.data(modelIndex, LeadModel.LeadScrap);
                                    linkGenerator.gotoScrap(scrap)
                                }
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: descriptionColumnId.columnWidth
                            implicitHeight: descriptionId.implicitHeight
                            Text {
                                id: descriptionId
                                text: delegateId.leadDescription
                            }
                        }
                    }

                }

                delegate: RowDelegate {}
            }
        }
    }
}

