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
// free: no resolved-name label, no EPSG line. CSComboBox wraps it to add the
// compact inline label used by fix-station rows. Laid out in a QQ.Flow so a host
// narrower than the controls wraps the trailing controls onto a second line
// instead of overflowing; a host at least oneLineWidth wide (a fix-station cell)
// keeps them on one line.
QQ.Item {
    id: rootId

    property string value: ""

    readonly property int currentMode: CoordinateSystem.modeFor(rootId.value)

    // Whether the zone and hemisphere controls apply.
    readonly property bool showsUtm: rootId.currentMode === CoordinateSystem.UTM

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

            readonly property var modes: [
                CoordinateSystem.LatLon,
                CoordinateSystem.UTM,
                CoordinateSystem.Custom
            ]

            model: [qsTr("Lat/Lon (WGS84)"), qsTr("UTM"), qsTr("Custom...")]

            function modeAt(index) {
                return modes[index]
            }

            // -1, not 0, when this host doesn't offer the value's mode — the
            // combo then shows nothing rather than naming a system the row is
            // not on. Reachable from a hand-edited file: a fix-station row
            // carrying the blank CS that Local used to mean.
            function indexForMode(mode) {
                return modes.indexOf(mode)
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
            visible: rootId.showsUtm
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
                if (rootId.showsUtm) {
                    rootId.commitMode(CoordinateSystem.UTM)
                }
            }
        }

        QC.ComboBox {
            id: hemiComboId
            objectName: "csUtmHemisphere"
            visible: rootId.showsUtm
            width: Theme.csHemisphereFieldWidth
            model: ["N", "S"]
            currentIndex: CoordinateSystem.utmNorthFor(rootId.value) ? 0 : 1
            onActivated: {
                if (rootId.showsUtm) {
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
