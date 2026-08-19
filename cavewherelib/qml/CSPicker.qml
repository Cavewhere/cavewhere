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

    //! The datum a value that names none is committed on. The host binds the
    //! project's context datum here (cwCavingRegion::defaultFixDatum) — this
    //! item knows nothing about regions, and both fix surfaces bind it.
    property string defaultDatum: CoordinateSystem.wgs84()

    readonly property int currentMode: CoordinateSystem.modeFor(rootId.value)

    // Whether the zone and hemisphere controls apply.
    readonly property bool showsUtm: rootId.currentMode === CoordinateSystem.UTM

    // What the controls are on: the value's own datum whenever it names one, so
    // a stored row keeps saying what it stores, and the host's default only for
    // a value that names none — a blank system, or a Custom CRS on its way to
    // Lat/Lon.
    readonly property string currentDatum: {
        const own = CoordinateSystem.datumFor(rootId.value)
        if (own !== "") {
            return own
        }
        return CoordinateSystem.latLonCS(rootId.defaultDatum) !== ""
                ? rootId.defaultDatum
                : CoordinateSystem.wgs84()
    }

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

    // Commits the system \a mode names on \a datumCode, built from the zone and
    // hemisphere on screen. A datum whose series stops short of that zone falls
    // back to WGS84, the one series covering all sixty — the same fallback the
    // datum combo shows, so the two agree on what a zone edit did.
    function commitCS(mode: int, datumCode: string): void {
        switch (mode) {
        case CoordinateSystem.LatLon: {
            const cs = CoordinateSystem.latLonCS(datumCode)
            rootId.committed(cs === "" ? CoordinateSystem.wgs84() : cs)
            return
        }
        case CoordinateSystem.UTM: {
            const zone = zoneSpinId.value
            const north = hemiComboId.currentIndex === 0
            const cs = CoordinateSystem.utmZoneToEpsg(zone, north, datumCode)
            rootId.committed(cs === "" ? CoordinateSystem.utmZoneToEpsg(zone, north) : cs)
            return
        }
        }
    }

    function commitMode(mode) {
        if (mode === CoordinateSystem.Custom) {
            customDialogLoader.active = true
            customDialogLoader.item.open()
            return
        }
        // Keep the row on the datum the controls show, so editing a zone or a
        // hemisphere moves only the zone.
        rootId.commitCS(mode, rootId.currentDatum)
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

            // The datum is the combo beside this one, so the mode names only
            // the shape of the coordinate.
            model: [qsTr("Lat/Lon"), qsTr("UTM"), qsTr("Custom...")]

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

        // Last in the row and quieter than the controls before it: the datum is
        // the answer most rows never change, and a wrong one moves a fix by a
        // meter or two rather than by a zone.
        QC.ComboBox {
            id: datumComboId
            objectName: "csDatum"

            // UTM offers only the datums whose series reaches the zone and
            // hemisphere on screen, so every entry names a system this picker
            // can build.
            readonly property list<string> datumCodes: rootId.showsUtm
                ? CoordinateSystem.utmDatumList(zoneSpinId.value, hemiComboId.currentIndex === 0)
                : CoordinateSystem.datumList()

            //! What one popup row needs: the widest label as the style lays it
            //! out, and at least the closed control's own width.
            readonly property real popupRowWidth:
                Math.max(datumComboId.width, datumRowProbeId.implicitWidth)

            // Custom carries its datum inside the CRS the dialog picked, and
            // Local has none, so only Lat/Lon and UTM ask for one.
            visible: rootId.currentMode === CoordinateSystem.LatLon || rootId.showsUtm
            width: Theme.csDatumFieldWidth
            height: modeComboId.height
            font.pixelSize: Theme.fontSizeSmall
            // The rows lead with the region because that, rather than the
            // acronym, is what tells a caver which datum is theirs. The closed
            // control keeps the short name, so the field stays as narrow as the
            // table column it sits in.
            model: datumComboId.datumCodes.map(
                       (code) => CoordinateSystem.datumRegionName(code) + " · "
                                 + CoordinateSystem.datumDisplayName(code))
            // WGS84 leads the table, so index 0 is the fallback a datum outside
            // this list commits to — which is what a zone edit past the end of
            // a series does.
            currentIndex: Math.max(0, datumComboId.datumCodes.indexOf(rootId.currentDatum))
            displayText: CoordinateSystem.datumDisplayName(
                             datumComboId.datumCodes[datumComboId.currentIndex] ?? "")

            // A ComboBox popup is as wide as its control, which would elide
            // every region-led row. Both the rows and the popup take the width
            // the probe below measures.
            popup.width: datumComboId.popupRowWidth
                         + datumComboId.popup.leftPadding
                         + datumComboId.popup.rightPadding

            delegate: QC.ItemDelegate {
                id: datumItemId

                required property int index
                required property string modelData

                width: datumComboId.popupRowWidth
                highlighted: datumComboId.highlightedIndex === datumItemId.index

                contentItem: QC.Label {
                    objectName: "csDatumItemLabel." + datumItemId.index
                    text: datumItemId.modelData
                    font.pixelSize: Theme.fontSizeSmall
                    elide: QQ.Text.ElideRight
                    verticalAlignment: QQ.Text.AlignVCenter
                }
            }

            onActivated: (index) => rootId.commitCS(rootId.currentMode,
                                                    datumComboId.datumCodes[index])
        }
    }

    QQ.FontMetrics {
        id: datumFontMetricsId
        font.pixelSize: Theme.fontSizeSmall
    }

    // A row of the popup's own kind, off the Flow and out of sight, carrying the
    // longest label: its implicitWidth is what the style needs for that row,
    // padding included, which is the one number the popup and its rows share.
    QC.ItemDelegate {
        id: datumRowProbeId

        visible: false
        font.pixelSize: Theme.fontSizeSmall
        text: {
            let widest = ""
            let widestWidth = 0
            for (const label of datumComboId.model) {
                const labelWidth = datumFontMetricsId.advanceWidth(label)
                if (labelWidth > widestWidth) {
                    widestWidth = labelWidth
                    widest = label
                }
            }
            return widest
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
