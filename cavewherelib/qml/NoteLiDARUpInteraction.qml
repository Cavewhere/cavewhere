
/**************************************************************************
**
**    NoteLiDARUpInteraction.qml
**    Interactive tool for aligning LiDAR up rotation via two picked points.
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

NoteLiDARTwoPointInteraction {
    id: lidarUpInteraction
    objectName: "noteLiDARUpInteraction"

    guideItemComponent: upGuideComponent
    firstHelpText: "<b>Click</b> first up reference point"
    secondHelpText: "<b>Click</b> second point to set up"
    defaultUserValue: 90.0
    showAdjustmentPanel: false
    autoApplyAfterSecondPick: true
    valueValidator: ClinoValidator {}

    function measurementCalculator(firstPoint, secondPoint) {
        const noteTransform = note ? note.noteTransformation : null
        if (!noteTransform) {
            return defaultUserValue
        }
        return noteTransform.calculateVerticalAngle(firstPoint, secondPoint)
    }

    function applyHandler(context) {
        const noteTransform = note ? note.noteTransformation : null
        if (!noteTransform || !context.firstPoint || !context.secondPoint) {
            return
        }
        const upQuaternion = noteTransform.calculateUpQuaternion(context.firstPoint,
                                                                context.secondPoint)
        noteTransform.upMode = NoteLiDARTransformation.UpMode.Custom
        noteTransform.upCustom = upQuaternion
        noteTransform.upSign = 1.0
    }

    QQ.Component {
        id: upGuideComponent
        NorthArrowItem {
            anchors.fill: parent
            visible: false
        }
    }
}
