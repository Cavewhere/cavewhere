/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

Item {

    property NoteTransform noteTransform
    property HelpArea northUpHelp
    property alias enable: setNorthButton.visible

    signal northUpInteractionActivated()

    height: childrenRect.height
    width: row.width

    Row {
        id: row
        spacing: 5

        Button {
            id: setNorthButton

            width: 24

            onClicked: northUpInteractionActivated()
        }

        LabelWithHelp {
            id: labelId
            helpArea: northUpHelp
            text: qsTr("North")
            anchors.verticalCenter: parent.verticalCenter
        }

        ClickTextInput {
            id: clickInput
            readOnly: !enable
            text: qsTr("")
            onFinishedEditting: ( { } )
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: unit
            textFormat: Text.RichText
            text: qsTr("&deg")
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    states: [
        State {
            when: noteTransform !== null

            PropertyChanges {
                target: clickInput
                text: noteTransform.northUp.toFixed(2)
                onFinishedEditting: noteTransform.northUp = newText
            }
        }

    ]


}
