import QtQuick 1.1

Text {
    id: label

    property HelpArea helpArea

//    font.bold: true
    font.underline: false

    MouseArea {
        id: textMouseArea
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            helpArea.visible = !helpArea.visible
        }
    }

    states: [
        State {
            name: "HOVER"
            when: textMouseArea.containsMouse

            PropertyChanges {
                target: label
                color: "blue"
                font.underline: true
            }
        }
    ]
}








