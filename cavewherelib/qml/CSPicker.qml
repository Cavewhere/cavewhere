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

    // Which modes this host offers. A project that isn't georeferenced needs
    // Local to say so; a fix-station row does not — it needs Project, which is
    // meaningless on the project's own picker. See issues #618 and #625.
    property bool allowLocal: true
    property bool allowProject: false

    // The project's global CS, so Project has something to stamp in. Only the
    // fix-station hosts supply it.
    property string projectCS: ""

    // Project is a display rule, not stored state: the row shows it whenever it
    // holds the project's CS, and stops showing it the moment the project moves
    // to something else — the row stays put, which is the point. The non-empty
    // guard keeps an unset row out of Project when the project has no CS either.
    readonly property int currentMode: rootId.allowProject
                                       && rootId.value !== ""
                                       && rootId.value === rootId.projectCS
                                       ? CoordinateSystem.Project
                                       : CoordinateSystem.modeFor(rootId.value)

    // Whether the zone and hemisphere controls apply — the row's CS is a UTM
    // zone, whatever the combo labels it. A row holding the project's CS reads
    // as Project, but when that CS is a UTM zone the zone is still the thing to
    // edit; keying the controls off the label alone would hide them and leave
    // the zone unreachable except through the Custom dialog. Editing it commits
    // a different zone, which no longer matches the project, so the label falls
    // back to UTM on its own.
    readonly property bool showsUtm: rootId.currentMode === CoordinateSystem.UTM
                                     || (rootId.currentMode === CoordinateSystem.Project
                                         && CoordinateSystem.modeFor(rootId.value) === CoordinateSystem.UTM)

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
        case CoordinateSystem.Project:
            rootId.committed(rootId.projectCS)
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

            // One ordered list, filtered by the host's flags, rather than a
            // literal per combination — allowGeographic (survex's cavern can't
            // emit geographic output), allowLocal and allowProject already make
            // eight, and the next flag would double that again.
            readonly property var entries: [
                { mode: CoordinateSystem.Local,   label: qsTr("Local"),            allowed: rootId.allowLocal },
                { mode: CoordinateSystem.LatLon,  label: qsTr("Lat/Lon (WGS84)"),  allowed: rootId.allowGeographic },
                { mode: CoordinateSystem.UTM,     label: qsTr("UTM"),              allowed: true },
                // Hidden while the project has no CS of its own — picking it
                // then would commit the blank this whole change exists to retire.
                { mode: CoordinateSystem.Project, label: qsTr("Project"),          allowed: rootId.allowProject && rootId.projectCS !== "" },
                { mode: CoordinateSystem.Custom,  label: qsTr("Custom..."),        allowed: true }
            ].filter((e) => e.allowed)

            readonly property var modes: entries.map((e) => e.mode)

            model: entries.map((e) => e.label)

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

            // A ComboBox resets currentIndex to the top of the list whenever its
            // model is replaced, and it writes the value straight in rather than
            // through the property system — so the binding above is left intact
            // but not dirty, and never recovers. The model is replaced whenever
            // Project appears or disappears (the project gaining or losing a CS),
            // which would silently relabel every row to the first entry. Restore
            // the binding after the reset.
            onModelChanged: Qt.callLater(() => {
                modeComboId.currentIndex = Qt.binding(
                            () => modeComboId.indexForMode(rootId.currentMode))
            })

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
