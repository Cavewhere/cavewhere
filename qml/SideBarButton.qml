import QtQuick 2.0

Rectangle {
    id: button
    width: 100
    height: 80
    color: "#00000000"
    clip: true;

    property alias text: textLabel.text;
    property alias image: icon.source;
    property bool troggled: false;
    property int buttonIndex;

    anchors.left: parent.left;
    anchors.right: parent.right

    //Called when troggle is true
    signal buttonIsTroggled()

    Text {
        id: textLabel
        color: "#ffffff"
        text: "text"
        smooth: true
        style: Text.Sunken
        anchors.top: icon.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.topMargin: 3
        anchors.rightMargin: 0
        anchors.leftMargin: 0
        anchors.bottomMargin: 3
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 14
    }

    Image {
        id: icon
        width: 94
        height: 52
        anchors.top: parent.top
        anchors.topMargin: 3
        anchors.right: parent.right
        anchors.rightMargin: 3
        anchors.left: parent.left
        anchors.leftMargin: 3
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Rectangle {
        id: hoverBackground
        x: -parent.height / 2
        y: parent.height / 2
        width: parent.height
        height: parent.width
        z: -2
        gradient: Gradient {
            GradientStop {
                id: gradientstop1
                position: 0
                color: "#00d1d1d1"
            }

            GradientStop {
                id: gradientstop2
                position: 0.47
                color: "#96b5b5b5"
            }

            GradientStop {
                id: gradientstop3
                position: 1
                color: "#00000000"
            }
        }
        transformOrigin: Item.Top
        rotation: -90
        visible: false
        opacity: 1


    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent;
        hoverEnabled: true;

        onEntered: button.state = "hoverState"
        onExited: button.state = ""
        onClicked: buttonIsTroggled();
    }

    Rectangle {
        id: borderRectangle
        width: parent.width + 4
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: "#00ffffff"
        anchors.horizontalCenter: parent.horizontalCenter
        border.color: "#00000000"
        border.width: 0
        z: -1
        opacity: 1
    }

    states: [
        State {
            name: "toggledState"

            PropertyChanges {
                target: gradientstop1
                position: 0
                color: "#ffffff"
            }

            PropertyChanges {
                target: gradientstop2
                position: 1
                color: "#000000"
            }

            PropertyChanges {
                target: gradientstop3
                position: 1
                color: "#c8c0c0c0"
            }

            PropertyChanges {
                target: hoverBackground
                z: -1
                visible: true
            }

            PropertyChanges {
                target: textLabel
                color: "#000000"
                styleColor: "#aaaaaa"
                style: "Raised"
            }

            PropertyChanges {
                target: borderRectangle
                border.color: "#313131"
            }

            PropertyChanges {
                target: mouseArea
                onEntered: {}
                onExited: {}
            }
        },
        State {
            name: "hoverState"

            PropertyChanges {
                target: hoverBackground
                visible: true
            }

            PropertyChanges {
                target: gradientstop1
                position: 0
                color: "#00d1d1d1"
            }

            PropertyChanges {
                target: gradientstop2
                position: 0.47
                color: "#32b5b5b5"
            }

            PropertyChanges {
                target: gradientstop3
                position: 1
                color: "#00000000"
            }

            PropertyChanges {
                target: borderRectangle
                border.color: "#ffffff"
            }

        }
    ]

    onTroggledChanged: {
        if(troggled) {
            button.state = "toggledState";
        } else {
            button.state = "";
        }
    }
}
