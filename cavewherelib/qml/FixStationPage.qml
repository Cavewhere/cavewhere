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

    // The wide layout is a fixed-width table and nothing scrolls it sideways, so
    // it may only stay up while the whole row fits: every column, the two
    // trailing warning icons with their gaps, the page's margins, and room for
    // the vertical scrollbar.
    readonly property real wideMinimumWidth: columnModelId.totalWidth
                                             + 2 * (Theme.iconSizeButton + Theme.tightSpacing)
                                             + 2 * Theme.pageMargin
                                             + Theme.scrollBarAllowance

    // Measured against the page's own width, never the table's content width, so
    // the fit answer stays a one-way read. The floor keeps a genuinely narrow
    // host on the narrow layout even if the column set shrinks further someday.
    readonly property bool isNarrow: width < Math.max(Theme.breakpointPanelCollapse,
                                                      fixStationPage.wideMinimumWidth)
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

    // The whole coordinate in one write, so a pasted one re-solves the line plot
    // once. Text that won't parse never reaches here: the field's
    // CoordinateTextValidator holds the editor open and shows the reason, in the
    // row's own axis order.
    function commitCoordinate(rowIndex, newText) {
        fixStationPage.fixStationsModel.setCoordinateText(
            rowIndex, newText, ProjectUnits.unitSystem)
    }

    // Re-stores text the row already held, rather than text someone typed — so
    // it is read back in the units it was stored under, not the ones on display.
    // A stored elevation may be bare (the load path takes a hand-edited string
    // as-is), and cwFixStation reads a bare one as meters; committing it in an
    // imperial project would re-read the same digits as feet and move the fix
    // vertically, which is not something the user asked for by exchanging two
    // horizontal numbers.
    function commitStoredCoordinate(rowIndex: int, newText: string): void {
        fixStationPage.fixStationsModel.setCoordinateText(rowIndex, newText, Units.Metric)
    }

    // Hands row \a rowIndex to the 3D view to be placed by clicking the terrain,
    // the way the Map page's Add Layer hands its selection to the view.
    function pickFromView(rowIndex: int): void {
        FixStationPick.begin(fixStationPage.cave, rowIndex)
    }

    // Both coordinate-system cells commit through here so the transposition
    // question is asked once rather than per layout. `orderWasUnknown` and
    // `coordinateText` are the row as it stood *before* this write — the write
    // is what gives a system-less row an axis order, and afterwards nothing
    // remembers it never had one.
    function commitCS(rowIndex: int, newCS: string, orderWasUnknown: bool,
                      coordinateText: string): void {
        fixStationPage.commitEdit(rowIndex, FixStationModel.InputCSRole, newCS)
        coordinateOrderAskId.askAbout(rowIndex, coordinateText, newCS, orderWasUnknown)
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
        //! The coordinate as it was written, when the row has one. Empty ⇒ the
        //! editor opens on what the cell displays, which is what every field
        //! other than the coordinate does.
        property string editValue: ""

        text: value
        //! A coordinate cell now shows the row's own text when that text can't
        //! be read (see the value bindings below), and such text is by
        //! definition unvalidated — it reached the project by hand. AutoText
        //! would render "<b>610016" as markup and show the user something other
        //! than what is stored.
        textFormat: QC.Label.PlainText
        //! Display and edit are deliberately different for a coordinate: the
        //! column renders every row in the project's units so it can be scanned,
        //! while an edit starts from what the user wrote.
        editText: field.editValue !== "" ? field.editValue : field.text
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
        //! An item the cell parks at its right edge, declared as a child of the
        //! cell; the field gives up the room it takes. The coordinate column
        //! uses it so the "Coordinate" header owns the crosshair that fills it.
        property QQ.Item trailingItem: null
        property alias value: field.value
        property alias role: field.role
        property alias rowIndex: field.rowIndex
        property alias error: field.error
        property alias coordinate: field.coordinate
        property alias axisOrder: field.axisOrder
        property alias editValue: field.editValue
        //! Names the inner editable field rather than this wrapper, so callers
        //! reach the same item the narrow layout exposes directly.
        property alias fieldObjectName: field.objectName

        implicitWidth: columnWidth
        implicitHeight: Math.max(field.implicitHeight,
                                 cell.trailingItem ? cell.trailingItem.implicitHeight : 0)
        clip: true

        FixField {
            id: field
            anchors.left: parent.left
            anchors.right: cell.trailingItem ? cell.trailingItem.left : parent.right
            anchors.rightMargin: cell.trailingItem ? Theme.tightSpacing : 0
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // Inline per-row warning icon with a hover tooltip, driven by a message
    // string (empty ⇒ hidden and zero-width). One of these carries whatever is
    // wrong with the row's coordinate — it can't be read at all
    // (CoordinateErrorRole) or it falls outside its own CS (DomainErrorRole),
    // never both — and another carries the station reference (StationErrorRole).
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

            // Every message this carries quotes something the user supplied — a
            // station name, the unit token the parser refused — and the default
            // content item reads its text as AutoText, so a name like A<b>B
            // becomes a tag. Same reason the coordinate cell declares PlainText.
            contentItem: QC.Label {
                text: inlineWarning.message
                textFormat: QC.Label.PlainText
            }
        }
    }

    component CSCell : QQ.Item {
        id: csCell
        property int columnWidth: 0
        property string value
        property int rowIndex
        //! The row's own coordinate and whether anything records which axis it
        //! leads with — see fixStationPage.commitCS().
        property string coordinateText: ""
        property bool orderUnknown: false
        //! What the row's datum combo may offer, and whether it may be used at
        //! all — see CSPicker.availableDatums / datumEnabled.
        property list<string> availableDatums: []
        property bool datumEnabled: false

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
            availableDatums: csCell.availableDatums
            datumEnabled: csCell.datumEnabled
            onCommitted: (newCS) => fixStationPage.commitCS(
                csCell.rowIndex, newCS, csCell.orderUnknown, csCell.coordinateText)
        }
    }

    RemoveAskBox {
        id: removeChallengeId
        onRemove: fixStationPage.removeFix(indexToRemove)
    }

    // Page-level, not per row: a table delegate is destroyed as its row scrolls
    // out of view, which would take the open question down with it. The row it
    // is asking about therefore travels with the question.
    CoordinateOrderAskBox {
        id: coordinateOrderAskId

        onSwapRequested: (row, swappedCoordinate) => fixStationPage.commitStoredCoordinate(
            row, swappedCoordinate)
    }

    // The coordinate column carries the crosshair, so it has to know how much
    // room the crosshair takes. A tool button's footprint is its icon plus
    // whatever padding the style puts around it, so the column asks a button
    // rather than guessing. Invisible: it exists to be measured.
    PickFromViewButton {
        id: pickButtonMetricsId
        visible: false
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
                // Wide enough for UTM mode's four controls — mode combo (sized
                // to its widest entry, "Custom...") + zone + hemisphere + datum
                // — so none of them is clipped by the cell.
                columnWidth: 400
                text: "Input CS"
            },
            TableStaticColumn {
                id: coordinateColumn
                // Wide enough for a UTM triple with its elevation unit, e.g.
                // "610016.792, 5615117.075, 2545.34m", plus the footprint of
                // the pick-from-view crosshair that shares the column with it.
                columnWidth: 300 + pickButtonMetricsId.implicitWidth + Theme.tightSpacing
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
            required property string coordinateText
            required property string domainError
            required property bool eastingDomainError
            required property bool northingDomainError
            required property string coordinateError
            required property bool coordinateOrderUnknown
            required property string stationError
            required property list<string> availableDatums

            //! Whether the datum may be changed: there has to be a coordinate
            //! for a datum to say anything about, and it has to read as one.
            readonly property bool hasReadableCoordinate:
                wideDelegateId.coordinateText.trim() !== ""
                && wideDelegateId.coordinateError === ""

            // The two coordinate complaints can't both speak: the domain check
            // judges a coordinate the row has, and this one says there isn't one
            // to judge. So they share the single warning beside the row rather
            // than competing for space that only ever holds one of them.
            readonly property string coordinateWarning: wideDelegateId.coordinateError !== ""
                                                        ? wideDelegateId.coordinateError
                                                        : wideDelegateId.domainError

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
                    coordinateText: wideDelegateId.coordinateText
                    orderUnknown: wideDelegateId.coordinateOrderUnknown
                    availableDatums: wideDelegateId.availableDatums
                    datumEnabled: wideDelegateId.hasReadableCoordinate
                }

                WideCell {
                    fieldObjectName: "coordinateCell." + wideDelegateId.index
                    columnWidth: coordinateColumn.columnWidth
                    coordinate: true
                    trailingItem: pickButtonId
                    // Normally the cell renders the numbers, so it needs the
                    // same axis order they were read under. A CS flip swaps the
                    // numbers and the render order together, so what this
                    // displays stays put — it is the meaning underneath that
                    // moved.
                    //
                    // A row with a coordinate error has no numbers: it reports
                    // three zeros, and rendering those would hide the very text
                    // that is wrong with it behind a coordinate at the origin.
                    // So it shows what was written, which is all such a row has.
                    value: wideDelegateId.coordinateError !== ""
                           ? wideDelegateId.coordinateText
                           : CoordinateText.format(wideDelegateId.easting,
                                                   wideDelegateId.northing,
                                                   wideDelegateId.elevation,
                                                   ProjectUnits.unitSystem,
                                                   CoordinateText.axisOrderFor(wideDelegateId.inputCS))
                    // The cell above renders the numbers in the project's
                    // units; the editor re-offers the coordinate as written.
                    editValue: wideDelegateId.coordinateText
                    rowIndex: wideDelegateId.index
                    axisOrder: CoordinateText.axisOrderFor(wideDelegateId.inputCS)
                    // One field now holds both horizontal components, so the
                    // two per-axis domain flags share the one tint — and so does
                    // a coordinate that couldn't be read at all.
                    error: wideDelegateId.coordinateError !== ""
                           || wideDelegateId.eastingDomainError
                           || wideDelegateId.northingDomainError

                    PickFromViewButton {
                        id: pickButtonId
                        objectName: "pickFromViewButton." + wideDelegateId.index
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: fixStationPage.pickFromView(wideDelegateId.index)
                    }
                }

                InlineWarning {
                    objectName: "stationWarning." + wideDelegateId.index
                    Layout.leftMargin: Theme.tightSpacing
                    Layout.alignment: Qt.AlignVCenter
                    message: wideDelegateId.stationError
                }

                InlineWarning {
                    objectName: "coordinateWarning." + wideDelegateId.index
                    Layout.leftMargin: Theme.tightSpacing
                    Layout.alignment: Qt.AlignVCenter
                    message: wideDelegateId.coordinateWarning
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
            required property string coordinateText
            required property string domainError
            required property bool eastingDomainError
            required property bool northingDomainError
            required property string coordinateError
            required property bool coordinateOrderUnknown
            required property string stationError
            required property list<string> availableDatums

            //! See the wide delegate.
            readonly property bool hasReadableCoordinate:
                narrowDelegateId.coordinateText.trim() !== ""
                && narrowDelegateId.coordinateError === ""

            //! Mutually exclusive with the domain error — see the wide delegate.
            readonly property string coordinateWarning: narrowDelegateId.coordinateError !== ""
                                                        ? narrowDelegateId.coordinateError
                                                        : narrowDelegateId.domainError

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
                objectName: "narrowFixRow." + narrowDelegateId.index
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
                    objectName: "coordinateWarning." + narrowDelegateId.index
                    message: narrowDelegateId.coordinateWarning
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
                    availableDatums: narrowDelegateId.availableDatums
                    datumEnabled: narrowDelegateId.hasReadableCoordinate
                    onCommitted: (newCS) => fixStationPage.commitCS(
                        narrowDelegateId.index, newCS,
                        narrowDelegateId.coordinateOrderUnknown,
                        narrowDelegateId.coordinateText)
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                FixField {
                    objectName: "coordinateCell." + narrowDelegateId.index
                    //! The text itself when there are no numbers to render —
                    //! see the wide delegate.
                    value: narrowDelegateId.coordinateError !== ""
                           ? narrowDelegateId.coordinateText
                           : CoordinateText.format(narrowDelegateId.easting,
                                                   narrowDelegateId.northing,
                                                   narrowDelegateId.elevation,
                                                   ProjectUnits.unitSystem,
                                                   CoordinateText.axisOrderFor(narrowDelegateId.inputCS))
                    editValue: narrowDelegateId.coordinateText
                    rowIndex: narrowDelegateId.index
                    coordinate: true
                    axisOrder: CoordinateText.axisOrderFor(narrowDelegateId.inputCS)
                    error: narrowDelegateId.coordinateError !== ""
                           || narrowDelegateId.eastingDomainError
                           || narrowDelegateId.northingDomainError
                }

                PickFromViewButton {
                    objectName: "pickFromViewButton." + narrowDelegateId.index
                    onClicked: fixStationPage.pickFromView(narrowDelegateId.index)
                }
            }
        }
    }
}
