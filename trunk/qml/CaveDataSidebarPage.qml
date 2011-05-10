import QtQuick 1.0
import Cavewhere 1.0

Rectangle {
    id: page

    anchors.fill: parent
    anchors.bottomMargin: 1;
    anchors.rightMargin: 1;

    Image {
        id: splitter
        fillMode: Image.TileVertically
        source: "icons/verticalLine.png"
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: parent.right
        anchors.leftMargin: -3
    }

    Column {
        id: staticElements
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        spacing: view.spacing

        TreeRootElement {
            id: cavesElement
            regionVisualDataModel: regionVisualDataModel;
            view: view;
            viewIndex: -1; //The root element
            name: "All Caves"
            iconSource: "icons/caves-64x64.png"
            addButtonText: "Add Cave"

            onClicked: {
                //This returns make the caveElement disappear
                caveElement.visible = false;
                currentCave = null
                currentTrip = null
            }

            addButtonVisible: !caveElement.visible

            //Add a cave to the model
            onAddButtonClicked: {
                region.addCave(); //c++ adds a cave
            }
        }

        TreeRootElement {
            id: caveElement
            regionVisualDataModel: regionVisualDataModel;
            view: view;
            viewIndex: -2; //The root element
            addButtonText: "Add Trip"
            iconSource: "icons/cave-64x64.png"
            visible: false

            anchors.leftMargin: 5
        }

    }

    VisualDataModel {
        id: regionVisualDataModel

        model: regionModel

        delegate: CaveTreeDelegate {
            focus: view.currentIndex == index
        }


}


ListView {
    id: view
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.top: staticElements.bottom

    anchors.leftMargin: caveElement.visible ? 10 : 5

    clip: true
    spacing: -3
    currentIndex: -1

    model:  regionVisualDataModel

    onModelChanged: {
        cavesElement.index = regionVisualDataModel.rootIndex
    }
}




}
