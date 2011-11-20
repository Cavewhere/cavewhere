import QtQuick 1.0
import Cavewhere 1.0

Item {

    property NoteTransform noteTransform
    property HelpArea northUpHelp

    signal northUpInteractionActivated()

    height: childrenRect.height
    width: row.width

    Row {
        id: row
        spacing: 5

        Button {
            id: setNorthButton

            width: 24

            onClicked: {
                interactionManager.active(northInteraction)
            }
        }

        LabelWithHelp {
            id: labelId
            helpArea: northUpHelp
            text: "North"
            anchors.verticalCenter: parent.verticalCenter
        }

        ClickTextInput {
            id: clickInput
            color: "blue"
            text: noteTransform.northUp.toFixed(2)
            onFinishedEditting: noteTransform.northUp = newText
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: unit
            textFormat: Text.RichText
            text: "&deg"
            anchors.verticalCenter: parent.verticalCenter
        }
    }


}
