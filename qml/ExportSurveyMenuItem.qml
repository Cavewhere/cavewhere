// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import QtDesktop 0.1 as Desktop
import Cavewhere 1.0

Desktop.MenuItem {

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
