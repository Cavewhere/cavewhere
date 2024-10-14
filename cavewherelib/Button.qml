/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts 1.1
// import QtGraphicalEffects

QQ.Rectangle {
    id: button
    property alias text:  buttonText.text
    property alias iconSource: icon.source
    property bool checkable:     false
    property bool troggled: false
    property bool iconOnTheLeft:  true
    property alias iconSize: icon.sourceSize
    property bool hasText:  text.length > 0
    // property int buttonMargin:  container.anchors.margins
    property bool mousePressed: false
    property bool enabled: true
    property bool globalClickToUncheck: false //This is useful, if you're connect the button to a popup

    signal clicked();

    implicitWidth:  buttonLayoutId.implicitWidth + 6
    implicitHeight: buttonLayoutId.implicitHeight + 6

    radius: 4

    border.width: 1
    border.color: "#7A7A7A"

    gradient: QQ.Gradient {
        QQ.GradientStop {id: stop1; position:0.0; color:"#FAFAFA" }
        QQ.GradientStop {id: stop2; position:1.0; color:"#D3D3D3" }
    }

    state: "enabledState"

    RowLayout {
        id: buttonLayoutId

        anchors.centerIn: parent
        visible: false;

        QQ.Image {
            id: icon
            visible: button.iconOnTheLeft
        }

        Text {
            id: buttonText
            visible: button.hasText
            color: "black"
        }

        QQ.Image {
            id: iconRightId
            source: icon.source
            sourceSize: icon.sourceSize
            visible: !button.iconOnTheLeft
        }
    }

    // Colorize {
    //     id: colorizeEffect
    //     source: buttonLayoutId
    //     anchors.fill: buttonLayoutId

    //     hue: 1.0
    //     saturation: 0.0
    //     lightness: 0.5
    // }

    QQ.MouseArea {
        id: mouseArea
        anchors.fill: parent

        enabled: true
        hoverEnabled: true

        onClicked: {
            if(button.checkable) {
                button.troggled = !button.troggled

                // if(button.globalClickToUncheck) {
                //     button.globalDialogHandler.enabled = true
                // }
            }
            button.clicked();
        }

        onPressed: {
            button.mousePressed = true
        }

        onReleased: {
            button.mousePressed = false
        }

        // QQ.Connections {
        //     target: troggled ? globalDialogHandler : null

        //     onClicked: {
        //         console.log("--------------- Clicked! ----------");
        //         troggled = !troggled
        //         globalDialogHandler.enabled = false
        //     }
        // }
    }


//     states: [
//         QQ.State {
//             name: "hover"; when: mouseArea.containsMouse && !button.mousePressed && button.enabled && !button.troggled
//             extend: "enabledState"
//             QQ.PropertyChanges { stop1.color: "#E6E6E6" }
//             QQ.PropertyChanges { stop2.color: "#B3B3B3" }
//         },

//         QQ.State {
//             name: "mousePressedState";
//             extend: "enabledState"
//             when: (mouseArea.containsMouse && button.mousePressed && button.enabled) || button.troggled
//             QQ.PropertyChanges { stop1.color: "#B3B3B3" }
//             QQ.PropertyChanges { stop2.color: "#B3B3B3" }
//         },

// //        QQ.State {
// //            name: "toggledState"
// //            extend: "enabledState"
// //            when: troggled
// //            QQ.PropertyChanges { target: stop1; color: "#D3D3D3" }
// //            QQ.PropertyChanges { target: stop2; color: "#FAFAFA" }
// //        },

//         QQ.State {
//             name: "enabledState"
//             when: button.enabled
//             QQ.PropertyChanges { stop1.color: "#FAFAFA" }
//             QQ.PropertyChanges { stop2.color: "#D3D3D3" }
//             QQ.PropertyChanges {
//                 mouseArea.enabled: true
//             }
//             // QQ.PropertyChanges {
//             //     target: colorizeEffect
//             //     visible: false
//             // }
//             QQ.PropertyChanges {
//                 buttonLayoutId.visible: true
//             }
//         },

//         QQ.State {
//             name: "disabledState"
//             when: !button.enabled
//             QQ.PropertyChanges {stop1.color:"#FAFAFA" }
//             QQ.PropertyChanges {stop2.color:"#D3D3D3" }
//             QQ.PropertyChanges {
//                 mouseArea.enabled: false
//             }
//         }

    // ]

}
