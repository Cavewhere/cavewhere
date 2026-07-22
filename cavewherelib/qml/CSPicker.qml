/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The coordinate-system picker controls — mode combo, plus the UTM zone spinbox
// and N/S hemisphere combo — and nothing else. It is deliberately presentation-
// free: no resolved-name label, no EPSG line. The two shells wrap it: CSComboBox
// adds the compact inline label (fix-station rows); DataMainPage's project
// GroupBox adds the name/EPSG lines. Laid out in a QQ.Flow so a host narrower
// than the controls (the project's slim info column) wraps the trailing controls
// onto a second line instead of overflowing; a host at least oneLineWidth wide
// (a fix-station cell) keeps them on one line.
QQ.Item {
    id: rootId

    property string value: ""
    property bool allowGeographic: true

    readonly property int currentMode: CoordinateSystem.modeFor(rootId.value)

    // The width the visible controls need on a single line, summed generically
    // from the Flow's children (their explicit/implicit widths and spacing) so
    // adding or hiding a control needs no edit here. Drives implicitWidth so an
    // unconstrained host gets one line; a narrower host wraps.
    readonly property real oneLineWidth: {
        let w = 0
        let count = 0
        const kids = flowId.children
        for (let i = 0; i < kids.length; i++) {
            const c = kids[i]
            if (!c.visible) {
                continue
            }
            w += c.width
            count += 1
        }
        return count > 0 ? w + (count - 1) * flowId.spacing : 0
    }

    signal committed(string newCS)

    implicitWidth: rootId.oneLineWidth
    implicitHeight: flowId.implicitHeight

    function commitMode(mode) {
        switch (mode) {
        case CoordinateSystem.Local:
            rootId.committed("")
            return
        case CoordinateSystem.LatLon:
            rootId.committed(CoordinateSystem.wgs84())
            return
        case CoordinateSystem.UTM:
            rootId.committed(CoordinateSystem.utmZoneToEpsg(
                zoneSpinId.value,
                hemiComboId.currentIndex === 0))
            return
        case CoordinateSystem.Custom:
            customDialogLoader.active = true
            customDialogLoader.item.open()
            return
        }
    }

    QQ.Flow {
        id: flowId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        QC.ComboBox {
            id: modeComboId
            objectName: "csModePicker"

            // allowGeographic == false hides LatLon — survex's cavern can't
            // emit geographic output.
            readonly property bool hideGeographic: !rootId.allowGeographic

            readonly property var modes: hideGeographic
                ? [CoordinateSystem.Local,
                   CoordinateSystem.UTM,
                   CoordinateSystem.Custom]
                : [CoordinateSystem.Local,
                   CoordinateSystem.LatLon,
                   CoordinateSystem.UTM,
                   CoordinateSystem.Custom]

            model: hideGeographic
                ? [qsTr("Local"), qsTr("UTM"), qsTr("Custom...")]
                : [qsTr("Local"), qsTr("Lat/Lon (WGS84)"), qsTr("UTM"), qsTr("Custom...")]

            function modeAt(index) {
                return modes[index]
            }

            function indexForMode(mode) {
                const i = modes.indexOf(mode)
                return i >= 0 ? i : 0
            }

            currentIndex: indexForMode(rootId.currentMode)

            onActivated: (index) => {
                const mode = modeAt(index)
                if (mode === rootId.currentMode && mode !== CoordinateSystem.Custom) {
                    return
                }
                rootId.commitMode(mode)
            }
        }

        QC.SpinBox {
            id: zoneSpinId
            objectName: "csUtmZone"
            visible: rootId.currentMode === CoordinateSystem.UTM
            width: Theme.csZoneFieldWidth
            // The native macOS style sizes a SpinBox shorter than a ComboBox (24
            // vs 32); QQ.Flow top-aligns a row, so the shorter box would ride
            // high. Match the mode combo's height to center all three. A no-op in
            // Fusion/Basic, where the two controls are already the same height.
            height: modeComboId.height
            from: 1
            to: 60
            value: {
                const z = CoordinateSystem.utmZoneFor(rootId.value)
                return z > 0 ? z : 16
            }
            editable: true
            onValueModified: {
                if (rootId.currentMode === CoordinateSystem.UTM) {
                    rootId.commitMode(CoordinateSystem.UTM)
                }
            }
        }

        QC.ComboBox {
            id: hemiComboId
            objectName: "csUtmHemisphere"
            visible: rootId.currentMode === CoordinateSystem.UTM
            width: Theme.csHemisphereFieldWidth
            model: ["N", "S"]
            currentIndex: CoordinateSystem.utmNorthFor(rootId.value) ? 0 : 1
            onActivated: {
                if (rootId.currentMode === CoordinateSystem.UTM) {
                    rootId.commitMode(CoordinateSystem.UTM)
                }
            }
        }
    }

    // Lazy: each fix-station row instantiates a picker, so we defer
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
