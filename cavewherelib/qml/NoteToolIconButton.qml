/**************************************************************************
**
**    NoteToolIconButton.qml
**    Shared icon button used by note transform editors for tool activation.
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as Controls

Controls.RoundButton {
    id: root
    radius: 2

    property string toolTipText: ""
    property alias iconSource: root.icon.source

    icon {
        width: 20
        height: 20
    }

    Controls.ToolTip {
        delay: 250
        visible: hovered && toolTipText.length > 0
        text: toolTipText
    }



    // Accessible.name: toolTipText
}

