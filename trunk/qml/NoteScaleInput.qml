import QtQuick 1.0

Item {

    property NoteTransform noteTransform

    Row {
        Button {
            id: setLength

            width: 24
        }

        LabelWithHelp {
            id: labelId
            helpArea: helpAreaId
            text: "Scale"
            anchors.verticalCenter: parent.verticalCenter
        }

        ClickTextInput {
            id: paperUnits
            text: noteTransform.northUp.toFixed(0)
            onFinishedEditting: noteTransform.northUp = newText
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: ":"
        }

        ClickTextInput {
            id: paperUnits
            text: noteTransform.northUp.toFixed(0)
            onFinishedEditting: noteTransform.northUp = newText
            anchors.verticalCenter: parent.verticalCenter
        }


    }


}
