/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ

QQ.Rectangle {
    id: dataSideBar

    property alias caveSidebar: caveDataSidebar

    CompactTabWidget {
        id: tabWidget
        anchors.fill: parent;

        children: [
            QQ.Image {
                id: splitter
                fillMode: QQ.Image.TileVertically
                source: "qrc:icons/verticalLine.png"
               // anchors.bottom: parent.bottom
                height: tabWidget.areaY + 4
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: 1
                z: 1
            }
        ]

        CaveDataSidebarPage {
            id: caveDataSidebar
            property string label: "Caves"
            property string icon: "qrc:icons/caves-64x64.png"
        }

//        Text {
//            property string label: "Surface"
//            property string icon: "qrc:icons/surface.png"
//            text: "This is the Surface page"
//        }
    }
}
