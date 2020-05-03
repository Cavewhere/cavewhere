import QtQuick 2.0 as QQ
import QtQml 2.2
import QtQuick.Controls 1.2 as QC1
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.1
import Cavewhere 1.0

StandardPage {
    id: leadPage

    property Cave cave;

    LeadModel {
        id: leadModel
        regionModel: rootData.regionTreeModel
        cave: leadPage.cave
    }

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            QC.TextField {
                id: searchBox

                placeholderText: "Filter..."
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
                onFinishedEditting: {
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
            pageSelectionModel: rootData.pageSelectionModel
        }

        QC1.TableView {
            id: tableView

            property bool blockChanges: false

            Layout.fillHeight: true
            Layout.fillWidth: true

            sortIndicatorVisible: true
            sortIndicatorColumn: 1


            implicitWidth: 400
            model: LeadsSortFilterProxyModel {
                //We check if the leadPage is visible to improve preformance where leadModel changes
                source: leadPage.visible ? leadModel : null

                sortOrder: tableView.sortIndicatorOrder
                sortCaseSensitivity: Qt.CaseInsensitive
                sortRole: tableView.getColumn(tableView.sortIndicatorColumn).role

                filterString: "*" + searchBox.text + "*"
                filterSyntax: SortFilterProxyModel.Wildcard
                filterCaseSensitivity: Qt.CaseInsensitive
                filterRole: ""
            }

            QC1.TableViewColumn { role: "leadCompleted"; title: "Completed"; width: 60 }
            QC1.TableViewColumn { role: "leadPosition"; title: "Goto"; width: 40 }
            QC1.TableViewColumn { role: "leadNearestStation"; title: "Station"; width: 65}
            QC1.TableViewColumn { role: "leadSizeAsString"; title: "Size"; width: 50 }
            QC1.TableViewColumn {
                role: "leadDistanceToReferanceStation";
                title: "Distance to " + leadModel.referanceStation;
                width: 100
            }
            QC1.TableViewColumn { role: "leadTrip"; title: "Trip"; width: 40 }
            QC1.TableViewColumn { role: "leadDescription"; title: "Description"; width: 400 }

            section.property: "leadCompleted"
            section.delegate: QQ.Rectangle {
                width: tableView.width
                height: childrenRect.height

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    text: section == "true" ? "Completed Leads" : "Leads"
                    font.bold: true
                }
            }

            rowDelegate: QQ.Rectangle {
                height: 30
                color: styleData.row % 2 ? "white" : "#f2f2f2"
            }

            itemDelegate: QQ.Item {
                QQ.Loader {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    clip: true
                    sourceComponent: {
                        switch(tableView.getColumn(styleData.column).role) {
                        case "leadCompleted":
                            return checkboxComponent;
                        case "leadNearestStation":
                        case "leadSizeAsString":
                        case "leadDescription":
                            return textComponent
                        case "leadPosition":
                            return gotoViewComponent
                        case "leadTrip":
                            return noteViewComponent
                        case "leadDistanceToReferanceStation":
                            return lengthComponent
                        }
                    }
                }

                QQ.Component {
                    id: checkboxComponent
                    QQ.Item {
                        height: 30
                        width: 40
                        CheckBox {
                            id: checkbox
                            anchors.verticalCenter: parent.verticalCenter

                            Binding {
                                target: checkbox
                                property: "checked"
                                value: styleData.value
                            }

                            onCheckedChanged: {
                                if(styleData.value !== "") {
                                    var index = tableView.model.index(styleData.row);
                                    tableView.model.setData(index, checked, LeadModel.LeadCompleted);
                                }

                            }
                        }
                    }
                }

                QQ.Component {
                    id: lengthComponent
                    Text {
                        text: Math.round(styleData.value) + " m"
                        x: 3
                    }
                }

                QQ.Component {
                    id: textComponent
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: styleData.value
                        x: 3
                    }
                }

                QQ.Component {
                    id: gotoViewComponent
                    LinkText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Goto"
                        onClicked: {
                            var index = tableView.model.index(styleData.row);
                            var scrap = tableView.model.data(index, LeadModel.LeadScrap);
                            var leadIndex = tableView.model.data(index, LeadModel.LeadIndexInScrap);

                            //This is a work around, just passing styleData.value to TurnTableInteraction.centerOn()
                            //doesn't work, and it centerOn (0, 0, 0)
                            var pos = Qt.vector3d(styleData.value.x, styleData.value.y, styleData.value.z )

                            //Change to the view page, animate to the lead position, and select it
                            rootData.pageSelectionModel.currentPageAddress = "View"
                            pageView.currentPageItem.turnTableInteraction.centerOn(pos, true);
                            pageView.currentPageItem.leadView.select(scrap, leadIndex);
                        }
                    }
                }

                QQ.Component {
                    id: noteViewComponent
                    LinkText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: styleData.value
                        onClicked: {
                            var index = tableView.model.index(styleData.row);
                            var scrap = tableView.model.data(index, LeadModel.LeadScrap);
                            linkGenerator.gotoScrap(scrap)
                        }
                    }
                }
            }
        }
    }
}

