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

    required property NoteLiDAR note
    required property TurnTableInteraction turnTableInteraction
    required property QQ.matrix4x4 modelMatrix

    //Private properties
    readonly property bool _hasFirstPoint: _firstPick !== null

    property var _firstPick: null //type: cwRayTriangleHit
    property point _firstScreenPoint: Qt.point(0, 0)
    property point _secondScreenPoint: Qt.point(0, 0)
    property QQ.vector3d _secondPoint: Qt.vector3d(0, 0, 0)
    property real _measuredBearing: 0.0
    property real _userAzimuth: NaN

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    function pickHit(eventPoint) {
        const hit = turnTableInteraction.pick(eventPoint.position)
        if (!hit.hit) {
            helpBox.text = "No LiDAR geometry under cursor. Try again."
            return null
        }
        return hit
    }

    function handleFirstTap(eventPoint) {
        const hit = pickHit(eventPoint)
        if (!hit) {
            return
        }
        _firstPick = hit
        _firstScreenPoint = eventPoint.position
        northArrowProjector.enabled = true
        northArrowProjector.p1World = hit.pointModel
        northArrow.p1 = _firstScreenPoint
        northArrow.p2 = _firstScreenPoint
        state = "AwaitSecondPick"
    }

    function handleSecondTap(eventPoint) {
        if (!_hasFirstPoint) {
            return
        }
        const hit = pickHit(eventPoint)
        if (!hit) {
            return
        }
        const noteTransform = note.noteTransformation
        _secondScreenPoint = eventPoint.position
        _secondPoint = hit.pointModel
        northArrowProjector.p1World = _firstPick.pointModel
        northArrowProjector.p2World = _secondPoint
        northArrowProjector.enabled = true
        _measuredBearing = noteTransform.calculateNorth(_firstPick.pointModel, _secondPoint)
        if (isNaN(_measuredBearing)) {
            _measuredBearing = noteTransform.northUp
        }
        _userAzimuth = 0.0 //Start off with north
        state = "AwaitAzimuth"
    }

    function finish() {
        lidarNorthInteraction.deactivate()
    }

    function animatePitch(targetPitch) {
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

    onDeactivated: {
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

    TwoPointProjector {
        id: northArrowProjector
        target: northArrow
        camera: lidarNorthInteraction.turnTableInteraction.camera
        modelMatrix: lidarNorthInteraction.modelMatrix
        enabled: false
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
            objectName: "azimuthInput"
            onFinishedEditting: (newText) => {
                lidarNorthInteraction._userAzimuth = Number(newText)
            }
        }

        QC.Button {
            id: azimuthApplyButton
            objectName: "apply"
            text: "Apply"
            Layout.alignment: Qt.AlignVCenter
            onClicked: {
                const noteTransform = lidarNorthInteraction.note.noteTransformation
                noteTransform.northUp = noteTransform.wrapDegrees360(lidarNorthInteraction._measuredBearing - lidarNorthInteraction._userAzimuth)
                finish()
            }
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
            name: ""
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
            }

            //initilization script
            QQ.StateChangeScript {
                script: () => {
                    _firstPick = null
                    _firstScreenPoint = Qt.point(0, 0)
                    _secondScreenPoint = Qt.point(0, 0)
                    _secondPoint = Qt.vector3d(0, 0, 0)
                    _measuredBearing = 0.0
                    _userAzimuth = 0.0
                    northArrowProjector.enabled = false
                    northArrowProjector.p1World = Qt.vector3d(0, 0, 0)
                    northArrowProjector.p2World = Qt.vector3d(0, 0, 0)
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
                azimuthPanel.visible: false
                helpBox.text: "<b>Click</b> second point to set north"
                tapHandler.onTapped: function(eventPoint, button) {
                    handleSecondTap(eventPoint)
                }

            }

            QQ.StateChangeScript {
                script: () => {
                    northArrow.p1 = _firstScreenPoint
                    northArrow.p2 = _firstScreenPoint
                }
            }
        },
        QQ.State {
            name: "AwaitAzimuth"
            QQ.PropertyChanges {
                hoverHandler.enabled: false
                hoverHandler.onPointChanged: function() {}
                northArrow.visible: true
                azimuthPanel.visible: true
                azimuthInput.text: Number(lidarNorthInteraction._userAzimuth).toFixed(1)
                helpBox.text: "Confirm or adjust measured azimuth"
                tapHandler.onTapped: function(eventPoint, button) {
                    handleFirstTap(eventPoint)
                }
            }
            QQ.StateChangeScript {
                script: setAzimuthPanelPosition(_secondScreenPoint)
            }
        }
    ]
}
