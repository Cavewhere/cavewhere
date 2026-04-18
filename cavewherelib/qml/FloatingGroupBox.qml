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
    id: floatingGroupBoxId

    property int margin: 3
    property alias title: titleId.text
    property real borderWidth: 0
    property bool collapsed: false
    default property alias boxGroupChildren: container.children

    implicitWidth: backgroundRect.width
    implicitHeight: backgroundRect.height

    QQ.Row {
        id: titleRow
        z: 1
        spacing: 2

        QQ.Item {
            id: collapseButtonId
            objectName: "collapseButton"
            width: 16
            height: 16
            anchors.verticalCenter: titleId.verticalCenter

            Icon {
                anchors.centerIn: parent
                source: floatingGroupBoxId.collapsed
                    ? "qrc:/twbs-icons/icons/chevron-right.svg"
                    : "qrc:/twbs-icons/icons/chevron-down.svg"
                sourceSize: Qt.size(12, 12)
            }
        }

        QC.Label {
            id: titleId
            font.bold: true
        }
    }

    QQ.Item {
        z: 2
        anchors.fill: titleRow

        QQ.TapHandler {
            gesturePolicy: QQ.TapHandler.WithinBounds
            onTapped: floatingGroupBoxId.collapsed = !floatingGroupBoxId.collapsed
        }
    }

    ShadowRectangle {
        id: backgroundRect

        color: Theme.floatingWidgetColor
        radius: Theme.floatingWidgetRadius
        height: floatingGroupBoxId.collapsed
            ? titleRow.height + floatingGroupBoxId.margin
            : boxGroupId.height + titleRow.height + floatingGroupBoxId.margin
        width: floatingGroupBoxId.collapsed
            ? titleRow.width + floatingGroupBoxId.margin * 2
            : Math.max(titleRow.width, boxGroupId.width) + floatingGroupBoxId.margin * 2
    }

    QQ.Rectangle {
        id: boxGroupId

        visible: !floatingGroupBoxId.collapsed
        x: floatingGroupBoxId.margin
        y: titleRow.height

        border.width: floatingGroupBoxId.borderWidth
        radius: Theme.floatingWidgetRadius
        color: Theme.transparent

        width: container.width + floatingGroupBoxId.margin * 2
        height: container.height + floatingGroupBoxId.margin * 2

        QQ.Item {
            id: container
            x: floatingGroupBoxId.margin
            y: floatingGroupBoxId.margin
            width: childrenRect.width
            height: childrenRect.height
        }
    }
}
