/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QC.Popup {
    id: root

    required property MeasurementInteraction interaction

    readonly property int _lengthPrecision: 2
    readonly property int _anglePrecision: 1
    readonly property int _narrowWidth: 480

    // Auto-collapse to a distance-only chip when the host view is narrow; the
    // header button overrides (assigning here breaks this binding, as intended).
    property bool collapsed: root.parent ? root.parent.width < root._narrowWidth : false

    // Lengths render in the user-selected unit (shared with the clipboard via
    // cwMeasurementInteraction). Reading the lengthUnit.name property re-evaluates
    // these bindings when the unit selector changes; fromMeters() is a plain
    // invokable QML can't track, but it re-runs on that re-evaluation.
    function _length(meters) {
        let value = root.interaction.lengthUnit.fromMeters(meters)
        return qsTr("%1 %2").arg(Number(value).toFixed(root._lengthPrecision))
                            .arg(root.interaction.lengthUnit.name)
    }

    function _angle(degrees) {
        return qsTr("%1°").arg(Number(degrees).toFixed(root._anglePrecision))
    }

    // Signed length for the by-axis components, where the sign carries the
    // direction (east/west, north/south, up/down). Positive gets an explicit
    // '+'; a value that rounds to zero shows no sign — parseFloat collapses a
    // "-0.00" round-down to -0, which toFixed then renders as a clean "0.00".
    function _signedLength(meters) {
        let value = root.interaction.lengthUnit.fromMeters(meters)
        let rounded = parseFloat(Number(value).toFixed(root._lengthPrecision))
        return (rounded > 0 ? "+" : "")
               + qsTr("%1 %2").arg(Number(rounded).toFixed(root._lengthPrecision))
                              .arg(root.interaction.lengthUnit.name)
    }

    // The azimuth value's tooltip: the reason when n/a, otherwise the
    // east-positive corrections folded into the displayed bearing (so the value
    // is reproducible). Grid needs none, so its detail is empty.
    function _azimuthDetail() {
        if (!root.interaction.referenceAvailable) {
            return root.interaction.referenceReason
        }
        if (root.interaction.azimuthReference === AzimuthReference.Grid) {
            return ""
        }
        let parts = [qsTr("Convergence %1°").arg(Number(root.interaction.convergence).toFixed(root._anglePrecision))]
        if (root.interaction.azimuthReference === AzimuthReference.Magnetic) {
            parts.push(qsTr("Declination %1°").arg(Number(root.interaction.declination).toFixed(root._anglePrecision)))
        }
        return parts.join(qsTr(" · "))
    }

    // Read-only TextInput (not a Label, not a TextField) so the value can be
    // selected and copied with the mouse and reads as flat text, not an input.
    // A QC.TextField would need a background override, which the native style
    // forbids; TextInput has none — matching the app's SelectableCaveStat. Shared
    // by ReadoutRow and the azimuth row so every readout value is copyable.
    component ReadoutValue: QQ.TextInput {
        id: valueId

        // Hover tooltip text; empty means no tooltip (the plain rows).
        property string detailText: ""

        // A little inner padding plus a content margin so the right-aligned text
        // is never sized flush to its glyphs: on a HiDPI display, a field exactly
        // as wide as contentWidth loses its leading digit to sub-pixel rounding
        // (14.2 -> 4.2). The slack absorbs that rounding.
        readonly property int _valuePadding: 4
        readonly property int _valueClipMargin: 6

        // TextInput derives its own (read-only) implicitWidth from content and
        // padding; size the layout slot to that plus the clip margin instead.
        readonly property real _slotWidth: valueId.contentWidth + valueId.leftPadding
                                           + valueId.rightPadding + valueId._valueClipMargin

        readOnly: true
        selectByMouse: true
        font.family: Theme.fontFamilyMono
        color: Theme.text
        horizontalAlignment: QQ.TextInput.AlignRight
        topPadding: 0
        bottomPadding: 0
        leftPadding: valueId._valuePadding
        rightPadding: valueId._valuePadding
        // A fillWidth sibling (the label or a spacer) would otherwise shrink this
        // field below its text and clip the leading digit. Pin both the preferred
        // and the minimum width to the content-fitting slot.
        Layout.preferredWidth: valueId._slotWidth
        Layout.minimumWidth: valueId._slotWidth

        QQ.HoverHandler { id: valueHover }

        QC.ToolTip.visible: valueHover.hovered && valueId.detailText.length > 0
        QC.ToolTip.text: valueId.detailText
    }

    component ReadoutRow: RowLayout {
        id: rowId
        required property string label
        required property string value
        property string valueName: ""

        Layout.fillWidth: true
        spacing: 8

        QC.Label {
            text: rowId.label
            color: Theme.textSecondary
            Layout.fillWidth: true
        }

        ReadoutValue {
            objectName: rowId.valueName
            text: rowId.value
        }
    }

    padding: 12
    // Collapsed, it behaves like a dismissable chip: pressing outside the 3D
    // view closes it. (Outside the view, not outside the popup — a press on the
    // terrain is how points get placed, so that must not dismiss it.) Expanded,
    // it stays put as a persistent panel.
    closePolicy: root.collapsed ? QC.Popup.CloseOnPressOutsideParent
                                : QC.Popup.NoAutoClose
    modal: false

    background: QQ.Rectangle {
        color: Theme.surfaceRaised
        border.color: Theme.border
        border.width: 1
        radius: Theme.floatingWidgetRadius
    }

    // The detailed azimuth reference (PROJ grid convergence + IGRF) only resolves
    // while this panel is expanded and on screen, so the per-hover live preview
    // and the collapsed distance chip stay cheap.
    QQ.Binding {
        target: root.interaction
        property: "calculateDetails"
        value: root.visible && !root.collapsed
        restoreMode: QQ.Binding.RestoreNone
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            QC.Label {
                text: qsTr("Measurement")
                color: Theme.text
                font.bold: true
                Layout.fillWidth: true
            }

            // One length unit for every length readout (and the clipboard). Hidden
            // when collapsed to keep the distance chip minimal.
            UnitCombo {
                objectName: "measurementLengthUnitCombo"
                visible: !root.collapsed
                unitModel: root.interaction.lengthUnit.names
                unit: root.interaction.lengthUnit.index
                onNewUnit: (unit) => { root.interaction.lengthUnit.index = unit }
            }

            QC.ToolButton {
                objectName: "measurementCollapseButton"
                text: root.collapsed ? qsTr("Expand") : qsTr("Collapse")
                onClicked: root.collapsed = !root.collapsed
            }
        }

        // Collapsed: distance only.
        ReadoutRow {
            visible: root.collapsed
            label: qsTr("Distance")
            value: root._length(root.interaction.distance)
        }

        // Expanded: the full readout plus mode toggle and copy.
        ColumnLayout {
            visible: !root.collapsed
            Layout.fillWidth: true
            spacing: 8

            // Distance — the straight-line span vs its map-plane projection.
            // Both are unsigned magnitudes; adjacency teaches 3D = 2D + vertical.
            QC.GroupBox {
                title: qsTr("Distance")
                Layout.fillWidth: true

                ColumnLayout {
                    width: parent.width
                    spacing: 4

                    ReadoutRow { label: qsTr("Straight-line (3D)"); value: root._length(root.interaction.distance); valueName: "measurementDistanceValue" }
                    ReadoutRow { label: qsTr("Horizontal (2D)"); value: root._length(root.interaction.horizontal); valueName: "measurementHorizontalValue" }
                }
            }

            // Direction — the bearing (with its north-reference selector) and dip.
            QC.GroupBox {
                title: qsTr("Direction")
                Layout.fillWidth: true

                ColumnLayout {
                    width: parent.width
                    spacing: 4

                    // Azimuth gets a north-reference selector in place of a fixed label:
                    // Grid (the map) / True (the globe) / Magnetic (your compass). True
                    // and Magnetic need a coordinate system, so they disable — and the
                    // value reads n/a — on a local-only or invalid-CRS project.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        // "Azimuth" stays flush-left with the other row labels; the "?"
                        // sits after the selector it explains rather than in a left gutter.
                        QC.Label {
                            text: qsTr("Azimuth")
                            color: Theme.textSecondary
                        }

                        QC.ComboBox {
                            id: azimuthReferenceCombo
                            objectName: "azimuthReferenceCombo"
                            Layout.preferredWidth: implicitWidth

                            // Rows are in enum order (Grid/True/Magnetic), so a row index
                            // is its own AzimuthReference value. Driven by the property,
                            // not the reverse, so a C++ coerce back to Grid (no CRS) shows.
                            model: [qsTr("Grid"), qsTr("True"), qsTr("Magnetic (today)")]
                            currentIndex: root.interaction.azimuthReference

                            onActivated: (index) => { root.interaction.azimuthReference = index }

                            delegate: QC.ItemDelegate {
                                id: azimuthItemId
                                required property string modelData
                                required property int index

                                width: azimuthReferenceCombo.width
                                text: azimuthItemId.modelData
                                // Row 0 (Grid) is always available; True/Magnetic need a CS.
                                enabled: azimuthItemId.index === 0 || root.interaction.geoReferenced
                                highlighted: azimuthReferenceCombo.highlightedIndex === azimuthItemId.index
                            }
                        }

                        InformationButton {
                            showItemOnClick: azimuthHelpId
                        }

                        QQ.Item { Layout.fillWidth: true }

                        ReadoutValue {
                            objectName: "azimuthReferenceValue"
                            text: root.interaction.referenceAvailable
                                  ? root._angle(root.interaction.referenceAzimuth)
                                  : qsTr("n/a")
                            color: root.interaction.referenceAvailable ? Theme.text : Theme.textSecondary
                            detailText: root._azimuthDetail()
                        }
                    }

                    HelpArea {
                        id: azimuthHelpId
                        Layout.fillWidth: true
                        // Each paragraph is its own qsTr() so it extracts as a complete,
                        // translatable sentence (a concatenated qsTr argument doesn't).
                        text: qsTr("<b>Azimuth reference</b> — which north the bearing is measured from. <b>Grid = the map, True = the globe, Magnetic = your compass.</b>")
                              + "<br><br>" + qsTr("<b>Grid</b> — north of the map's coordinate grid (e.g. UTM grid lines). Matches the 3D view, the survey's coordinates, and a printed map.")
                              + "<br>" + qsTr("<b>True</b> — geographic north, toward the pole. Differs from grid by the grid convergence at this location.")
                              + "<br>" + qsTr("<b>Magnetic (today)</b> — where a compass points right now. Differs from true by the magnetic declination, which drifts over time, so it's computed for today's date at this location.")
                    }

                    ReadoutRow { label: qsTr("Inclination"); value: root._angle(root.interaction.inclination); valueName: "measurementInclinationValue" }
                }
            }

            // By Axis — the vector between the two points split into its signed
            // coordinate components. The sign is the direction (east/west, etc.).
            QC.GroupBox {
                title: qsTr("By Axis")
                Layout.fillWidth: true

                ColumnLayout {
                    width: parent.width
                    spacing: 4

                    ReadoutRow { label: qsTr("Easting (X)"); value: root._signedLength(root.interaction.deltaEast); valueName: "measurementEastingValue" }
                    ReadoutRow { label: qsTr("Northing (Y)"); value: root._signedLength(root.interaction.deltaNorth); valueName: "measurementNorthingValue" }
                    ReadoutRow { label: qsTr("Vertical (Z)"); value: root._signedLength(root.interaction.vertical); valueName: "measurementVerticalValue" }
                }
            }

            QQ.Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                QC.Switch {
                    objectName: "stationOnlySwitch"
                    text: qsTr("Station only")
                    checked: root.interaction.mode === MeasurementInteraction.StationOnly
                    onToggled: {
                        root.interaction.mode = checked
                            ? MeasurementInteraction.StationOnly
                            : MeasurementInteraction.Free
                    }
                }

                QQ.Item { Layout.fillWidth: true }

                QC.ToolButton {
                    objectName: "copyMeasurementButton"
                    text: qsTr("Copy")
                    onClicked: root.interaction.copyToClipboard()
                }
            }
        }
    }
}
