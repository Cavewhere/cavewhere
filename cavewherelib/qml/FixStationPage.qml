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

    // Edits go to fixStationsModel; the rows the table shows come from the
    // diagnostics proxy, which layers the read-only warning roles on top. Same
    // rows, same role names — only the derived warnings are extra.
    readonly property FixStationDiagnosticsModel diagnosticsModel: cave ? cave.fixStationDiagnostics : null

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

    function commitEdit(rowIndex, role, newText) {
        fixStationPage.fixStationsModel.setData(
            fixStationPage.fixStationsModel.index(rowIndex), newText, role)
    }

    // Easting, northing and elevation in one write, so a pasted coordinate
    // re-solves the line plot once. Text that won't parse never reaches here:
    // the field's CoordinateTextValidator holds the editor open and shows the
    // reason, in the row's own axis order.
    //
    // The axis order is re-derived from the row's own CS rather than passed in,
    // so it can't drift from the one the cell was rendered with — a mismatch
    // would silently transpose a lat/long.
    function commitCoordinate(rowIndex, newText) {
        const model = fixStationPage.fixStationsModel
        const rowIdx = model.index(rowIndex)
        model.setCoordinateText(
            rowIndex, newText, ProjectUnits.unitSystem,
            CoordinateText.axisOrderFor(model.data(rowIdx, FixStationModel.InputCSRole)))
    }

    component FixField : DoubleClickTextInput {
        id: field
        property string value
        property int role
        property int rowIndex
        // Tints the value red when the row's coordinate falls outside the input
        // CS's valid domain (U4), or when the station name matches nothing.
        property bool error: false
        //! When set, the field carries the whole coordinate rather than one
        //! role: it commits through setCoordinateText() and is validated as a
        //! unit. See cwCoordinateText for the formats it takes.
        property bool coordinate: false
        //! Which axis the coordinate leads with, from this row's own CS. It
        //! reaches the validator because a refusal has to name the axes the row
        //! actually wants — the verdict is order-independent, the message isn't.
        property int axisOrder: CoordinateText.EastingNorthing

        text: value
        color: field.error ? Theme.errorText : Theme.text
        validator: field.coordinate ? coordinateValidatorId : null

        onFinishedEditting: (newText) => {
            if (field.coordinate) {
                fixStationPage.commitCoordinate(field.rowIndex, newText)
            } else {
                fixStationPage.commitEdit(field.rowIndex, field.role, newText)
            }
        }

        // One per field rather than one per page: the axis order it explains
        // itself in belongs to the row, and two rows can disagree.
        CoordinateTextValidator {
            id: coordinateValidatorId
            axisOrder: field.axisOrder
        }
    }

    component WideCell : QQ.Item {
        id: cell
        property int columnWidth: 0
        property alias value: field.value
        property alias role: field.role
        property alias rowIndex: field.rowIndex
        property alias error: field.error
        property alias coordinate: field.coordinate
        property alias axisOrder: field.axisOrder
        //! Names the inner editable field rather than this wrapper, so callers
        //! reach the same item the narrow layout exposes directly.
        property alias fieldObjectName: field.objectName

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

    // Inline per-row warning icon with a hover tooltip, driven by a message
    // string (empty ⇒ hidden and zero-width). Shared by the coordinate-domain
    // flag (DomainErrorRole) and the station-reference flag (StationErrorRole),
    // so the two read the same but carry distinct messages.
    component InlineWarning : QQ.Item {
        id: inlineWarning
        property string message: ""

        visible: inlineWarning.message !== ""
        implicitWidth: visible ? Theme.iconSizeButton : 0
        implicitHeight: Theme.iconSizeButton

        QQ.Image {
            anchors.centerIn: parent
            source: "qrc:icons/svg/warning.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
        }

        QQ.HoverHandler {
            id: warningHover
        }

        QC.ToolTip {
            visible: warningHover.hovered && inlineWarning.message !== ""
            text: inlineWarning.message
            delay: 300
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
                csCell.rowIndex, FixStationModel.InputCSRole, newCS)
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
                // Wide enough for UTM mode's mode combo + zone + hemisphere
                // controls (mode combo sizes to its widest entry, "Lat/Lon
                // (WGS84)") so the hemisphere combo isn't clipped by the cell.
                columnWidth: 300
                text: "Input CS"
            },
            TableStaticColumn {
                id: coordinateColumn
                // Wide enough for a UTM triple with its elevation unit, e.g.
                // "610016.792, 5615117.075, 2545.34m".
                columnWidth: 300
                // Both orders are named because the order is per row, not per
                // column: a geographic CS writes latitude first, a projected one
                // easting first. The headers this column replaced ("Easting /
                // Long", "Northing / Lat") were the only place that was said.
                text: "Coordinate (East, North / Lat, Long)"
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

        // Entering fixes here without a project output CS leaves the caves
        // unplaceable; prompt for one right where the fixes are typed.
        OutputCSPrompt {
            objectName: "outputCSPrompt"
            Layout.fillWidth: true
            visible: RootData.region.fixStationValidator.needsOutputCS
            suggestedCS: RootData.region.fixStationValidator.suggestedOutputCS
            coordinateInvalid: RootData.region.fixStationValidator.outputCSCoordinateInvalid
            onUseSuggested: (cs) => RootData.region.geoReference.globalCoordinateSystem = cs
        }

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

                model: fixStationPage.diagnosticsModel
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
            required property string domainError
            required property bool eastingDomainError
            required property bool northingDomainError
            required property string stationError

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
                    fieldObjectName: "stationCell." + wideDelegateId.index
                    columnWidth: stationColumn.columnWidth
                    value: wideDelegateId.stationName
                    role: FixStationModel.StationNameRole
                    rowIndex: wideDelegateId.index
                    error: wideDelegateId.stationError !== ""
                }

                CSCell {
                    columnWidth: csColumn.columnWidth
                    value: wideDelegateId.inputCS
                    rowIndex: wideDelegateId.index
                }

                WideCell {
                    fieldObjectName: "coordinateCell." + wideDelegateId.index
                    columnWidth: coordinateColumn.columnWidth
                    // Reading inputCS here is what makes the cell re-render
                    // when the CS flips between geographic and projected — the
                    // two write their axes in opposite orders.
                    value: CoordinateText.format(wideDelegateId.easting,
                                                 wideDelegateId.northing,
                                                 wideDelegateId.elevation,
                                                 ProjectUnits.unitSystem,
                                                 CoordinateText.axisOrderFor(wideDelegateId.inputCS))
                    rowIndex: wideDelegateId.index
                    coordinate: true
                    axisOrder: CoordinateText.axisOrderFor(wideDelegateId.inputCS)
                    // One field now holds both horizontal components, so the
                    // two per-axis domain flags share the one tint.
                    error: wideDelegateId.eastingDomainError
                           || wideDelegateId.northingDomainError
                }

                InlineWarning {
                    objectName: "stationWarning." + wideDelegateId.index
                    Layout.leftMargin: Theme.tightSpacing
                    Layout.alignment: Qt.AlignVCenter
                    message: wideDelegateId.stationError
                }

                InlineWarning {
                    objectName: "domainWarning." + wideDelegateId.index
                    Layout.leftMargin: Theme.tightSpacing
                    Layout.alignment: Qt.AlignVCenter
                    message: wideDelegateId.domainError
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
            required property string domainError
            required property bool eastingDomainError
            required property bool northingDomainError
            required property string stationError

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

                InlineWarning {
                    objectName: "stationWarning." + narrowDelegateId.index
                    message: narrowDelegateId.stationError
                }

                InlineWarning {
                    objectName: "domainWarning." + narrowDelegateId.index
                    message: narrowDelegateId.domainError
                }

                FixField {
                    objectName: "stationCell." + narrowDelegateId.index
                    value: narrowDelegateId.stationName
                    role: FixStationModel.StationNameRole
                    rowIndex: narrowDelegateId.index
                    font.bold: true
                    error: narrowDelegateId.stationError !== ""
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                CSComboBox {
                    objectName: "inputCSComboBox." + narrowDelegateId.index
                    value: narrowDelegateId.inputCS
                    allowGeographic: true
                    onCommitted: (newCS) => fixStationPage.commitEdit(
                        narrowDelegateId.index, FixStationModel.InputCSRole, newCS)
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                FixField {
                    objectName: "coordinateCell." + narrowDelegateId.index
                    value: CoordinateText.format(narrowDelegateId.easting,
                                                 narrowDelegateId.northing,
                                                 narrowDelegateId.elevation,
                                                 ProjectUnits.unitSystem,
                                                 CoordinateText.axisOrderFor(narrowDelegateId.inputCS))
                    rowIndex: narrowDelegateId.index
                    coordinate: true
                    axisOrder: CoordinateText.axisOrderFor(narrowDelegateId.inputCS)
                    error: narrowDelegateId.eastingDomainError
                           || narrowDelegateId.northingDomainError
                }
            }
        }
    }
}
