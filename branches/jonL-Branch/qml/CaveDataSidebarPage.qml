import QtQuick 1.0
import Cavewhere 1.0

Rectangle {
    id: page

    property variant currentIndex //ModelIndex

    anchors.fill: parent
    anchors.bottomMargin: 1;
    anchors.rightMargin: 1;

    /**
      Keeps this view stateful when items are removed
      */
    function modelRemovingIndexes(parentIndex, begin, end) {
        if(!caveElement.visible) {
            return;
        }

        //Search through all the removed indexes and compare them the current cave
        for(var i = begin; i <= end; i++) {
            var removedIndex = regionModel.index(i, 0, parentIndex);
            var removedObject = regionModel.data(removedIndex, RegionTreeModel.ObjectRole);
            var caveElementObject = regionModel.data(caveElement.index, RegionTreeModel.ObjectRole);

            if(removedObject == caveElementObject) {
                regionVisualDataModel.rootIndex = cavesElement.index
                view.currentIndex = cavesElement.viewIndex
            }
        }
    }

    Component.onCompleted: {
        regionModel.rowsAboutToBeRemoved.connect(modelRemovingIndexes);
    }

     Image {
        id: splitter
        fillMode: Image.TileVertically
        source: "qrc:icons/verticalLine.png"
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
            model: regionModel
            view: view;
            viewIndex: -1; //The root element
            name: "All Caves"
            iconSource: "qrc:icons/caves-64x64.png"
            addButtonText: "Add Cave"

            addButtonVisible: !caveElement.visible

            //Add a cave to the model
            onAddButtonClicked: {
                var keepSelected = view.currentIndex == viewIndex
                region.addCave(); //c++ adds a cave

                if(keepSelected) {
                    view.currentIndex = viewIndex
                }
            }
        }

        TreeRootElement {
            id: caveElement
            regionVisualDataModel: regionVisualDataModel;
            model: regionModel
            view: view;
            viewIndex: -2; //The root element
            addButtonText: "Add Trip"
            iconSource: "qrc:icons/cave-64x64.png"
            visible: false;

            anchors.leftMargin: 5

            //Add a trip to the model
            onAddButtonClicked: {
                var keepSelected = view.currentIndex == viewIndex

                var cave = regionModel.cave(caveElement.index);
                cave.addTrip()

                if(keepSelected) {
                    view.currentIndex = viewIndex
                }
            }
        }

    }

    VisualDataModel {
        id: regionVisualDataModel

        model: regionModel

        delegate: CaveTreeDelegate {
            focus: view.currentIndex == index
        }

        onRootIndexChanged:  {
            switch(regionModel.data(rootIndex, RegionTreeModel.TypeRole)) {
            case RegionTreeModel.RegionType:
                caveElement.visible = false;
                view.currentIndex = cavesElement.viewIndex
                break
            case RegionTreeModel.CaveType: {
                caveElement.index = regionVisualDataModel.rootIndex
                caveElement.visible = true;
                view.currentIndex = caveElement.viewIndex
                break
            }
            default:
                break
            }
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

        onCurrentIndexChanged: {
            if(currentIndex == cavesElement.viewIndex) {
                //currentIndex == -1
                page.currentIndex = cavesElement.index
            } else if (currentIndex == caveElement.viewIndex) {
                //currentIndex == -2
                page.currentIndex = caveElement.index
            } else {
                page.currentIndex = regionVisualDataModel.modelIndex(currentIndex);
            }
        }
    }

}
