/**************************************************************************
**
**    NoteLiDARTwoPointInteraction.qml
**    Shared interaction logic for two-point LiDAR alignment tools.
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

Interaction {
    id: twoPointInteraction
    state: ""

    required property NoteLiDAR note
    required property TurnTableInteraction turnTableInteraction
    required property QQ.matrix4x4 modelMatrix

    property string firstHelpText: "<b>Click</b> first reference point"
    property string secondHelpText: "<b>Click</b> second reference point"
    property string adjustHelpText: "Confirm or adjust measurement"
    property string panelLabel: "Value"
    property real defaultUserValue: 0.0
    property int valuePrecision: 1
    property var measurementCalculator //type: call back function
    property var applyHandler //type: call back function
    property var valueFormatter //type: call back function
    property alias valueInputObjectName: valueInput.objectName
    property alias applyButtonObjectName: valueApplyButton.objectName
    property alias valueValidator: valueInput.validator
    property bool showAdjustmentPanel: true
    property bool autoApplyAfterSecondPick: false
    property QQ.Component guideItemComponent: null
    property QQ.Component valueEntryComponent: null
    readonly property bool hasCustomValueEntry: valueEntryComponent !== null
    readonly property real measuredValue: _measuredValue

    readonly property bool _hasFirstPoint: _firstPick !== null

    property var _firstPick: null // type: cwRayTriangleHit
    property point _firstScreenPoint: Qt.point(0, 0)
    property point _secondScreenPoint: Qt.point(0, 0)
    property QQ.vector3d _secondPoint: Qt.vector3d(0, 0, 0)
    property real _measuredValue: 0.0
    property real _userValue: defaultUserValue

    signal measurementReady(real measuredValue)

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    function _formatValue(value) {
        if (typeof valueFormatter === "function") {
            return valueFormatter(value)
        }
        if (!Number.isFinite(value)) {
            return ""
        }
        return Number(value).toFixed(valuePrecision)
    }

    function _guideItemObject() {
        return guideItemLoader.item
    }

    function _setGuideVisible(visible) {
        const guideItem = _guideItemObject()
        if (guideItem) {
            guideItem.visible = visible
        }
    }

    function _setGuideScreenPoints(p1, p2) {
        const guideItem = _guideItemObject()
        if (!guideItem) {
            return
        }
        if (p1) {
            guideItem.p1 = p1
        }
        if (p2) {
            guideItem.p2 = p2
        }
    }

    function _updateGuideHover(point) {
        const guideItem = _guideItemObject()
        if (guideItem) {
            guideItem.p2 = point
        }
    }

    function _updateDefaultValueInput(value) {
        if (!hasCustomValueEntry && valueInput) {
            valueInput.text = _formatValue(value)
        }
    }

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
        guideArrowProjector.enabled = true
        guideArrowProjector.p1World = hit.pointModel
        _setGuideScreenPoints(_firstScreenPoint, _firstScreenPoint)
        state = "AwaitSecondPick"
    }

    function _calculateMeasurement(firstPoint, secondPoint) {
        if (typeof measurementCalculator === "function") {
            return measurementCalculator(firstPoint, secondPoint)
        }
        return NaN
    }

    function handleSecondTap(eventPoint) {
        if (!_hasFirstPoint) {
            return
        }
        const hit = pickHit(eventPoint)
        if (!hit) {
            return
        }
        _secondScreenPoint = eventPoint.position
        _secondPoint = hit.pointModel
        guideArrowProjector.p1World = _firstPick.pointModel
        guideArrowProjector.p2World = _secondPoint
        guideArrowProjector.enabled = true
        let measured = _calculateMeasurement(_firstPick.pointModel, _secondPoint)
        if (!Number.isFinite(measured)) {
            measured = defaultUserValue
        }
        _measuredValue = measured
        _userValue = defaultUserValue
        _updateDefaultValueInput(_userValue)
        measurementReady(_measuredValue)
        if (autoApplyAfterSecondPick) {
            applyMeasurement()
            finish()
            return
        }
        state = "AwaitAdjust"
    }

    function finish() {
        twoPointInteraction.deactivate()
    }

    function setPanelPosition(screenPoint) {
        const pos = twoPointInteraction.mapToItem(parent, screenPoint.x, screenPoint.y)
        valuePanel.x = pos.x - valuePanel.width * 0.5
        valuePanel.y = pos.y + 12
    }

    function applyMeasurement() {
        if (typeof applyHandler === "function") {
            applyHandler({
                             measuredValue: _measuredValue,
                             userValue: _userValue,
                             firstPoint: _firstPick ? _firstPick.pointModel : null,
                             secondPoint: _secondPoint
                         })
        }
    }

    QQ.TapHandler {
        id: tapHandler
        target: twoPointInteraction
        acceptedButtons: Qt.LeftButton
        onTapped: (eventPoint, button) => {
                      handleFirstTap(eventPoint)
                  }
    }


    QQ.HoverHandler {
        id: hoverHandler
        target: twoPointInteraction
        enabled: false
    }

    QQ.Component {
        id: defaultGuideItemComponent
        NorthArrowItem {
            anchors.fill: parent
            visible: false
            parent: twoPointInteraction
        }
    }

    QQ.Loader {
        id: guideItemLoader
        active: true
        anchors.fill: parent
        sourceComponent: twoPointInteraction.guideItemComponent !== null ?
                             twoPointInteraction.guideItemComponent :
                             defaultGuideItemComponent
        onItemChanged: {
            if (item) {
                item.visible = false
                item.parent = twoPointInteraction
            }
        }
    }

    TwoPointProjector {
        id: guideArrowProjector
        target: guideItemLoader.item
        camera: twoPointInteraction.turnTableInteraction.camera
        modelMatrix: twoPointInteraction.modelMatrix
        enabled: false
    }

    RowLayoutPanel {
        id: valuePanel
        visible: false

        Text {
            text: twoPointInteraction.panelLabel
            Layout.alignment: Qt.AlignVCenter
        }

        ClickNumberInput {
            id: valueInput
            objectName: "valueInput"
            visible: !twoPointInteraction.hasCustomValueEntry
            Layout.alignment: Qt.AlignVCenter
            onFinishedEditting: (newText) => {
                twoPointInteraction._userValue = Number(newText)
                text = twoPointInteraction._userValue.toFixed(1)
            }
        }

        QQ.Loader {
            id: valueEntryLoader
            visible: twoPointInteraction.hasCustomValueEntry
            active: twoPointInteraction.hasCustomValueEntry
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: twoPointInteraction.valueEntryComponent
        }

        QC.Button {
            id: valueApplyButton
            objectName: "apply"
            text: "Apply"
            Layout.alignment: Qt.AlignVCenter
            onClicked: {
                applyMeasurement()
                finish()
            }
        }
    }

    HelpBox {
        id: helpBox
        text: twoPointInteraction.firstHelpText
        visible: twoPointInteraction.visible
    }

    onDeactivated: state = ""

    QQ.Keys.onEscapePressed: finish()

    states: [
        QQ.State {
            name: ""
            // QQ.PropertyChanges {
            //     hoverHandler.enabled: false
            //     hoverHandler.onPointChanged: function() {}
            //     guideArrow.visible: false
            //     valuePanel.visible: false
            //     helpBox.text: twoPointInteraction.firstHelpText
            //     valueInput.text: twoPointInteraction._formatValue(twoPointInteraction.defaultUserValue)
            //     tapHandler.onTapped: function(eventPoint, button) {
            //         handleFirstTap(eventPoint)
            //     }
            // }

            QQ.StateChangeScript {
                script: {
                    twoPointInteraction._firstPick = null
                    twoPointInteraction._firstScreenPoint = Qt.point(0, 0)
                    twoPointInteraction._secondScreenPoint = Qt.point(0, 0)
                    twoPointInteraction._secondPoint = Qt.vector3d(0, 0, 0)
                    twoPointInteraction._measuredValue = 0.0
                    twoPointInteraction._userValue = twoPointInteraction.defaultUserValue
                    guideArrowProjector.enabled = false
                    guideArrowProjector.p1World = Qt.vector3d(0, 0, 0)
                    guideArrowProjector.p2World = Qt.vector3d(0, 0, 0)
                    twoPointInteraction._setGuideVisible(false)
                    twoPointInteraction._updateDefaultValueInput(twoPointInteraction.defaultUserValue)
                }
            }
        },
        QQ.State {
            name: "AwaitSecondPick"
            QQ.PropertyChanges {
                hoverHandler.enabled: true
                hoverHandler.onPointChanged: function(event) {
                    twoPointInteraction._updateGuideHover(hoverHandler.point.position)
                }
                valuePanel.visible: false
                helpBox.text: twoPointInteraction.secondHelpText
                tapHandler.onTapped: function(eventPoint, button) {
                    handleSecondTap(eventPoint)
                }
            }

            QQ.StateChangeScript {
                script: {
                    twoPointInteraction._setGuideVisible(true)
                    twoPointInteraction._setGuideScreenPoints(twoPointInteraction._firstScreenPoint,
                                                              twoPointInteraction._firstScreenPoint)
                }
            }
        },
        QQ.State {
            name: "AwaitAdjust"
            QQ.PropertyChanges {
                hoverHandler.enabled: false
                hoverHandler.onPointChanged: function() {}
                valuePanel.visible: twoPointInteraction.showAdjustmentPanel
                helpBox.text: twoPointInteraction.adjustHelpText
                tapHandler.onTapped: function(eventPoint, button) {
                    handleFirstTap(eventPoint)
                }
            }
            QQ.StateChangeScript {
                script: {
                    if (twoPointInteraction.showAdjustmentPanel && twoPointInteraction._hasFirstPoint) {
                        setPanelPosition(twoPointInteraction._secondScreenPoint)
                    }
                    twoPointInteraction._setGuideVisible(true)
                }
            }
        }
    ]
}
