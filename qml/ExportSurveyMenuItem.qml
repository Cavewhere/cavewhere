/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

MenuItem {

    property string prefixText
    property string currentText

    text: {
        if(currentText.length !== 0) {
            return prefixText + " - " + currentText
        }
        return prefixText
    }

    enabled:  currentText.length !== 0
}
