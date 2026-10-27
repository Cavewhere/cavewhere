/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A compact warning icon beside a "Fix stations:" line, shown while messages
// is non-empty; its tooltip lists them. The caller supplies the lines: the
// project-wide list on the Data page, the node-and-descendants subset on a node
// page. Presentation-only: the Fix Stations page is where the error is
// corrected.
QQ.Item {
    id: badgeId

    required property list<string> messages

    visible: badgeId.messages.length > 0
    implicitWidth: badgeId.visible ? Theme.iconSizeButton : 0
    implicitHeight: Theme.iconSizeButton

    QQ.Image {
        anchors.centerIn: parent
        source: "qrc:icons/svg/warning.svg"
        sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
    }

    QQ.HoverHandler {
        id: hoverHandler
    }

    QC.ToolTip {
        visible: hoverHandler.hovered && badgeId.messages.length > 0
        text: badgeId.messages.join("\n")
        delay: 300
    }
}
