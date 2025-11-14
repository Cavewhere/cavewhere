
/**************************************************************************
**
**    NoteLiDARUpInteraction.qml
**    Interactive tool for aligning LiDAR up rotation via two picked points.
**
**************************************************************************/

import cavewherelib

NoteLiDARTwoPointInteraction {
    id: lidarUpInteraction
    objectName: "noteLiDARUpInteraction"

    panelLabel: "Vertical"
    firstHelpText: "<b>Click</b> first up reference point"
    secondHelpText: "<b>Click</b> second point to set up"
    adjustHelpText: "Confirm or adjust vertical angle"
    defaultUserValue: 90.0
    valueValidator: ClinoValidator {}

    // measurementCalculator: (firstPoint, secondPoint) => {
    //                            return 0.0; //Unused

    //     const noteTransform = note ? note.noteTransformation : null
    //     if (!noteTransform) {
    //         return defaultUserValue
    //     }
    //     let measured = noteTransform.calculateVerticalAngle(firstPoint, secondPoint)
    //     if (!Number.isFinite(measured)) {
    //         measured = defaultUserValue
    //     }
    //     return measured
    // }

    applyHandler: (context) => {
        const noteTransform = note ? note.noteTransformation : null
        if (!noteTransform || !context.firstPoint || !context.secondPoint) {
            return
        }
        const upQuaternion = noteTransform.calculateUpQuaternion(context.firstPoint,
                                                                context.secondPoint,
                                                                context.userValue)
        noteTransform.upMode = NoteLiDARTransformation.UpMode.Custom
        noteTransform.upCustom = upQuaternion
        noteTransform.upSign = 1.0
    }
}
