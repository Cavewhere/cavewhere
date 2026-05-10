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

QQ.Item {
    id: rootId

    property string value: ""
    property bool allowGeographic: true

    readonly property string currentMode: CoordinateSystem.modeFor(rootId.value)
    readonly property string resolvedDescription: {
        if (rootId.currentMode !== "custom") {
            return rootId.value
        }
        const name = CoordinateSystem.nameFor(rootId.value)
        return name.length > 0 ? rootId.value + " — " + name : rootId.value
    }

    signal committed(string newCS)

    implicitWidth: rowId.implicitWidth
    implicitHeight: rowId.implicitHeight

    function commitMode(mode) {
        switch (mode) {
        case "local":
            rootId.committed("")
            return
        case "latlon":
            rootId.committed("EPSG:4326")
            return
        case "utm":
            rootId.committed(CoordinateSystem.utmZoneToEpsg(
                zoneSpinId.value,
                hemiComboId.currentIndex === 0))
            return
        case "custom":
            customDialogLoader.active = true
            customDialogLoader.item.open()
            return
        }
    }

    RowLayout {
        id: rowId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        QC.ComboBox {
            id: modeComboId
            objectName: "csModePicker"

            // Two parallel arrays: visible labels (shown in the popup) and
            // canonical mode keys. allowGeographic == false hides the
            // "latlon" entry — survex's cavern can't emit geographic output.
            readonly property bool hideGeographic: !rootId.allowGeographic

            model: hideGeographic
                ? [qsTr("Local"), qsTr("UTM"), qsTr("Custom...")]
                : [qsTr("Local"), qsTr("Lat/Lon (WGS84)"), qsTr("UTM"), qsTr("Custom...")]

            function modeAt(index) {
                const modes = hideGeographic
                    ? ["local", "utm", "custom"]
                    : ["local", "latlon", "utm", "custom"]
                return modes[index]
            }

            function indexForMode(mode) {
                const modes = hideGeographic
                    ? ["local", "utm", "custom"]
                    : ["local", "latlon", "utm", "custom"]
                const i = modes.indexOf(mode)
                return i >= 0 ? i : 0
            }

            currentIndex: indexForMode(rootId.currentMode)

            onActivated: (index) => {
                const mode = modeAt(index)
                if (mode === rootId.currentMode && mode !== "custom") {
                    return
                }
                rootId.commitMode(mode)
            }
        }

        QC.SpinBox {
            id: zoneSpinId
            objectName: "csUtmZone"
            visible: rootId.currentMode === "utm"
            from: 1
            to: 60
            value: {
                const z = CoordinateSystem.utmZoneFor(rootId.value)
                return z > 0 ? z : 16
            }
            editable: true
            onValueModified: {
                if (rootId.currentMode === "utm") {
                    rootId.commitMode("utm")
                }
            }
        }

        QC.ComboBox {
            id: hemiComboId
            objectName: "csUtmHemisphere"
            visible: rootId.currentMode === "utm"
            model: ["N", "S"]
            currentIndex: CoordinateSystem.utmNorthFor(rootId.value) ? 0 : 1
            onActivated: {
                if (rootId.currentMode === "utm") {
                    rootId.commitMode("utm")
                }
            }
        }

        QC.Label {
            objectName: "csResolvedLabel"
            visible: rootId.currentMode === "custom"
                     || rootId.currentMode === "utm"
            text: rootId.resolvedDescription
            color: Theme.textSubtle
            font.family: Theme.fontFamilyMono
            elide: QC.Label.ElideRight
            Layout.fillWidth: true
        }
    }

    // Lazy: each fix-station row instantiates a CSComboBox, so we defer
    // CSCustomDialog (and its CRSSearchModel, which loads ~7000 EPSG rows
    // from proj.db on first use) until the user actually picks "Custom...".
    QQ.Loader {
        id: customDialogLoader
        active: false
        sourceComponent: customDialogComponent
    }

    QQ.Component {
        id: customDialogComponent
        CSCustomDialog {
            onAccepted: (cs) => rootId.committed(cs)
        }
    }
}
