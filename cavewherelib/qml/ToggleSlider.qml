/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A wide switch whose thumb can rest anywhere along the track while it is
// dragged, so sliderPos can scrub a transition. It draws with the tokens of the
// style's Switch.
QQ.Item {
    id: toggleSliderId

    property int sliderRange: trackId.width - sliderButtonId.width
    property bool isLeft: sliderPos <= 0.0
    property bool isRight: sliderPos >= 1.0
    property bool setLeft
    property bool setRight
    property real sliderPos: sliderButtonId.x / sliderRange //From 0 to 1.0

    property alias leftText: leftTextId.text
    property alias rightText: rightTextId.text

    readonly property int textMargin: Theme.delegatePadding

    implicitWidth: Math.max(Theme.switchWidth,
                            sliderButtonId.width
                            + Math.max(leftTextId.implicitWidth, rightTextId.implicitWidth)
                            + 2 * toggleSliderId.textMargin)
    implicitHeight: Theme.switchHeight

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

    QQ.Rectangle {
        id: trackId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: Theme.switchHeight
        radius: height / 2
        color: Theme.track
        opacity: toggleSliderId.enabled ? 1 : Theme.disabledOpacity
    }

    QQ.Item {
        id: leftClipBox
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: sliderButtonId.left
        anchors.left: parent.left

        clip: true

        QC.Label {
            id: leftTextId
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: toggleSliderId.textMargin
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

        QC.Label {
            id: rightTextId
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: toggleSliderId.textMargin
            opacity:  1.0 - (sliderButtonId.x / toggleSliderId.sliderRange)
        }
    }

    // The thumb's slot along the track; its x is the slider's state
    QQ.Item {
        id: sliderButtonId
        objectName: "toggleSliderThumb"
        width: trackId.height
        height: trackId.height
        anchors.verticalCenter: trackId.verticalCenter

        QQ.Behavior on x {
            id: behaviorId
            QQ.NumberAnimation {}
        }

        QQ.Rectangle {
            anchors.fill: parent
            anchors.margins: Theme.switchThumbInset
            radius: width / 2
            color: Theme.background
            border.width: 1
            border.color: Theme.controlBorder
            opacity: toggleSliderId.enabled ? 1 : Theme.disabledOpacity
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
