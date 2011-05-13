import QtQuick 1.0

Rectangle {
    id: rootElement

    property variant index
    property variant regionVisualDataModel
    property variant view
    property int viewIndex
    property alias name: nameText.text
    property alias addButtonText: addButton.text
    property alias addButtonVisible:  addButton.visible
    property bool selected: rootElement.view.currentIndex == viewIndex
    property alias iconSource:  icon.source

    signal addButtonClicked()
    signal clicked()

    anchors.left: parent.left
    anchors.right: parent.right
    height:  35
    color: "#00000000"

    DataSidebarItemTab {
        id: backgroundCavesTab
        selected: rootElement.selected
    }

    Image {
        id: icon
        sourceSize.width: 16
        sourceSize.height: 16
        anchors.left: parent.left
        anchors.leftMargin: 7
        anchors.verticalCenter: backgroundCavesTab.verticalCenter
    }

    Text {
        id: nameText
        anchors.left: icon.right
        anchors.leftMargin: 2
        anchors.verticalCenter: backgroundCavesTab.verticalCenter
        font.bold: selected
    }

    MouseArea {
        anchors.fill: parent;

        onClicked: {
            regionVisualDataModel.rootIndex = rootElement.index
            view.currentIndex = viewIndex
            rootElement.clicked();
        }

        Button {
            id: addButton

            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter

            iconSource: "qrc:icons/plus.png"

            onClicked: {
                //Add a cave
                rootElement.addButtonClicked();
            }
        }
    }
}
