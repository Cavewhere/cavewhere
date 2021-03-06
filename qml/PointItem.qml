/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 1.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ
import Cavewhere 1.0

/**
    This class related to cwAbstractPointManager
  */
Positioner3D {

    property var parentView; //Is an abstract point manager
    property int pointIndex; //The index in the item list
    property bool selected: false

    onSelectedChanged: {
        if(parentView.selectedItemIndex === pointIndex && !selected) {
            parentView.selectedItemIndex = -1
        } else if(selected) {
            parentView.selectedItemIndex = pointIndex
        }
    }
}
