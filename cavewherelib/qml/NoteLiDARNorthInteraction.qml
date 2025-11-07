/**************************************************************************
**
**    NoteLiDARNorthInteraction.qml
**    Interactive tool for aligning LiDAR notes to north using two picked
**    points on the model while the view is presented in plan.
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

Interaction {
    id: lidarNorthInteraction
    objectName: "noteLiDARNorthInteraction"
    state: "Idle"

    property NoteLiDARTransformation noteTransform
    property NoteLiDAR note
    property RegionViewer viewer
    property TurnTableInteraction turnTableInteraction
    property GltfScene scene

    readonly property bool hasFirstPoint: firstPick !== null

    property var firstPick: null //type: cwRayTriangleHit
    property point firstScreenPoint: Qt.point(0, 0)
    property point secondScreenPoint: Qt.point(0, 0)
    property QQ.vector3d secondPoint: Qt.vector3d(0, 0, 0)
    property real measuredBearing: 0.0
    property real userAzimuth: NaN

    property real originalPitch: 0.0
    property bool originalPitchValid: false
    property bool previousPitchLocked: false

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    function pickHit(eventPoint) {
        if (!turnTableInteraction) {
            return null
        }
        const hit = turnTableInteraction.pick(eventPoint.position)
        if (!hit.hit) {
            helpBox.text = "No LiDAR geometry under cursor. Try again."
            return null
        }
        return hit
    }

    function handleFirstTap(eventPoint) {
        if (!noteTransform) {
            return
        }
        const hit = pickHit(eventPoint)
        if (!hit) {
            return
        }
        firstPick = hit
        firstScreenPoint = eventPoint.position
        state = "AwaitSecondPick"
    }

    function handleSecondTap(eventPoint) {
        if (!noteTransform || !hasFirstPoint) {
            return
        }
        const hit = pickHit(eventPoint)
        if (!hit) {
            return
        }
        secondScreenPoint = eventPoint.position
        secondPoint = hit.pointModel
        measuredBearing = noteTransform.calculateNorth(firstPick.pointModel, secondPoint)
        if (isNaN(measuredBearing)) {
            measuredBearing = noteTransform.northUp
        }
        userAzimuth = 0.0 //Start off with north
        state = "AwaitAzimuth"
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

    function setAzimuthPanelPosition(screenPoint) {
        const pos = lidarNorthInteraction.mapToItem(parent, screenPoint.x, screenPoint.y)
        azimuthPanel.x = pos.x - azimuthPanel.width * 0.5
        azimuthPanel.y = pos.y + 12
    }

    onTurnTableInteractionChanged: pitchAnimator.target = turnTableInteraction

    onActivated: {
        state = "Idle"
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
        if (turnTableInteraction) {
            turnTableInteraction.pitchLocked = previousPitchLocked
        }
        if (turnTableInteraction && originalPitchValid) {
            animatePitch(originalPitch)
        }
        state = ""
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
    }

    QQ.HoverHandler {
        id: hoverHandler
        target: lidarNorthInteraction
        enabled: false
    }

    NorthArrowItem {
        id: northArrow
        anchors.fill: parent
        visible: false
        parent: lidarNorthInteraction
    }

    RowLayoutPanel {
        id: azimuthPanel
        visible: false

        Text {
            text: "Azimuth"
            Layout.alignment: Qt.AlignVCenter
        }

        ClickNumberInput {
            id: azimuthInput
            onFinishedEditting: (newText) => {
                lidarNorthInteraction.userAzimuth = Number(newText)
            }
        }

        QC.Button {
            id: azimuthApplyButton
            text: "Apply"
            Layout.alignment: Qt.AlignVCenter
        }
    }

    HelpBox {
        id: helpBox
        text: "Activate to set LiDAR north"
        visible: lidarNorthInteraction.visible
    }

    QQ.Keys.onEscapePressed: finish()

    states: [
        QQ.State {
            name: "Idle"
            QQ.PropertyChanges {
                hoverHandler.enabled: false
                hoverHandler.onPointChanged: function() {}
                northArrow.visible: false
                azimuthPanel.visible: false
                helpBox.text: "<b>Click</b> first north reference point"
                azimuthInput.text: "0.0"
                tapHandler.onTapped: function(eventPoint, button) {
                    handleFirstTap(eventPoint)
                }
                azimuthApplyButton.onClicked: function() {}
            }

            //initilization script
            QQ.StateChangeScript {
                script: () => {
                    firstPick = null
                    firstScreenPoint = Qt.point(0, 0)
                    secondScreenPoint = Qt.point(0, 0)
                    secondPoint = Qt.vector3d(0, 0, 0)
                    measuredBearing = 0.0
                    userAzimuth = 0.0
                }
            }
        },
        QQ.State {
            name: "AwaitSecondPick"
            QQ.PropertyChanges {
                hoverHandler.enabled: true
                hoverHandler.onPointChanged: function(event) {
                    northArrow.p2 = hoverHandler.point.position
                }
                northArrow.visible: true
                northArrow.p1: firstScreenPoint
                azimuthPanel.visible: false
                helpBox.text: "<b>Click</b> second point to set north"
                tapHandler.onTapped: function(eventPoint, button) {
                    handleSecondTap(eventPoint)
                }
                azimuthApplyButton.onClicked: function() {}

            }

            QQ.StateChangeScript {
                script: () => {
                    northArrow.p2 = firstScreenPoint
                }
            }
        },
        QQ.State {
            name: "AwaitAzimuth"
            QQ.PropertyChanges {
                hoverHandler.enabled: false
                hoverHandler.onPointChanged: function() {}
                northArrow.visible: true
                northArrow.p1: firstScreenPoint
                northArrow.p2: secondScreenPoint
                azimuthPanel.visible: true
                azimuthInput.text: Number(lidarNorthInteraction.userAzimuth).toFixed(1)
                helpBox.text: "Confirm or adjust measured azimuth"
                tapHandler.onTapped: function(eventPoint, button) {
                    handleFirstTap(eventPoint)
                }
                azimuthApplyButton.onClicked: function() {
                    noteTransform.northUp = lidarNorthInteraction.measuredBearing + lidarNorthInteraction.userAzimuth
                    finish()
                }
            }
            QQ.StateChangeScript {
                script: setAzimuthPanelPosition(secondScreenPoint)
            }
        }
    ]
}
