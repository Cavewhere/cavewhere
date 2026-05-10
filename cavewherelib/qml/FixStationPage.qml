/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQml
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: fixStationPage
    objectName: "fixStationPage"

    property Cave cave

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse
    readonly property FixStationModel fixStationsModel: cave ? cave.fixStations : null

    function addFix() {
        if (fixStationPage.fixStationsModel) {
            fixStationPage.fixStationsModel.addFixStation()
        }
    }

    function removeFix(index) {
        if (fixStationPage.fixStationsModel) {
            fixStationPage.fixStationsModel.removeAt(index)
        }
    }

    function commitEdit(rowIndex, role, newText, numeric) {
        const v = numeric ? Number(newText) : newText
        fixStationPage.fixStationsModel.setData(
            fixStationPage.fixStationsModel.index(rowIndex), v, role)
    }

    component FixField : DoubleClickTextInput {
        id: field
        property string value
        property int role
        property int rowIndex
        property bool numeric: false

        text: value
        onFinishedEditting: (newText) => fixStationPage.commitEdit(field.rowIndex, field.role, newText, field.numeric)
    }

    component WideCell : QQ.Item {
        id: cell
        property int columnWidth: 0
        property alias value: field.value
        property alias role: field.role
        property alias rowIndex: field.rowIndex
        property alias numeric: field.numeric

        implicitWidth: columnWidth
        implicitHeight: field.implicitHeight
        clip: true

        FixField {
            id: field
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    component CSCell : QQ.Item {
        id: csCell
        property int columnWidth: 0
        property string value
        property int rowIndex

        implicitWidth: columnWidth
        implicitHeight: combo.implicitHeight
        clip: true

        CSComboBox {
            id: combo
            objectName: "inputCSComboBox." + csCell.rowIndex
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            value: csCell.value
            allowGeographic: true
            onCommitted: (newCS) => fixStationPage.commitEdit(
                csCell.rowIndex, FixStationModel.InputCSRole, newCS, false)
        }
    }

    RemoveAskBox {
        id: removeChallengeId
        onRemove: fixStationPage.removeFix(indexToRemove)
    }

    TableStaticColumnModel {
        id: columnModelId
        columns: [
            TableStaticColumn {
                id: stationColumn
                columnWidth: 130
                text: "Station"
            },
            TableStaticColumn {
                id: csColumn
                columnWidth: 140
                text: "Input CS"
            },
            TableStaticColumn {
                id: eastingColumn
                columnWidth: 130
                text: "Easting"
            },
            TableStaticColumn {
                id: northingColumn
                columnWidth: 130
                text: "Northing"
            },
            TableStaticColumn {
                id: elevationColumn
                columnWidth: 110
                text: "Elevation"
            }
        ]
    }

    AddAndSearchBar {
        id: addFixBar
        objectName: "addFixBar"
        addButtonText: "Add Fix"
        onAdd: fixStationPage.addFix()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: Theme.sectionSpacing

        LayoutItemProxy {
            target: addFixBar
            Layout.fillWidth: true
        }

        HorizontalHeaderStaticView {
            visible: !fixStationPage.isNarrow
            view: tableView
            Layout.fillWidth: true
            delegate: TableStaticHeaderColumn {
                model: tableView.model
            }
        }

        QC.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitWidth: fixStationPage.isNarrow
                           ? 0
                           : tableView.implicitWidth + QC.ScrollBar.vertical.implicitWidth

            TableStaticView {
                id: tableView
                objectName: "fixStationTableView"

                model: fixStationPage.fixStationsModel
                columnModel: columnModelId

                implicitWidth: fixStationPage.isNarrow ? 0 : columnModelId.totalWidth

                delegate: fixStationPage.isNarrow ? narrowDelegateComponent : wideDelegateComponent
            }
        }
    }

    HelpBox {
        objectName: "noFixStationsHelpBox"
        anchors.centerIn: parent
        text: "No fix stations yet. Click <b>Add Fix</b> to anchor a station to absolute coordinates."
        visible: fixStationPage.fixStationsModel
                 && fixStationPage.fixStationsModel.count === 0
    }

    QQ.Component {
        id: wideDelegateComponent

        QQ.Item {
            id: wideDelegateId

            required property int index
            required property string stationName
            required property string inputCS
            required property double easting
            required property double northing
            required property double elevation

            implicitHeight: rowLayoutId.implicitHeight + Theme.tightSpacing * 2
            implicitWidth: rowLayoutId.implicitWidth

            TableRowBackground {
                isSelected: tableView.currentIndex === wideDelegateId.index
                rowIndex: wideDelegateId.index
                anchors.fill: parent
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: wideDelegateId.index
                name: wideDelegateId.stationName
            }

            QQ.MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: tableView.currentIndex = wideDelegateId.index
            }

            RowLayout {
                id: rowLayoutId
                spacing: 0
                anchors.verticalCenter: parent.verticalCenter

                WideCell {
                    columnWidth: stationColumn.columnWidth
                    value: wideDelegateId.stationName
                    role: FixStationModel.StationNameRole
                    rowIndex: wideDelegateId.index
                }

                CSCell {
                    columnWidth: csColumn.columnWidth
                    value: wideDelegateId.inputCS
                    rowIndex: wideDelegateId.index
                }

                WideCell {
                    columnWidth: eastingColumn.columnWidth
                    value: wideDelegateId.easting
                    role: FixStationModel.EastingRole
                    rowIndex: wideDelegateId.index
                    numeric: true
                }

                WideCell {
                    columnWidth: northingColumn.columnWidth
                    value: wideDelegateId.northing
                    role: FixStationModel.NorthingRole
                    rowIndex: wideDelegateId.index
                    numeric: true
                }

                WideCell {
                    columnWidth: elevationColumn.columnWidth
                    value: wideDelegateId.elevation
                    role: FixStationModel.ElevationRole
                    rowIndex: wideDelegateId.index
                    numeric: true
                }
            }
        }
    }

    QQ.Component {
        id: narrowDelegateComponent

        QQ.Item {
            id: narrowDelegateId

            required property int index
            required property string stationName
            required property string inputCS
            required property double easting
            required property double northing
            required property double elevation

            width: QQ.ListView.view ? QQ.ListView.view.width : 0
            implicitHeight: narrowFlow.implicitHeight + Theme.delegatePadding * 2

            TableRowBackground {
                isSelected: tableView.currentIndex === narrowDelegateId.index
                rowIndex: narrowDelegateId.index
                anchors.fill: parent
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: narrowDelegateId.index
                name: narrowDelegateId.stationName
            }

            QQ.MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: tableView.currentIndex = narrowDelegateId.index
            }

            QQ.Flow {
                id: narrowFlow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.delegatePadding
                anchors.rightMargin: Theme.delegatePadding
                spacing: Theme.flowSpacing

                FixField {
                    value: narrowDelegateId.stationName
                    role: FixStationModel.StationNameRole
                    rowIndex: narrowDelegateId.index
                    font.bold: true
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                CSComboBox {
                    objectName: "inputCSComboBox." + narrowDelegateId.index
                    value: narrowDelegateId.inputCS
                    allowGeographic: true
                    onCommitted: (newCS) => fixStationPage.commitEdit(
                        narrowDelegateId.index, FixStationModel.InputCSRole, newCS, false)
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                FixField {
                    value: narrowDelegateId.easting
                    role: FixStationModel.EastingRole
                    rowIndex: narrowDelegateId.index
                    numeric: true
                }

                FixField {
                    value: narrowDelegateId.northing
                    role: FixStationModel.NorthingRole
                    rowIndex: narrowDelegateId.index
                    numeric: true
                }

                FixField {
                    value: narrowDelegateId.elevation
                    role: FixStationModel.ElevationRole
                    rowIndex: narrowDelegateId.index
                    numeric: true
                }
            }
        }
    }
}
