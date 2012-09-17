import QtQuick 2.0

Item {
    id: splitter
//    anchors.bottom: parent.bottom
//    anchors.top: parent.top
    //anchors.left: dataSideBar.right
    anchors.leftMargin: -width / 2

    width:  10
    property variant resizeObject
    property string expandDirection: "right"

    z:1

    MouseArea {
        id: splitterMouseArea
        anchors.fill: parent

        property int lastMousePosition;

        onPressed: {
            lastMousePosition = mapToItem(null, mouseX, 0).x;
        }

        states: [
            State {
                when: splitterMouseArea.pressed
                PropertyChanges {
                    target: splitterMouseArea

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
        ]
    }
}
