import QtQuick 1.0
import QtDesktop 0.1 as Destkop
import Cavewhere 1.0
Item {
    property NoteTransform noteTransform

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

            LabelWithHelp {
                id: unitLabelId
                helpArea: unitsHelpAreaId
                text: "Units"
            }

            Row {
                UnitInput {
                    unitModel: noteTransform.scaleNumerator.unitNames
                    unit: noteTransform.scaleNumerator.unit
                    onUnitChanged: noteTransform.scaleNumerator.unit = unit
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

        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter

            LabelWithHelp {
                id: pageResLabel
                helpArea: unitsHelpAreaId
                text: "Page Resolution"
            }

            Row {
                ClickTextInput {
                    id: pageResInput
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
