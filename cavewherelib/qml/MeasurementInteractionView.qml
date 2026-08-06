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

MeasurementInteraction {
    id: measurementId

    // The turn-table that owns canonical camera-pose state. We forward orbit +
    // zoom to it (left-click is reserved for placing points), the same pattern
    // the picker and clip tools use, so the compass and readouts stay in sync.
    required property BaseTurnTableInteraction turnTableInteraction

    readonly property int _snappedRingSize: 14
    readonly property int _freeDotSize: 10
    readonly property int _placedDotSize: 12

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    // The default readout unit follows the project system until the user picks
    // one; that choice persists and overrides this.
    unitSystem: ProjectUnits.unitSystem

    onDeactivated: measurementId.reset()

    // Crosshair cursor while hovering the active tool. cursorShape lives on the
    // HoverHandler (a passive handler), so clicks still fall through to the
    // TapHandler below — no MouseArea needed.
    QQ.HoverHandler {
        cursorShape: Qt.CrossCursor
        onPointChanged: measurementId.hover(point.position)
    }

    QQ.TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: (eventPoint) => measurementId.place(eventPoint.position)
    }

    TurnTableForwardingHandlers {
        turnTableInteraction: measurementId.turnTableInteraction
    }

    // Always-on screen-space connecting line (A<->B, or rubber-band A<->hover).
    MeasurementLineCanvas {
        id: lineCanvasId
        anchors.fill: parent
        // QCanvasPainterItem composites opaquely by default; pair alphaBlending
        // with a transparent clear color so only the painted line shows over
        // the GL terrain.
        alphaBlending: true
        fillColor: "transparent"
        camera: measurementId.camera
        interaction: measurementId
        lineColor: Theme.accent
        invalidColor: Theme.danger
        z: 1
    }

    // Re-project the placed/hover points through the camera on every camera
    // change so the marker dots track the geometry as the user orbits/zooms.
    TransformUpdater {
        id: markerUpdaterId
        camera: measurementId.camera
        enabled: measurementId.visible
    }

    QQ.Item {
        z: 2

        // Point A
        PositionItem {
            id: firstMarkerId
            position3D: measurementId.firstPoint
            visible: firstMarkerId.inFrustum && measurementId.hasFirst

            QQ.Rectangle {
                x: -width / 2
                y: -height / 2
                width: measurementId._placedDotSize
                height: measurementId._placedDotSize
                radius: width / 2
                color: Theme.accent
                border.color: Theme.text
                border.width: 1
            }

            QQ.Component.onCompleted: markerUpdaterId.addPointItem(firstMarkerId)
        }

        // Point B
        PositionItem {
            id: secondMarkerId
            position3D: measurementId.secondPoint
            visible: secondMarkerId.inFrustum && measurementId.hasMeasurement

            QQ.Rectangle {
                x: -width / 2
                y: -height / 2
                width: measurementId._placedDotSize
                height: measurementId._placedDotSize
                radius: width / 2
                color: Theme.accent
                border.color: Theme.text
                border.width: 1
            }

            QQ.Component.onCompleted: markerUpdaterId.addPointItem(secondMarkerId)
        }

        // Hover indicator: an accent ring when snapped to a station, a smaller
        // dot for a free point. Hidden when a click here wouldn't place a point
        // (Station-only over empty space), per hoverValid.
        PositionItem {
            id: hoverMarkerId
            position3D: measurementId.hoverPoint
            visible: hoverMarkerId.inFrustum
                     && measurementId.hoverValid && !measurementId.hasMeasurement

            QQ.Rectangle {
                x: -width / 2
                y: -height / 2
                width: measurementId.hoverSnapped ? measurementId._snappedRingSize
                                                  : measurementId._freeDotSize
                height: width
                radius: width / 2
                color: measurementId.hoverSnapped ? "transparent" : Theme.accent
                border.color: Theme.accent
                border.width: 2
            }

            QQ.Component.onCompleted: markerUpdaterId.addPointItem(hoverMarkerId)
        }

        // Live distance chip pinned to the middle of the line — tracks the
        // cursor while awaiting B, then freezes on the placed segment.
        PositionItem {
            id: midpointMarkerId
            objectName: "measurementMidpointMarker"

            property QQ.vector3d _otherPoint: measurementId.hasMeasurement
                                              ? measurementId.secondPoint
                                              : measurementId.hoverPoint

            position3D: measurementId.firstPoint.plus(midpointMarkerId._otherPoint).times(0.5)
            visible: midpointMarkerId.inFrustum
                     && measurementId.hasFirst
                     && (measurementId.hasMeasurement || measurementId.hoverValid)
            z: 1

            QQ.Rectangle {
                x: -width / 2
                y: -height / 2
                width: distanceLabelId.implicitWidth + 12
                height: distanceLabelId.implicitHeight + 6
                radius: Theme.floatingWidgetRadius
                // Semi-transparent so the centerline and geometry behind the
                // chip stay visible.
                color: Qt.rgba(Theme.surfaceRaised.r, Theme.surfaceRaised.g,
                               Theme.surfaceRaised.b, 0.7)
                border.color: Theme.border
                border.width: 1

                QC.Label {
                    id: distanceLabelId
                    objectName: "measurementLineDistanceLabel"
                    anchors.centerIn: parent
                    // Routed through the shared C++ length formatter so the chip,
                    // the readout panel, and the clipboard agree by construction.
                    // Reading the reactive lengthUnit.name re-evaluates this on a
                    // unit change; format() is a plain invokable QML can't track.
                    text: (measurementId.lengthUnit.name,
                           measurementId.lengthUnit.format(measurementId.distance))
                    color: Theme.text
                    font.family: Theme.fontFamilyMono
                }
            }

            QQ.Component.onCompleted: markerUpdaterId.addPointItem(midpointMarkerId)
        }
    }

    HelpBox {
        objectName: "measurementHelpBox"
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.bottom: undefined

        text: {
            if (!measurementId.hasFirst) {
                return measurementId.mode === MeasurementInteraction.StationOnly
                    ? qsTr("Click a survey station to start measuring.")
                    : qsTr("Click to place the first point.")
            }
            if (!measurementId.hasMeasurement) {
                return qsTr("Click to place the second point.")
            }
            return qsTr("Click to start a new measurement, or press <b>Esc</b> to exit.")
        }
    }

    // A window-context Shortcut instead of Keys.onEscapePressed: the latter
    // needs this item to hold focus, which a click on a handler or overlay can
    // steal. The shortcut is live only while the tool is active.
    QQ.Shortcut {
        sequences: ["Escape"]
        enabled: measurementId.visible
        context: Qt.WindowShortcut
        onActivated: measurementId.deactivate()
    }
}
