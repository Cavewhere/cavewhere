/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0

Rectangle {
    id: pageId

    /**
      This is used to test if the current Index in the dataSideBar is of type type.

      If the type is equal to the current index's type, this returns true, otherwise false.
      */
    function currentIndexIsType(type) {
        var index = dataSideBar.caveSidebar.currentIndex
        var indexsType = regionModel.data(index, RegionTreeModel.TypeRole);
        return (indexsType === type);
    }

    /**
      This takes the currently selected index in the dataSideBar and gets the object

      This function returns null if the select index is not of 'type'.  If it is,
      it returns the object
      */
    function getObject(type) {
        if(currentIndexIsType(type)) {
            var index = dataSideBar.caveSidebar.currentIndex
            return regionModel.data(index, RegionTreeModel.ObjectRole);
        }
        return null;
    }

    function resetSideBar() {
        dataSideBar.caveSidebar.reset();
    }


    DataSideBar {
        id: dataSideBar
        width: 300;
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 5
    }


    Image {
        fillMode: Image.TileVertically
        source: "qrc:icons/verticalLine.png"
        height: dataSideBar.anchors.topMargin
        anchors.left: dataSideBar.right
        anchors.leftMargin: -4
        anchors.top: parent.top
       // width: 5
        z:2
    }


    Splitter {
        id: splitter
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.left: dataSideBar.right
        resizeObject: dataSideBar
    }


    Rectangle {
        id: caveTabs
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: dataSideBar.right
        anchors.right: parent.right
        anchors.leftMargin: -1


        AllCavesTabWidget {
            anchors.fill: parent
            visible: currentIndexIsType(RegionTreeModel.CaveType)
        }

        CaveTabWidget {
            anchors.fill: parent
            visible: currentIndexIsType(RegionTreeModel.CaveType)
            currentCave: getObject(RegionTreeModel.CaveType)
        }

        TripTabWidget {
            anchors.fill: parent
            visible: currentIndexIsType(RegionTreeModel.TripType)
            currentTrip: getObject(RegionTreeModel.TripType)

        }
    }

}
