pragma ComponentBehavior: Bound


import QtQuick as QQ
import QtQml
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs as QD
import cavewherelib

StandardPage {
    id: leadPage
    objectName: "leadPage"

    property Cave cave
    property string filterText: ""

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    LeadModel {
        id: leadModel
        regionModel: RootData.regionTreeModel
        cave: leadPage.cave
    }

    LeadsSortFilterProxyModel {
        id: proxyModel
        source: leadModel
        sortCaseSensitivity: Qt.CaseInsensitive
        filterString: "*" + leadPage.filterText + "*"
        filterKeyColumn: 0
        filterCaseSensitivity: Qt.CaseInsensitive
        filterRole: LeadModel.LeadDesciption
    }

    LeadCSVExporter {
        id: leadExporter
        model: proxyModel
        leadModel: leadModel
        futureManagerToken: RootData.futureManagerModel.token
    }

    LinkGenerator {
        id: linkGenerator
        pageSelectionModel: RootData.pageSelectionModel
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

    TableStaticColumnModel {
        id: columnModelId
        columns: [
            TableStaticColumn {
                id: doneColumn
                columnWidth: 50
                text: "Done"
                sortRole: LeadModel.LeadCompleted
            },
            TableStaticColumn {
                id: goToColumn
                columnWidth: 50
                text: "Goto"
            },
            TableStaticColumn {
                id: nearestColumn
                columnWidth: 75
                text: "Nearest"
                sortRole: LeadModel.LeadNearestStation
            },
            TableStaticColumn {
                id: sizeColumn
                columnWidth: 75
                text: "Size"
                sortRole: LeadModel.LeadSizeAsString
            },
            TableStaticColumn {
                id: distanceColumn
                columnWidth: 75
                text: "Distance"
                sortRole: LeadModel.LeadDistanceToReferanceStation
            },
            TableStaticColumn {
                id: tripColumn
                columnWidth: 120
                text: "Trip"
                sortRole: LeadModel.LeadTrip
            },
            TableStaticColumn {
                id: descriptionColumn
                columnWidth: 200
                text: "Description"
                sortRole: LeadModel.LeadDesciption
            }
        ]
    }

    function gotoLead(scrap, leadIndex, leadPosition) {
        RootData.pageSelectionModel.currentPageAddress = "View"
        let viewpage = RootData.pageView.currentPageItem as RenderingView;
        viewpage.turnTableInteraction.centerOn(leadPosition, true);
        viewpage.leadView.select(scrap, leadIndex);
    }

    function leadScrapAndIndex(delegateIndex) {
        let modelIndex = proxyModel.index(delegateIndex, 0);
        return {
            scrap: proxyModel.data(modelIndex, LeadModel.LeadScrap),
            leadIndex: proxyModel.data(modelIndex, LeadModel.LeadIndexInScrap)
        };
    }

    QC.TextField {
        id: searchField
        placeholderText: "Filter by description..."
        inputMethodHints: Qt.ImhNoPredictiveText
        text: leadPage.filterText
        onTextEdited: leadPage.filterText = text
    }

    RowLayout {
        id: refStationGroup

        QC.Label { text: "Lead Distance from:" }

        ClickTextInput {
            text: leadModel.referanceStation.length === 0 ? "No Station" : leadModel.referanceStation
            onFinishedEditting: (newText) => {
                leadModel.referanceStation = newText
            }
        }

        InformationButton {
            onClicked: refStationHelpArea.visible = !refStationHelpArea.visible
        }

        HelpArea {
            id: refStationHelpArea
            implicitWidth: 200
            text: "Lead distance from a station, calculates the <b>line of sight</b> distance from the station to all the leads."
        }
    }

    QC.Button {
        id: exportButton
        text: "Export CSV"
        enabled: proxyModel.count > 0
        onClicked: exportLeadDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 3

        RowLayout {
            Layout.fillWidth: true
            visible: !leadPage.isNarrow

            LayoutItemProxy {
                target: leadPage.isNarrow ? null : searchField
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: 150
            }

            QQ.Item { implicitWidth: 50 }

            LayoutItemProxy {
                target: leadPage.isNarrow ? null : refStationGroup
            }

            QQ.Item { implicitWidth: 50 }

            LayoutItemProxy {
                target: leadPage.isNarrow ? null : exportButton
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: leadPage.isNarrow
            spacing: Theme.sectionSpacing

            LayoutItemProxy {
                target: leadPage.isNarrow ? searchField : null
                Layout.fillWidth: true
            }

            LayoutItemProxy {
                target: leadPage.isNarrow ? refStationGroup : null
                Layout.fillWidth: true
            }

            QQ.Flow {
                Layout.fillWidth: true
                spacing: Theme.actionBarSpacing

                RowLayout {
                    spacing: Theme.delegatePadding

                    QC.Label { text: "Sort:" }

                    QC.ComboBox {
                        id: narrowSortComboId
                        model: ["Done", "Nearest", "Size", "Distance", "Trip", "Description"]

                        property list<int> sortRoles: [
                            LeadModel.LeadCompleted,
                            LeadModel.LeadNearestStation,
                            LeadModel.LeadSizeAsString,
                            LeadModel.LeadDistanceToReferanceStation,
                            LeadModel.LeadTrip,
                            LeadModel.LeadDesciption
                        ]

                        onActivated: {
                            proxyModel.sortRole = sortRoles[currentIndex]
                            proxyModel.sort(narrowSortOrderButtonId.ascending ? Qt.AscendingOrder : Qt.DescendingOrder)
                        }
                    }

                    QC.RoundButton {
                        id: narrowSortOrderButtonId
                        property bool ascending: true

                        implicitHeight: narrowSortComboId.height
                        implicitWidth: implicitHeight

                        icon.source: narrowSortOrderButtonId.ascending
                                     ? "qrc:/twbs-icons/icons/sort-up.svg"
                                     : "qrc:/twbs-icons/icons/sort-down.svg"
                        icon.width: Theme.iconSizeButton
                        icon.height: Theme.iconSizeButton
                        icon.color: Theme.text

                        onClicked: {
                            ascending = !ascending
                            proxyModel.sortRole = narrowSortComboId.sortRoles[narrowSortComboId.currentIndex]
                            proxyModel.sort(ascending ? Qt.AscendingOrder : Qt.DescendingOrder)
                        }
                    }
                }

                LayoutItemProxy {
                    target: leadPage.isNarrow ? exportButton : null
                }
            }
        }

        HorizontalHeaderStaticView {
            visible: !leadPage.isNarrow
            view: tableView
            Layout.fillWidth: true
            delegate: TableStaticHeaderColumn {
                model: tableView.model
            }
        }

        QC.ScrollView {
            id: scrollViewId
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitWidth: leadPage.isNarrow
                           ? 0
                           : tableView.implicitWidth + QC.ScrollBar.vertical.implicitWidth

            TableStaticView {
                id: tableView
                objectName: "leadTableView"

                model: proxyModel
                columnModel: columnModelId

                implicitWidth: leadPage.isNarrow ? 0 : columnModelId.totalWidth

                delegate: leadPage.isNarrow ? narrowDelegateComponent : wideDelegateComponent
            }
        }
    }

    HelpBox {
        objectName: "noLeadsHelpBox"
        anchors.centerIn: parent
        text: "There's no leads. Add them in during carpeting"
        visible: proxyModel.count === 0
    }

    QQ.Component {
        id: wideDelegateComponent

        QQ.Item {
            id: wideDelegateId

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
                isSelected: false
                rowIndex: wideDelegateId.index
                anchors.fill: parent
            }

            RowLayout {
                id: rowLayoutId
                spacing: 0

                QQ.Item {
                    implicitWidth: doneColumn.columnWidth
                    implicitHeight: checkbox.implicitHeight
                    clip: true

                    Icon {
                        id: checkbox
                        source: "qrc:/twbs-icons/icons/check-lg.svg"
                        sourceSize: Qt.size(parent.implicitHeight, parent.implicitHeight)
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: wideDelegateId.leadCompleted
                        colorizationColor: Theme.text
                    }
                }

                QQ.Item {
                    implicitWidth: goToColumn.columnWidth
                    implicitHeight: gotoId.implicitHeight
                    clip: true
                    LinkText {
                        id: gotoId
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Goto"
                        onClicked: {
                            let lead = leadPage.leadScrapAndIndex(wideDelegateId.index)
                            leadPage.gotoLead(lead.scrap, lead.leadIndex, wideDelegateId.leadPosition)
                        }
                    }
                }

                QQ.Item {
                    clip: true
                    implicitWidth: nearestColumn.columnWidth
                    implicitHeight: nearestText.implicitHeight
                    QC.Label {
                        id: nearestText
                        text: wideDelegateId.leadNearestStation
                    }
                }

                QQ.Item {
                    clip: true
                    implicitWidth: sizeColumn.columnWidth
                    implicitHeight: leadSize.implicitHeight
                    QC.Label {
                        id: leadSize
                        text: wideDelegateId.leadSizeAsString
                    }
                }

                QQ.Item {
                    clip: true
                    implicitWidth: distanceColumn.columnWidth
                    implicitHeight: distanceId.implicitHeight
                    QC.Label {
                        id: distanceId
                        text: Math.round(wideDelegateId.leadDistanceToReferanceStation) + " m"
                    }
                }

                QQ.Item {
                    clip: true
                    implicitWidth: tripColumn.columnWidth
                    implicitHeight: tripId.implicitHeight
                    LinkText {
                        id: tripId
                        text: wideDelegateId.leadTrip
                        onClicked: linkGenerator.gotoScrap(leadPage.leadScrapAndIndex(wideDelegateId.index).scrap)
                    }
                }

                QQ.Item {
                    clip: true
                    implicitWidth: descriptionColumn.columnWidth
                    implicitHeight: descriptionId.implicitHeight
                    QC.Label {
                        id: descriptionId
                        text: wideDelegateId.leadDescription
                    }
                }
            }
        }
    }

    QQ.Component {
        id: narrowDelegateComponent

        QQ.Item {
            id: narrowDelegateId

            required property int index
            required property bool leadCompleted
            required property double leadDistanceToReferanceStation
            required property string leadNearestStation
            required property string leadSizeAsString
            required property string leadDescription
            required property QQ.vector3d leadPosition
            required property string leadTrip

            width: QQ.ListView.view ? QQ.ListView.view.width : 0
            implicitHeight: flowId.implicitHeight + Theme.delegatePadding * 2

            TableRowBackground {
                isSelected: false
                rowIndex: narrowDelegateId.index
                anchors.fill: parent
            }

            QQ.Flow {
                id: flowId
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.delegatePadding
                anchors.rightMargin: Theme.delegatePadding
                spacing: Theme.flowSpacing

                Icon {
                    source: "qrc:/twbs-icons/icons/check-lg.svg"
                    sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
                    width: Theme.iconSizeButton
                    height: Theme.iconSizeButton
                    visible: narrowDelegateId.leadCompleted
                    colorizationColor: Theme.text
                }

                LinkText {
                    text: "Goto"
                    onClicked: {
                        let lead = leadPage.leadScrapAndIndex(narrowDelegateId.index)
                        leadPage.gotoLead(lead.scrap, lead.leadIndex, narrowDelegateId.leadPosition)
                    }
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                QC.Label {
                    text: narrowDelegateId.leadNearestStation
                    font.bold: true
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                QC.Label {
                    text: narrowDelegateId.leadSizeAsString
                    color: Theme.textSubtle
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                QC.Label {
                    text: Math.round(narrowDelegateId.leadDistanceToReferanceStation) + " m"
                    color: Theme.textSubtle
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                LinkText {
                    text: narrowDelegateId.leadTrip
                    onClicked: linkGenerator.gotoScrap(leadPage.leadScrapAndIndex(narrowDelegateId.index).scrap)
                }

                QC.Label {
                    text: narrowDelegateId.leadDescription
                    wrapMode: QQ.Text.WordWrap
                    width: Math.min(implicitWidth, flowId.width)
                }
            }
        }
    }
}
