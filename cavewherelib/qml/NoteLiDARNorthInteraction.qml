/**************************************************************************
**
**    NoteLiDARNorthInteraction.qml
**    Interactive tool for aligning LiDAR notes to north using two picked
**    points on the model while the view is presented in plan.
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

Interaction {
    id: lidarNorthInteraction
    objectName: "noteLiDARNorthInteraction"

    property NoteLiDARTransformation noteTransform
    property NoteLiDAR note
    property RegionViewer viewer
    property TurnTableInteraction turnTableInteraction
    property GltfScene scene

    readonly property bool hasFirstPoint: firstPick !== null
    property var firstPick: null
    property point firstScreenPoint: Qt.point(0, 0)
    property real originalPitch: 0.0
    property bool originalPitchValid: false
    property bool previousPitchLocked: false

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    function resetState() {
        firstPick = null
        northArrow.visible = false
        helpBox.text = "Activate to set LiDAR north"
    }

    function finish() {
        lidarNorthInteraction.deactivate()
    }

    function animatePitch(targetPitch) {
        if (!turnTableInteraction) {
            return
        }
        pitchAnimator.stop()
        if (Math.abs(turnTableInteraction.pitch - targetPitch) < 0.01) {
            turnTableInteraction.pitch = targetPitch
            return
        }
        pitchAnimator.target = turnTableInteraction
        pitchAnimator.from = turnTableInteraction.pitch
        pitchAnimator.to = targetPitch
        pitchAnimator.start()
    }

    function handleSecondPoint(pointModel) {
        if (!noteTransform || !hasFirstPoint) {
            return
        }
        const angle = noteTransform.calculateNorth(firstPick.pointModel, pointModel)
        if (!isNaN(angle)) {
            noteTransform.northUp = angle
        }
        finish()
    }

    onTurnTableInteractionChanged: pitchAnimator.target = turnTableInteraction

    onActivated: {
        resetState()
        helpBox.text = "<b>Click</b> first north reference point"
        if (turnTableInteraction) {
            originalPitch = turnTableInteraction.pitch
            originalPitchValid = true
            previousPitchLocked = turnTableInteraction.pitchLocked
            turnTableInteraction.pitchLocked = true
            animatePitch(90.0)
        } else {
            originalPitchValid = false
        }
    }

    onDeactivated: {
        northArrow.visible = false
        if (turnTableInteraction) {
            turnTableInteraction.pitchLocked = previousPitchLocked
        }
        if (turnTableInteraction && originalPitchValid) {
            animatePitch(originalPitch)
        }
        resetState()
    }

    QQ.NumberAnimation {
        id: pitchAnimator
        property: "pitch"
        duration: 350
        easing.type: QQ.Easing.InOutQuad
    }

    QQ.TapHandler {
        id: tapHandler
        target: lidarNorthInteraction
        acceptedButtons: Qt.LeftButton
        enabled: lidarNorthInteraction.enabled
        onTapped: (eventPoint, button) => {
            if (!turnTableInteraction || !noteTransform) {
                return
            }

            const hit = turnTableInteraction.pick(eventPoint.position)
            if (!hit.hit) {
                helpBox.text = "No LiDAR geometry under cursor. Try again."
                return
            }

            if (!hasFirstPoint) {
                firstPick = hit
                firstScreenPoint = eventPoint.position
                northArrow.p1 = firstScreenPoint
                northArrow.p2 = firstScreenPoint
                northArrow.visible = true
                helpBox.text = "<b>Click</b> second point to set north"
            } else {
                northArrow.p2 = eventPoint.position
                handleSecondPoint(hit.pointModel)
            }
        }
    }

    QQ.HoverHandler {
        id: hoverHandler
        target: lidarNorthInteraction
        enabled: lidarNorthInteraction.enabled && lidarNorthInteraction.hasFirstPoint
        onPointChanged: {
            if (northArrow.visible) {
                northArrow.p2 = hoverHandler.point.position
            }
        }
    }

    NorthArrowItem {
        id: northArrow
        anchors.fill: parent
        visible: false
        parent: lidarNorthInteraction
    }

    HelpBox {
        id: helpBox
        text: "Activate to set LiDAR north"
        visible: lidarNorthInteraction.visible
    }

    QQ.Keys.onEscapePressed: finish()
}
