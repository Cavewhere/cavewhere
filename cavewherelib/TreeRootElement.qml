/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import cavewherelib

QQ.Rectangle {
    id: rootElement

    property variant index //QModelindex
    property variant regionVisualDataModel
    property variant model //cwRegionModel
    property variant view
    property int viewIndex
    property alias name: nameText.text
    property alias addButtonText: addButton.text
    property alias addButtonVisible:  addButton.visible
    property bool selected: rootElement.view.currentIndex === viewIndex
    property alias iconSource:  icon.source

    signal addButtonClicked()
    signal clicked()

    anchors.left: parent.left
    anchors.right: parent.right
    height:  35
    color: "#00000000"

    /**
      Handles the dataChanged signal in abstractitemmodel
      */
    function updateData(topLeft, bottomLeft) {
        name = model.data(index, RegionTreeModel.NameRole)
    }

    onIndexChanged: {
        name = model.data(index, RegionTreeModel.NameRole)
    }

    onModelChanged: {
        model.dataChanged.connect(updateData)
    }

    DataSidebarItemTab {
        id: backgroundCavesTab
        selected: rootElement.selected
    }

    QQ.Image {
        id: icon
        sourceSize.width: 16
        sourceSize.height: 16
        anchors.left: parent.left
        anchors.leftMargin: 7
        anchors.verticalCenter: backgroundCavesTab.verticalCenter
    }

    DoubleClickTextInput {
        id: nameText
        anchors.left: icon.right
        anchors.leftMargin: 2
        anchors.verticalCenter: backgroundCavesTab.verticalCenter
        font.bold: selected
        z:1

        onFinishedEditting: {
            regionVisualDataModel.model.setData(index, newText, RegionTreeModel.NameRole)
        }
    }

    QQ.MouseArea {
        anchors.fill: parent;

        onClicked: {
            regionSelectionModel.setCurrentIndex(rootElement.index, 0x0010); //select current
            regionVisualDataModel.rootIndex = rootElement.index
            view.currentIndex = viewIndex
            rootElement.clicked();
        }

        AddButton {
            id: addButton

            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter

            onClicked: {
                //Add a cave
                rootElement.addButtonClicked();
            }
        }
    }
}
