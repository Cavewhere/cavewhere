/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The 3D-view half of the fix-station pick: while one stands, it arms the
// coordinate picker, says which station the next click places, and writes the
// picked point back to the fix.
//
// It reuses the view's own CoordinatePicker rather than owning a second one, so
// the ray-cast, the crosshair cursor and the marker are the ones the Pick tool
// already draws.
//
// Three things end a pick that was never answered: Escape and arming another
// tool both take the picker out of the active slot, so watching the active
// interaction covers them together, and leaving the View page is watched
// through ViewTools. The Escape the banner promises is the picker's own
// window-context shortcut, which answers wherever the focus happens to sit.
QQ.Item {
    id: toolId

    required property GLTerrainRenderer view

    readonly property CoordinatePicker picker: toolId.view.coordinatePickerInteraction
    readonly property InteractionManager interactionManager: toolId.view.interactionManager

    //! The gap every banner over the 3D scene keeps from the edge it hangs on.
    readonly property int bannerMargin: 20

    anchors.fill: parent
    // Clickable chrome has to claim the overlay layer: LeadView and
    // LinePlotLabelView fill the renderer at zLabels and take every press first.
    z: view.zOverlay
    visible: FixStationPick.active

    // Writes \a scenePoint — a point in the project's local frame — to the fix
    // the pick names, and returns to the page it was started from. Split out
    // from the picker's own signal so a test can drive the write without a
    // ray-cast: an offscreen scene has no geometry to hit.
    function commitPick(scenePoint: QQ.vector3d): void {
        let cave = FixStationPick.cave
        if (cave === null || cave.fixStations === null) {
            FixStationPick.cancel()
            return
        }

        let region = RootData.region
        let wrote = cave.fixStations.setPickedPoint(FixStationPick.fixId,
                                                    scenePoint,
                                                    region.geoReference.localCoordinateSystem,
                                                    region.defaultFixDatum)
        if (!wrote) {
            // Nothing the pick could be written as. It stays pending, so the
            // user can orbit and click somewhere else.
            return
        }

        RootData.pageSelectionModel.back()
        FixStationPick.cancel()
    }

    ViewToolRegistration {
        toolName: ViewTools.fixStationPick

        onArmed: {
            toolId.picker.clearPick()
            toolId.picker.activate()
        }
    }

    // Whichever way a pick ends, the picker goes back to being the Pick tool the
    // user armed themselves — off, unless they armed it.
    QQ.Connections {
        target: FixStationPick

        function onActiveChanged(): void {
            if (!FixStationPick.active
                    && toolId.interactionManager.activeInteraction === toolId.picker) {
                toolId.picker.deactivate()
            }
        }
    }

    QQ.Connections {
        target: toolId.picker
        enabled: FixStationPick.active

        function onPickChanged(): void {
            if (toolId.picker.hasPick) {
                toolId.commitPick(toolId.picker.scenePoint)
            }
        }
    }

    // The picker leaving the active slot means the user pressed Escape over the
    // scene or armed another tool — either way they are no longer answering.
    QQ.Connections {
        target: toolId.interactionManager
        enabled: FixStationPick.active

        function onActiveInteractionChanged(): void {
            if (toolId.interactionManager.activeInteraction !== toolId.picker) {
                FixStationPick.cancel()
            }
        }
    }

    // A pick nobody is in front of any more is abandoned. Arriving on the View
    // page is what the pick asked for, so only leaving it ends one.
    QQ.Connections {
        target: ViewTools
        enabled: FixStationPick.active

        function onViewPageCurrentChanged(): void {
            if (!ViewTools.viewPageCurrent) {
                FixStationPick.cancel()
            }
        }
    }

    HelpBox {
        objectName: "fixStationPickHelpBox"
        // Top, like every other banner over the 3D scene — HelpBox itself
        // defaults to the bottom, which is where the notes editor wants it.
        anchors.top: parent.top
        anchors.topMargin: toolId.bannerMargin
        anchors.bottom: undefined
        // The station name is the user's own text, so it can't be pasted into
        // markup — HelpBox reads AutoText by default.
        textFormat: QC.Label.PlainText
        text: qsTr("Click the terrain to place %1. Esc cancels.").arg(FixStationPick.stationName)
    }
}
