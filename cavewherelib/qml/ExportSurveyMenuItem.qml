/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick.Controls as QC
import cavewherelib
QC.MenuItem {
    property string prefixText
    property string currentText

    // Region-wide export gate (cwSurveyExportManager::canExport) — false
    // when the project holds an external centerline attachment, with the
    // reason surfaced as the item's tooltip.
    property bool exportAllowed: true
    property string disabledReason: ""

    text: {
        if(currentText.length !== 0) {
            return prefixText + " - " + currentText
        }
        return prefixText
    }

    enabled: currentText.length !== 0 && exportAllowed

    QC.ToolTip.text: disabledReason
    QC.ToolTip.visible: hovered && !exportAllowed && disabledReason.length > 0
}
