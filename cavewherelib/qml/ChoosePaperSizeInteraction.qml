import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: itemId
//    property var viewer;

    //    parent: viewer

    anchors.fill: parent

    property alias paperRectangle: paperRectangleId
    property alias captureRectangle: captureRectangleId
    required property PaperMarginGroupBox paperMarginGroupBox

    QQ.Rectangle {
        id: leftMargin
        width: itemId.paperMarginGroupBox.leftMargin / paperRectangleId.paperWidth * paperRectangleId.width
        anchors.top: paperRectangleId.top
        anchors.bottom: paperRectangleId.bottom
        color: paperRectangleId.marginColor
    }

    QQ.Rectangle {
        id: rightMargin
        width: itemId.paperMarginGroupBox.rightMargin / paperRectangleId.paperWidth * paperRectangleId.width
        anchors.top: paperRectangleId.top
        anchors.bottom: paperRectangleId.bottom
        anchors.right: paperRectangleId.right
        color: paperRectangleId.marginColor
    }

    QQ.Rectangle {
        id: topMargin
        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        height: itemId.paperMarginGroupBox.topMargin / paperRectangleId.paperHeight * paperRectangleId.height
        color: paperRectangleId.marginColor
    }

    QQ.Rectangle {
        id: bottomMargin
        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        anchors.bottom: paperRectangleId.bottom
        height: itemId.paperMarginGroupBox.bottomMargin / paperRectangleId.paperHeight * paperRectangleId.height
        color: paperRectangleId.marginColor
    }

    QQ.Item {
        id: captureRectangleId
        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        anchors.top: topMargin.bottom
        anchors.bottom: bottomMargin.top
    }

    QQ.Rectangle {
        id: paperRectangleId

        property real paperWidth: 0
        property real paperHeight: 0

        property bool landScape: false
        property QQ.color marginColor: "#BBE8E8E8"

        border.width: 5
        color: Theme.transparent
    }
}
