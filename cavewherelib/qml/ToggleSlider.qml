/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
QQ.Item {
    id: toggleSliderId

    property int sliderRange: greenBackgroundId.width - sliderButtonId.width
    property bool isLeft: sliderPos <= 0.0
    property bool isRight: sliderPos >= 1.0
    property bool setLeft
    property bool setRight
    property real sliderPos: sliderButtonId.x / sliderRange //From 0 to 1.0

    property alias leftText: leftTextId.text
    property alias rightText: rightTextId.text

    width: sliderButtonId.width + rightTextId.width + 8
    height: greenBackgroundId.height

    onSetLeftChanged: {
        if(setLeft) {
            sliderButtonId.x = 0;
        }
    }

    onSetRightChanged: {
        if(setRight) {
            sliderButtonId.x = sliderPos
        }
    }

    QQ.MouseArea {
        anchors.fill: parent

        onPressed: {
            if(sliderButtonId.x == 0) {
                sliderButtonId.x = toggleSliderId.sliderRange
            } else {
                sliderButtonId.x = 0
            }
        }
    }

    QQ.Image {
        id: greenBackgroundId
//        source: "qrc:icons/toggleSlider/greenSliderBackground.png"
        source: "qrc:icons/toggleSlider/graySliderBackGround.png"
        anchors.left: parent.left
        anchors.right: parent.right
        height: 20


    }

//    QQ.Image {
//        id: blueBackgroundId
////        source: "qrc:icons/toggleSlider/blueSliderBackground.png"
//        source: "qrc:icons/toggleSlider/graySliderBackGround.png"
//        anchors.left: parent.left
//        anchors.right: parent.right
//        opacity: sliderButtonId.x / sliderRange
//    }

    QQ.Item {
        id: leftClipBox
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: sliderButtonId.left
        anchors.left: parent.left

        clip: true

        Text {
            id: leftTextId
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 4
            opacity: sliderButtonId.x / toggleSliderId.sliderRange
        }
    }

    QQ.Item {
        id: rightClipBox
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: sliderButtonId.right

        clip: true

        Text {
            id: rightTextId
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 4
            opacity:  1.0 - (sliderButtonId.x / toggleSliderId.sliderRange)
        }
    }

    QQ.Image {
        id: sliderButtonId
        source: "qrc:icons/toggleSlider/buttonSlider.png"
        height: greenBackgroundId.height
        width: 20

        QQ.Behavior on x {
            id: behaviorId
            QQ.NumberAnimation {}
        }

        QQ.MouseArea {
            property int firstPointX: 0

            anchors.fill: parent

            onPressed: function(mouse) {
                firstPointX = mapToItem(toggleSliderId, mouse.x, 0).x
                behaviorId.enabled = false
            }

            onPositionChanged: function(mouse) {
                var currentPosition = mapToItem(toggleSliderId, mouse.x, 0).x;
                var delta = currentPosition - firstPointX;
                sliderButtonId.x = Math.min(toggleSliderId.sliderRange,
                                             Math.max(0, sliderButtonId.x + delta));
                firstPointX = currentPosition
            }

            onReleased: {
                behaviorId.enabled = true
                var locationPercent = sliderButtonId.x / toggleSliderId.sliderRange;

                if(locationPercent > 0.5) {
                    sliderButtonId.x = toggleSliderId.sliderRange
                } else {
                    sliderButtonId.x = 0
                }
            }
        }
    }




}
