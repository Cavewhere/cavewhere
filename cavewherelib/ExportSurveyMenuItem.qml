/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import QtQuick.Controls

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
