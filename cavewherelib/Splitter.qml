/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

QQ.Item {
    id: splitter
    //    anchors.bottom: parent.bottom
    //    anchors.top: parent.top
    //anchors.left: dataSideBar.right
    anchors.leftMargin: -width / 2

    width:  10
    property variant resizeObject
    property string expandDirection: "right"

    z:1

    QQ.MouseArea {
        id: splitterMouseArea
        anchors.fill: parent

        property int lastMousePosition;

        onPressed: {
            lastMousePosition = mapToItem(null, mouseX, 0).x;
        }

        states: [
            QQ.State {
                when: splitterMouseArea.pressed
                QQ.PropertyChanges {
                    splitterMouseArea {

                        onPositionChanged: {
                            //Change the databar width
                            var mappedX = mapToItem(null, mouseX, 0).x;

                            switch(expandDirection) {
                                case "left":
                                splitter.resizeObject.width -=  mappedX - lastMousePosition
                                break;
                                case "right":
                                splitter.resizeObject.width +=  mappedX - lastMousePosition
                                break;
                            }


                            lastMousePosition = mappedX
                        }
                    }
                }
            }
        ]
    }
}
