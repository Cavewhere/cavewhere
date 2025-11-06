/**************************************************************************
**
**    RowLayoutPanel.qml
**    Convenience wrapper exposing a RowLayout inside the floating panel.
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

InteractionFloatingPanel {
    default property alias children: layout.children
    content: RowLayout {
        id: layout
        spacing: 6
    }
}
