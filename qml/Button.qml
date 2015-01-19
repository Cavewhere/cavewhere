/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0

Rectangle {
    id: button
    property alias text:  buttonText.text
    property alias iconSource: icon.source
    property bool checkable:     false
    property bool troggled: false
    property bool iconOnTheLeft:  true
    property alias iconSize: icon.sourceSize
    property bool hasText:  text.length > 0
    property int buttonMargin:  container.anchors.margins
    property bool mousePressed: false
    property bool enabled: true

    signal clicked();

    implicitWidth:  buttonLayoutId.implicitWidth + 6
    implicitHeight: buttonLayoutId.implicitHeight + 6

    radius: 4

    border.width: 1
    border.color: "#7A7A7A"

    gradient: Gradient {
        GradientStop {id: stop1; position:0.0; color:"#FAFAFA" }
        GradientStop {id: stop2; position:1.0; color:"#D3D3D3" }
    }

    state: "enabledState"

    RowLayout {
        id: buttonLayoutId

        anchors.centerIn: parent
        visible: false;

        Image {
            id: icon
            visible: iconOnTheLeft
        }

        Text {
            id: buttonText
            visible: hasText
            color: "black"
        }

        Image {
            id: iconRightId
            source: icon.source
            sourceSize: icon.sourceSize
            visible: !iconOnTheLeft
        }
    }

    Colorize {
        id: colorizeEffect
        source: buttonLayoutId
        anchors.fill: buttonLayoutId

        hue: 1.0
        saturation: 0.0
        lightness: 0.5
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent

        enabled: true
        hoverEnabled: true

        onClicked: {
            if(checkable) { troggled = !troggled }
            button.clicked();
        }

        onPressed: {
            mousePressed = true
        }

        onReleased: {
            mousePressed = false
        }
    }

    states: [
        State {
            name: "hover"; when: mouseArea.containsMouse && !mousePressed && enabled
            extend: "enabledState"
            PropertyChanges { target: stop1; color: "#E6E6E6" }
            PropertyChanges { target: stop2; color: "#B3B3B3" }
        },

        State {
            name: "mousePressedState";
            extend: "enabledState"
            when: mouseArea.containsMouse && mousePressed && enabled
            PropertyChanges { target: stop1; color: "#B3B3B3" }
            PropertyChanges { target: stop2; color: "#B3B3B3" }
        },

        State {
            name: "enabledState"
            when: enabled
            PropertyChanges { target: stop1; color: "#FAFAFA" }
            PropertyChanges { target: stop2; color: "#D3D3D3" }
            PropertyChanges {
                target: mouseArea
                enabled: true
            }
            PropertyChanges {
                target: colorizeEffect
                visible: false
            }
            PropertyChanges {
                target: buttonLayoutId
                visible: true
            }
        },

        State {
            name: "disabledState"
            when: !enabled
            PropertyChanges {target: stop1; color:"#FAFAFA" }
            PropertyChanges {target: stop2; color:"#D3D3D3" }
            PropertyChanges {
                target: mouseArea
                enabled: false
            }
        }

    ]

}
