import QtQuick 1.0

Rectangle {
    id: helpArea

    property alias text: helpText.text

    color: "gray"
    height: 0
    radius: 5

    visible: false

    onVisibleChanged: {
        if(!privateData.inTransition) {
            if(visible) {
                state = "VISIBLE"
            } else {
                state = ""
            }
        }
    }

    Text {
        id: helpText

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 5

        textFormat: Text.RichText
    }

    QtObject {
        id: privateData
        property bool inTransition: false
    }

    states: [
        State {
            name: "VISIBLE"

//            PropertyChanges {
//                target: helpArea
//                height: helpText.height + 10
//            }
        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "VISIBLE"

            SequentialAnimation {
                PropertyAction { target: privateData; property: "inTransition"; value: true }

                NumberAnimation {
                    target: object
                    property: "height"
                    to: helpText.height + 10
                    duration: 200
                }

                PropertyAction { target: privateData; property: "inTransition"; value: false }
            }
        },

        Transition {
            from: "VISIBLE"
            to: ""

            SequentialAnimation {
                PropertyAction { target: privateData; property: "inTransition"; value: true }

                PropertyAction { target: helpArea; property: "visible"; value: true }

                NumberAnimation {
                    target: object
                    property: "height"
                    to: 0
                    duration: 200
                }

                PropertyAction { target: helpArea; property: "visible"; value: false }

                PropertyAction { target: privateData; property: "inTransition"; value: false }
            }
        }
    ]
}
