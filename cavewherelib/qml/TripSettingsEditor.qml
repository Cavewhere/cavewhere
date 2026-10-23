import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Per-trip options. The Columns row hides the LRUD or Splays column of the
// survey table, which a trip can do only while that column is empty.
ColumnLayout {
    id: settingsEditorId
    objectName: "tripSettingsEditor"

    required property Trip trip

    readonly property TripCalibration _calibration: trip === null ? null : trip.calibration

    spacing: 5

    SectionLabel {
        text: "Settings"
        Layout.alignment: Qt.AlignHCenter
    }

    RowLayout {
        spacing: Theme.flowSpacing

        QC.Label {
            text: "Columns:"
        }

        ColumnToggle {
            name: "lrud"
            text: "LRUD"
            columnShown: settingsEditorId._calibration !== null && settingsEditorId._calibration.lruds
            hasData: settingsEditorId.trip !== null && settingsEditorId.trip.hasLrudData
            lockedText: "LRUD can be hidden once every L, R, U and D box in this trip is empty."
            editable: settingsEditorId._calibration !== null

            onToggled: (shown) => { settingsEditorId._calibration.lruds = shown }
        }

        ColumnToggle {
            name: "splays"
            text: "Splays"
            columnShown: settingsEditorId._calibration !== null && settingsEditorId._calibration.splays
            hasData: settingsEditorId.trip !== null && settingsEditorId.trip.hasSplayData
            lockedText: "Splays can be hidden once every splay in this trip is removed."
            editable: settingsEditorId._calibration !== null

            onToggled: (shown) => { settingsEditorId._calibration.splays = shown }
        }
    }

    // A disabled item receives no hover, so the tooltip that says why the
    // checkbox is locked hangs off this always-enabled wrapper instead
    component ColumnToggle: QQ.Item {
        id: toggleId
        objectName: name + "CheckBoxWrapper"

        required property string name
        required property string text
        required property bool columnShown
        required property bool hasData
        required property string lockedText
        required property bool editable

        signal toggled(bool shown)

        implicitWidth: checkBoxId.implicitWidth
        implicitHeight: checkBoxId.implicitHeight

        QQ.HoverHandler {
            id: hoverId
        }

        QC.CheckBox {
            id: checkBoxId
            objectName: toggleId.name + "CheckBox"

            text: toggleId.text
            checked: toggleId.columnShown
            enabled: toggleId.editable && (!checked || !toggleId.hasData)

            onToggled: toggleId.toggled(checked)
        }

        QC.ToolTip {
            objectName: toggleId.name + "CheckBoxToolTip"

            text: toggleId.lockedText
            visible: hoverId.hovered && toggleId.editable && !checkBoxId.enabled
            delay: Theme.toolTipDelay
        }
    }
}
