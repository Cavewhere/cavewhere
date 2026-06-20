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

    // v1 reports canonical SI (metres, degrees), matching copyToClipboard();
    // user-unit formatting is a documented follow-up.
    function _length(meters) {
        return qsTr("%1 m").arg(Number(meters).toFixed(root._lengthPrecision))
    }

    function _angle(degrees) {
        return qsTr("%1°").arg(Number(degrees).toFixed(root._anglePrecision))
    }

    component ReadoutRow: RowLayout {
        id: rowId
        required property string label
        required property string value
        property string valueName: ""

        // A little inner padding plus a content margin so the right-aligned text
        // is never sized flush to its glyphs: on a HiDPI display, a field exactly
        // as wide as contentWidth loses its leading digit to sub-pixel rounding
        // (14.2 -> 4.2). The slack absorbs that rounding.
        readonly property int _valuePadding: 4
        readonly property int _valueClipMargin: 6

        Layout.fillWidth: true
        spacing: 8

        QC.Label {
            text: rowId.label
            color: Theme.textSecondary
            Layout.fillWidth: true
        }

        // Read-only TextField (not Label) so the value can be selected and
        // copied with the mouse. Styled flat so it reads as text, not an input.
        QC.TextField {
            objectName: rowId.valueName
            text: rowId.value
            readOnly: true
            selectByMouse: true
            font.family: Theme.fontFamilyMono
            color: Theme.text
            horizontalAlignment: QQ.TextInput.AlignRight
            topPadding: 0
            bottomPadding: 0
            leftPadding: rowId._valuePadding
            rightPadding: rowId._valuePadding
            implicitWidth: contentWidth + leftPadding + rightPadding + rowId._valueClipMargin
            // The sibling label has fillWidth, so without a minimum the layout
            // shrinks this field below its text and the leading digit is clipped.
            // Pin the minimum to the content-fitting width.
            Layout.minimumWidth: implicitWidth
            background: QQ.Rectangle { color: "transparent" }
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

            ReadoutRow { label: qsTr("Distance"); value: root._length(root.interaction.distance); valueName: "measurementDistanceValue" }
            ReadoutRow { label: qsTr("Azimuth (grid)"); value: root._angle(root.interaction.azimuth) }
            ReadoutRow { label: qsTr("Inclination"); value: root._angle(root.interaction.inclination) }
            ReadoutRow { label: qsTr("Horizontal"); value: root._length(root.interaction.horizontal) }
            ReadoutRow { label: qsTr("Vertical"); value: root._length(root.interaction.vertical) }
            ReadoutRow { label: qsTr("ΔEast"); value: root._length(root.interaction.deltaEast) }
            ReadoutRow { label: qsTr("ΔNorth"); value: root._length(root.interaction.deltaNorth) }

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
