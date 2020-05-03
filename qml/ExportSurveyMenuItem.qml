/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ
import QtQuick.Controls 1.2

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
