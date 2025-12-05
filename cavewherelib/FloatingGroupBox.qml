/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: floatingGroupBoxId

    property int margin: 3
    property alias title: titleId.text
    property real borderWidth: 0
    default property alias boxGroupChildren: container.children

    implicitWidth: childrenRect.width
    implicitHeight: childrenRect.height

    Text {
        id: titleId
        z: 1
        anchors.right: boxGroupId.right
        anchors.bottom: boxGroupId.top
        anchors.rightMargin: 5
        font.bold:  true
        color: Theme.text
    }

    ShadowRectangle {
        id: backgroundRect

        color: Theme.floatingWidgetColor
        radius: Theme.floatingWidgetRadius
        height: boxGroupId.height + titleId.height + floatingGroupBoxId.margin
        width: boxGroupId.width + floatingGroupBoxId.margin * 2
        //        y: groupAreaRect.height / 2.0
    }


    QQ.Rectangle {
        id: boxGroupId

        x: floatingGroupBoxId.margin
        y: titleId.height

        border.width: floatingGroupBoxId.borderWidth
        radius: Theme.floatingWidgetRadius
        color: Theme.transparent

        width: container.width + floatingGroupBoxId.margin * 2;
        height: container.height + floatingGroupBoxId.margin * 2;

        QQ.Item {
            id: container
            x: floatingGroupBoxId.margin
            y: floatingGroupBoxId.margin
            width: childrenRect.width
            height: childrenRect.height
        }
    }
}
