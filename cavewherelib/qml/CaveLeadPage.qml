pragma ComponentBehavior: Bound


import QtQuick as QQ
import QtQml
// import QtQuick.Controls as QC1
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs as QD
import cavewherelib

StandardPage {
    id: leadPage
    objectName: "leadPage"

    property Cave cave;

    LeadModel {
        id: leadModel
        regionModel: RootData.regionTreeModel
        cave: leadPage.cave
    }

    LeadCSVExporter {
        id: leadExporter
        model: tableView.model
        leadModel: leadModel
        futureManagerToken: RootData.futureManagerModel.token
    }

    QD.FileDialog {
        id: exportLeadDialog

        title: "Export leads to CSV"
        nameFilters: ["CSV files (*.csv)", "All files (*)"]
        fileMode: QD.FileDialog.SaveFile
        currentFolder: RootData.lastDirectory

        onAccepted: {
            RootData.lastDirectory = selectedFile
            leadExporter.exportToFile(selectedFile)
        }
    }

    HelpBox {
        objectName: "noLeadsHelpBox"
        anchors.centerIn: parent
        text: "There's no leads. Add them in during carpeting"
        visible: tableView.count === 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 3

        RowLayout {
            implicitWidth: scrollViewId.implicitWidth

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

            QQ.Item { implicitWidth: 50 }

            QC.Button {
                text: "Export CSV"
                enabled: tableView.count > 0
                onClicked: exportLeadDialog.open()
            }
        }

        LinkGenerator {
            id: linkGenerator
            pageSelectionModel: RootData.pageSelectionModel
        }

        TableStaticColumnModel {
            id: columnModelId
            columnWidth: QtObject {
                id: columnWidth
                property int done
                property int goTo
                property int nearest
                property int size
                property int distance
                property int trip
                property int description
            }

            columnModel: ObjectModel {
                QtObject {
                    property string widthName: "done"
                    property int columnWidth: 50
                    property string text: "Done"
                    property int sortRole: LeadModel.LeadCompleted
                }
                QtObject {
                    property string widthName: "goTo"
                    property int columnWidth: 50
                    property string text: "Goto"
                }
                QtObject {
                    property string widthName: "nearest"
                    property int columnWidth: 75
                    property string text: "Nearest"
                    property int sortRole: LeadModel.LeadNearestStation
                }
                QtObject {
                    property string widthName: "size"
                    property int columnWidth: 75
                    property string text: "Size"
                    property int sortRole: LeadModel.LeadSizeAsString
                }
                QtObject {
                    property string widthName: "distance"
                    property int columnWidth: 75
                    property string text: "Distance"
                    property int sortRole: LeadModel.LeadDistanceToReferanceStation
                }
                QtObject {
                    property string widthName: "trip"
                    property int columnWidth: 120
                    property string text: "Trip"
                    property int sortRole: LeadModel.LeadTrip
                }
                QtObject {
                    property string widthName: "description"
                    property int columnWidth: 200
                    property string text: "Description"
                    property int sortRole: LeadModel.LeadDesciption
                }
            }
        }

        HorizontalHeaderStaticView {
            view: tableView
            Layout.fillWidth: true
            delegate: TableStaticColumn {
                model: tableView.model
            }
        }

        QC.ScrollView {
            id: scrollViewId
            implicitWidth: tableView.implicitWidth +
                           QC.ScrollBar.vertical.implicitWidth
            Layout.fillHeight: true

            TableStaticView {
                id: tableView
                objectName: "leadTableView"

                property bool blockChanges: false

                Layout.fillHeight: true
                model: LeadsSortFilterProxyModel {
                    id: proxyModel

                    //We check if the leadPage is visible to improve preformance where leadModel changes
                    source: leadModel //leadPage.visible ? leadModel : null

                    sortCaseSensitivity: Qt.CaseInsensitive
                    filterString: "*" + searchBox.text + "*"
                    filterKeyColumn: 0 //All columns
                    filterCaseSensitivity: Qt.CaseInsensitive
                    filterRole: LeadModel.LeadDesciption
                }

                //This will populate the HorizontalHeader
                columnModel: columnModelId.columnModel

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
                        anchors.fill: parent
                    }

                    RowLayout {
                        id: rowLayoutId
                        spacing: 0

                        QQ.Item {
                            implicitWidth: columnWidth.done
                            implicitHeight: checkbox.implicitHeight
                            clip: true

                            Icon {
                                id: checkbox
                                source: "qrc:/twbs-icons/icons/check-lg.svg"
                                sourceSize: Qt.size(parent.implicitHeight, parent.implicitHeight)
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: delegateId.leadCompleted
                                colorizationColor: Theme.text
                            }
                        }

                        QQ.Item {
                            implicitWidth: columnWidth.goTo
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
                            implicitWidth: columnWidth.nearest
                            implicitHeight: nearestText.implicitHeight
                            Text {
                                id: nearestText
                                text: delegateId.leadNearestStation
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: columnWidth.size
                            implicitHeight: leadSize.implicitHeight
                            Text {
                                id: leadSize
                                text: delegateId.leadSizeAsString
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: columnWidth.distance
                            implicitHeight: distanceId.implicitHeight
                            Text {
                                id: distanceId
                                text: Math.round(delegateId.leadDistanceToReferanceStation) + " m"
                            }
                        }

                        QQ.Item {
                            clip: true
                            implicitWidth: columnWidth.trip
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
                            implicitWidth: columnWidth.description
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
