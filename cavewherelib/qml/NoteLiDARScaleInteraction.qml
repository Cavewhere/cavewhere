/**************************************************************************
**
**    NoteLiDARScaleInteraction.qml
**    Interactive LiDAR scale tool using two picked points.
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

NoteLiDARTwoPointInteraction {
    id: lidarScaleInteraction
    objectName: "noteLiDARScaleInteraction"

    required property RegionViewer viewer
    property GltfScene scene

    panelLabel: measuredValue > 0
                ? `Real length (model ${formatModelLength(measuredValue)} units)`
                : "Real length"
    firstHelpText: "<b>Click</b> the first point to define scale"
    secondHelpText: "<b>Click</b> the second point"
    adjustHelpText: "Enter the actual distance between points"
    defaultUserValue: 1.0
    modelMatrix: scene && scene.gltf ? scene.gltf.modelMatrix : Qt.matrix4x4()
    guideItemComponent: scaleGuideComponent
    valueEntryComponent: scaleValueEntryComponent

    readonly property NoteLiDARTransformation noteTransform: note ? note.noteTransformation : null

    Length {
        id: realWorldLength
        unit: Units.Meters
        value: 1.0
    }

    measurementCalculator: (firstPoint, secondPoint) => {
        return firstPoint.minus(secondPoint).length();
    }

    function formatModelLength(value) {
        if (!Number.isFinite(value) || value <= 0) {
            return ""
        }
        if (value >= 1000.0) {
            return value.toFixed(0)
        }
        if (value >= 1.0) {
            return value.toFixed(2)
        }
        return value.toFixed(3)
    }

    applyHandler: (context) => {
        if (!noteTransform || !context) {
            return
        }
        const scaleObj = noteTransform.scaleObject
        if (!scaleObj) {
            return
        }
        const measured = Number(context.measuredValue)
        const realLength = Number(realWorldLength.value)
        if (!Number.isFinite(measured) || measured <= 0.0 ||
                !Number.isFinite(realLength) || realLength <= 0.0) {
            return
        }

        const numerator = scaleObj.scaleNumerator
        const denominator = scaleObj.scaleDenominator
        if (!numerator || !denominator) {
            return
        }

        numerator.unit = Units.LengthUnitless
        numerator.value = measured
        denominator.unit = realWorldLength.unit
        denominator.value = realLength
    }

    QQ.Component {
        id: scaleGuideComponent
        ScaleLengthItem {
            anchors.fill: parent
            visible: false
        }
    }

    QQ.Component {
        id: scaleValueEntryComponent
        UnitValueInput {
            id: scaleLengthInput
            objectName: "scaleLengthInput"
            unitValue: realWorldLength
            defaultUnit: Units.Meters
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
