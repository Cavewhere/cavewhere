/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import cavewherelib

/**
    This class related to cwAbstractPointManager
  */
Positioner {


    property AbstractPointManager parentView; //Is an abstract point manager
    property int pointIndex; //The index in the item list
    property bool selected: false

    //Written by cwTransformUpdater: false once the point falls outside the view
    //frustum. A subclass that overrides visible has to keep this term, or the point
    //renders at the garbage position an off-screen projection produces.
    property bool inFrustum: true

    focus: selected //A selected point takes keyboard focus so its Keys handlers fire

    visible: inFrustum

    onSelectedChanged: {
        if(parentView.selectedItemIndex === pointIndex && !selected) {
            parentView.selectedItemIndex = -1
        } else if(selected) {
            parentView.selectedItemIndex = pointIndex
            forceActiveFocus();
        }
    }
}
