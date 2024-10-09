import QtQuick 2.0 as QQ
import QtQuick.Controls
import QtQuick.Layouts 1.0
import Cavewhere 1.0

QQ.Item {
//    property var viewer;

    //    parent: viewer

    anchors.fill: parent

    property alias paperRectangle: paperRectangleId
    property alias captureRectangle: captureRectangleId

    QQ.Rectangle {
        id: leftMargin
        width: paperMarginGroupBoxId.leftMargin / paperRectangleId.paperWidth * paperRectangleId.width
        anchors.top: paperRectangleId.top
        anchors.bottom: paperRectangleId.bottom
        color: paperRectangleId.marginColor
    }

    QQ.Rectangle {
        id: rightMargin
        width: paperMarginGroupBoxId.rightMargin / paperRectangleId.paperWidth * paperRectangleId.width
        anchors.top: paperRectangleId.top
        anchors.bottom: paperRectangleId.bottom
        anchors.right: paperRectangleId.right
        color: paperRectangleId.marginColor
    }

    QQ.Rectangle {
        id: topMargin
        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        height: paperMarginGroupBoxId.topMargin / paperRectangleId.paperHeight * paperRectangleId.height
        color: paperRectangleId.marginColor
    }

    QQ.Rectangle {
        id: bottomMargin
        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        anchors.bottom: paperRectangleId.bottom
        height: paperMarginGroupBoxId.bottomMargin / paperRectangleId.paperHeight * paperRectangleId.height
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
        color: "#00000000"
    }
}
