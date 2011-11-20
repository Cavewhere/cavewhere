import QtQuick 1.0
import QtDesktop 0.1 as Destkop
import Cavewhere 1.0

Item {
    property NoteTransform noteTransform
    property Note note

    height: childrenRect.height
    width: inputRow.width

    Row {
        id: inputRow
        spacing: 5

//        anchors.left: parent.left
//        anchors.right: parent.right

        Button {
            id: setLength
            anchors.verticalCenter: parent.verticalCenter
            width: 24
        }

        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter

            LabelWithHelp {
                id: scaleLabelId
                helpArea: scaleHelpAreaId
                text: "Scale"
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {

                anchors.verticalCenter: parent.verticalCenter
                radius: 5

                width: childrenRect.width + columnOnPaper.x * 2.0
                height: childrenRect.height

                Column {
                    id: columnOnPaper
                    anchors.verticalCenter: parent.verticalCenter
                    x: 2.5

                    Text {
                        id: onPaperId
                        text: "On Paper"
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter

                        ClickTextInput {
                            id: paperUnits
                            text: noteTransform.scaleNumerator.value.toFixed(0)
                            onFinishedEditting: noteTransform.scaleNumerator.value = newText
                        }

                        UnitInput {
                            unitModel: noteTransform.scaleNumerator.unitNames
                            unit: noteTransform.scaleNumerator.unit
                            onUnitChanged: noteTransform.scaleNumerator.unit = unit
                        }

                    }
                }

            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "="
            }

            Rectangle {

                anchors.verticalCenter: parent.verticalCenter
                radius: 5

                width: childrenRect.width + columnInCave.x * 2.0
                height: childrenRect.height

                Column {
                    id: columnInCave
                    x: 2.5
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: inCaveId
                        text: "In Cave"
                    }


                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter

                        ClickTextInput {
                            id: caveUnits
                            text: noteTransform.scaleDenominator.value.toFixed(0)
                            onFinishedEditting: noteTransform.scaleDenominator.value = newText
                        }

                        UnitInput {
                            unitModel: noteTransform.scaleDenominator.unitNames
                            unit: noteTransform.scaleDenominator.unit
                            onUnitChanged: noteTransform.scaleDenominator.unit = unit
                        }
                    }
                }
            }

//            Row {


//                ClickTextInput {
//                    id: paperUnits
//                    text: noteTransform.scaleNumerator.value.toFixed(0)
//                    onFinishedEditting: noteTransform.scaleNumerator.value = newText
//                }

//                Text {
//                    text: ":"
//                }


//            }
        }

//        Row {
//            spacing: 3
//            anchors.verticalCenter: parent.verticalCenter

////            LabelWithHelp {
////                id: unitLabelId
////                helpArea: unitsHelpAreaId
////                text: "Units"
////            }

//            Row {


//                Text {
//                    text: ":"
//                }


//            }
//        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "="
        }


        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: !errorText.visible
//            visible: !((noteTransform.scaleDenominator.unit === Length.Unitless ||
//                     noteTransform.scaleNumerator.unit === Length.Unitless) ||
//                     noteTransform.scaleDenominator.unit === noteTransform.scaleNumerator.unit)
            text: "1:" + new Number((1 / noteTransform.scale).toFixed(1)).toString()
        }

        Text {
            id: errorText
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

        anchors.left: inputRow.left
        anchors.right: inputRow.right
        anchors.top: inputRow.bottom
        anchors.topMargin: 4
    }

}
