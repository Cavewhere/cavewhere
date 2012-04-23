import QtQuick 2.0

Rectangle {
    id: helpArea

    property alias text: helpText.text
    property alias pointSize: helpText.font.pointSize
    property alias imageSource: icon.source
    property bool animationToVisible: true
    property bool animationToInvisible: true

    color: "lightgray"
    height: 0
    radius: 5
    clip: true

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

    Image {
        id: icon
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.verticalCenter: parent.verticalCenter
//        source: "qrc:icons/Information20x20.png"
        width: sourceSize.width
        height: sourceSize.height

    }

    Text {
        id: helpText

        //anchors.top: parent.top
        anchors.left: icon.right
        anchors.right: parent.right
        anchors.margins: 5
        anchors.verticalCenter: parent.verticalCenter

        textFormat: Text.RichText
        wrapMode: Text.WordWrap
        font.pointSize: 9

        text: "No documentation"
    }

    QtObject {
        id: privateData
        property bool inTransition: false
    }

    states: [
        State {
            name: "VISIBLE"

            PropertyChanges {
                target: helpArea
                height: helpText.height + 10
            }
        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "VISIBLE"

            SequentialAnimation {

                PropertyAction { target: privateData; property: "inTransition"; value: true }
                PropertyAnimation {
                    target: helpArea
                    property: "height"
                    from: 0
                    to: helpText.height + 10
                    duration: animationToVisible ? 200 : 0
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
                    target: helpArea
                    property: "height"
                    to: 0
                    duration: animationToInvisible ? 200 : 0
                }

                PropertyAction { target: helpArea; property: "visible"; value: false }
                PropertyAction { target: privateData; property: "inTransition"; value: false }
            }
        }
    ]
}
