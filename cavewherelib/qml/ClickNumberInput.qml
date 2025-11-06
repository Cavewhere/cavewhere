/**************************************************************************
**
**    ClickNumberInput.qml
**    Click-to-edit numeric input used inside interactions where we need
**    quick manual overrides.
**
**************************************************************************/

import QtQuick
import QtQuick.Layouts
import cavewherelib

ClickTextInput {
    objectName: "clickNumberInput"
    validator: CompassValidator {  }
    readOnly: false
    Layout.preferredWidth: 70
}
