/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import "Theme.js" as Theme

QQ.Item {
    id: itemId

    property QQ.color backgroundColor: "white"
    property alias checked: checkbox.checked
    property alias text: checkbox.text
    property bool contentsVisible: true
    default property alias contentData: contentArea.data

    height: contentsVisible ? checkbox.height + contentArea.childrenRect.height + 3 : checkbox.height
    width: contentArea.childrenRect.width + 6

    implicitHeight: height
    implicitWidth: width

    QQ.Rectangle {
        id: checkBoxGroup
        border.width: 1
        border.color: "gray"
        radius: Theme.floatingWidgetRadius
        color: "#00000000"
        visible: itemId.contentsVisible

        anchors.top: checkbox.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        QQ.Item {
            id: contentArea
            anchors.top: checkBoxGroup.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            anchors.topMargin: checkbox.height / 2
            anchors.leftMargin: 3
            anchors.rightMargin: 3
            anchors.bottomMargin: 3
        }
    }

    QQ.Rectangle {
        color: itemId.backgroundColor
        anchors.fill: checkbox
        visible: itemId.contentsVisible
    }

    CheckBox {
        id: checkbox
        anchors.left: checkBoxGroup.left
        anchors.leftMargin: 6
    }

}
