import QtQuick 1.0
import QtDesktop 0.1 as Destkop
import Cavewhere 1.0

Item {
    property NoteTransform noteTransform
    property Note note

    height: childrenRect.height

    Row {
        id: inputRow
        spacing: 5

        anchors.left: parent.left
        anchors.right: parent.right

        Button {
            id: setLength

            width: 24
        }

        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter

            LabelWithHelp {
                id: scaleLabelId
                helpArea: scaleHelpAreaId
                text: "Scale"
            }

            Row {
                ClickTextInput {
                    id: paperUnits
                    text: noteTransform.scaleNumerator.value.toFixed(0)
                    onFinishedEditting: noteTransform.scaleNumerator.value = newText
                }

                Text {
                    text: ":"
                }

                ClickTextInput {
                    id: caveUnits
                    text: noteTransform.scaleDenominator.value.toFixed(0)
                    onFinishedEditting: noteTransform.scaleDenominator.value = newText
                }
            }
        }

        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter

//            LabelWithHelp {
//                id: unitLabelId
//                helpArea: unitsHelpAreaId
//                text: "Units"
//            }

            Row {
                UnitInput {
                    unitModel: noteTransform.scaleNumerator.unitNames
                    unit: noteTransform.scaleNumerator.unit
                    onUnitChanged: {
                        console.log("scaleNumerator: unit" + unit)
                        noteTransform.scaleNumerator.unit = unit
                    }
                }

                Text {
                    text: ":"
                }

                UnitInput {
                    unitModel: noteTransform.scaleDenominator.unitNames
                    unit: noteTransform.scaleDenominator.unit
                    onUnitChanged: noteTransform.scaleDenominator.unit = unit
                }
            }
        }


        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: !((noteTransform.scaleDenominator.unit === Length.Unitless ||
                     noteTransform.scaleNumerator.unit === Length.Unitless) ||
                     noteTransform.scaleDenominator.unit === noteTransform.scaleNumerator.unit)
            text: "= 1:" + new Number((1 / noteTransform.scale).toFixed(1)).toString()
        }

        Text {
            color: "red"
            anchors.verticalCenter: parent.verticalCenter
            visible: (noteTransform.scaleDenominator.unit === Length.Unitless ||
                      noteTransform.scaleNumerator.unit === Length.Unitless) &&
                     !(noteTransform.scaleDenominator.unit === Length.Unitless &&
                      noteTransform.scaleNumerator.unit === Length.Unitless)
            text: "Weird scaling units"
            font.italic: true
            font.bold: true
        }

//        Row {
//            spacing: 3
//            anchors.verticalCenter: parent.verticalCenter

//            LabelWithHelp {
//                id: pageResLabel
//                helpArea: unitsHelpAreaId
//                text: "Resolution"
//            }

//            Row {
//                ClickTextInput {
//                    id: pageResInput
//                    text: noteTransform.scaleDenominator.value.toFixed(0)
//                    onFinishedEditting: noteTransform.scaleDenominator.value = newText
//                }

//                UnitInput {
//                    unitModel: [
//                        "dpi",
//                        "dpm"
//                    ]
//                    unit: 0
//                    onUnitChanged: {

//                    }
//                }
//            }
//        }
    }

    HelpArea {
        id: scaleHelpAreaId

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: inputRow.bottom
        anchors.topMargin: 4
    }

    HelpArea {
        id: unitsHelpAreaId

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: scaleHelpAreaId.bottom
        anchors.topMargin: 4
    }
}
