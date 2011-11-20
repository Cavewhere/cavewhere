import QtQuick 1.0
import Cavewhere 1.0

Item {

    property NoteTransform noteTransform

    signal northUpInteractionActivated()

    height: childrenRect.height
    width: row.width

    Row {
        id: row
        spacing: 5

//        anchors.left: parent.left
//        anchors.right: parent.right

        Button {
            id: setNorthButton

            width: 24

            onClicked: {
                interactionManager.active(northInteraction)
            }
        }

        LabelWithHelp {
            id: labelId
            helpArea: helpAreaId
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

    HelpArea {
        id: helpAreaId
        anchors.left: row.left
        anchors.right: row.right
        anchors.top: row.bottom
        anchors.topMargin: 4

        text: "You can set the direction of <b>north</b> relative to page for a scrap.
        Cavewhere only uses <b>north</b> to help you automatically label stations."
    }
}
