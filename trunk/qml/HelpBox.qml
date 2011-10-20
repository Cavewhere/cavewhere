import QtQuick 1.0

ShadowRectangle {
    id: helpBox

    property alias text: textId.text

    width: textId.width + 10
    height: textId.height + 10

    anchors.bottom: parent.bottom
    anchors.bottomMargin: 20
    anchors.horizontalCenter: parent.horizontalCenter

    color: "#85c1f4"
    radius: 5


    Text {
        id: textId
        anchors.centerIn: parent

        font.pointSize: 14
        horizontalAlignment: Text.AlignHCenter
    }

    states: [
        State {
            name: "VISIBLE"
            when: visible
        }
    ]

    transitions: [
        Transition {
            to: "VISIBLE"
            NumberAnimation {
                target: helpBox;
                property: "opacity";
                from: 0.0
                to: 1.0;
                duration: 200
            }
        }
    ]

}
