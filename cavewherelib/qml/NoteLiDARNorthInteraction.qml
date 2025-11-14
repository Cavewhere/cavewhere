/**************************************************************************
**
**    NoteLiDARNorthInteraction.qml
**    Interactive tool for aligning LiDAR north via two picked points.
**
**************************************************************************/

import cavewherelib

NoteLiDARTwoPointInteraction {
    id: lidarNorthInteraction
    objectName: "noteLiDARNorthInteraction"

    panelLabel: "Arrow's Azimuth"
    firstHelpText: "<b>Click</b> the first point"
    secondHelpText: "<b>Click</b> the second point"
    adjustHelpText: "What is azimuth of the arrow?"
    defaultUserValue: 0.0
    valueInputObjectName: "azimuthInput"
    valueValidator: CompassValidator {}

    measurementCalculator: (firstPoint, secondPoint) => {
        const noteTransform = note ? note.noteTransformation : null
        if (!noteTransform) {
            return NaN
        }
        let measured = noteTransform.calculateNorth(firstPoint, secondPoint)
        if (isNaN(measured)) {
            measured = noteTransform.northUp
        }
        return measured
    }

    applyHandler: (context) => {
        const noteTransform = note ? note.noteTransformation : null
        if (!noteTransform) {
            return
        }

        noteTransform.northUp = noteTransform.wrapDegrees360(context.measuredValue - context.userValue)
    }
}
