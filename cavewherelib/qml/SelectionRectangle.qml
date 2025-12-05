import QtQuick as QQ
import cavewherelib


QQ.Rectangle {
    id: selectionRectangleId

    property int mouseBuffer: 5

    opacity: 0.30
    color: Theme.accent
    border.width: 1
    border.color: Theme.border

    QQ.MouseArea {
        id: leftSide
        width: selectionRectangleId.mouseBuffer
        anchors.horizontalCenter: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: selectionRectangleId.mouseBuffer
        anchors.bottomMargin: selectionRectangleId.mouseBuffer
        hoverEnabled: true
        cursorShape: Qt.SplitHCursor
        onPressed: { }
        onPositionChanged: (mouse) => {
            if(pressed) {
                var leftSide = mapToItem(selectionRectangleId.parent, mouse.x, 0).x
                var rightSide = selectionRectangleId.x + selectionRectangleId.width;
                var width = rightSide - leftSide
                if(width >= selectionRectangleId.mouseBuffer) {
                    selectionRectangleId.x = leftSide
                    selectionRectangleId.width = width
                }
            }
        }
    }

    QQ.MouseArea {
        id: rightSide
        width: selectionRectangleId.mouseBuffer
        anchors.horizontalCenter: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: selectionRectangleId.mouseBuffer
        anchors.bottomMargin: selectionRectangleId.mouseBuffer
        hoverEnabled: true
        cursorShape: Qt.SplitHCursor
        onPressed: { }
        onPositionChanged: (mouse) => {
            if(pressed) {
                var leftSide = selectionRectangleId.x;
                var rightSide = mapToItem(selectionRectangleId.parent, mouse.x, 0).x
                var width = rightSide - leftSide
                if(width >= selectionRectangleId.mouseBuffer) {
                    selectionRectangleId.width = width
                }
            }
        }
    }

    QQ.MouseArea {
        id: topSide
        height: selectionRectangleId.mouseBuffer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.top
        anchors.leftMargin: selectionRectangleId.mouseBuffer
        anchors.rightMargin: selectionRectangleId.mouseBuffer
        hoverEnabled: true
        cursorShape: Qt.SplitVCursor
        onPressed: { }
        onPositionChanged: (mouse) => {
            if(pressed) {
                var topSide = mapToItem(selectionRectangleId.parent, 0, mouse.y).y
                var bottomSide = selectionRectangleId.y + selectionRectangleId.height
                var height = bottomSide - topSide
                if(height >= selectionRectangleId.mouseBuffer) {
                    selectionRectangleId.y = topSide
                    selectionRectangleId.height = height
                }
            }
        }
    }

    QQ.MouseArea {
        id: bottomSide
        height: selectionRectangleId.mouseBuffer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.bottom
        anchors.leftMargin: selectionRectangleId.mouseBuffer
        anchors.rightMargin: selectionRectangleId.mouseBuffer
        hoverEnabled: true
        cursorShape: Qt.SplitVCursor
        onPressed: { }
        onPositionChanged: (mouse) => {
            if(pressed) {
                var topSide = selectionRectangleId.y
                var bottomSide = mapToItem(selectionRectangleId.parent, 0, mouse.y).y
                var height = bottomSide - topSide
                if(height >= selectionRectangleId.mouseBuffer) {
                    selectionRectangleId.height = height
                }
            }
        }
    }

}
