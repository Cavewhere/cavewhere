/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A compact warning icon shown next to the "Fix stations:" line on CavePage when
// a cave carries a fix-station error. Bound to the cave's errorModel but scoped
// by errorTypeIds to just the fix-station warning kinds, so it stays distinct
// from the page banner's all-warnings roll-up. Presentation-only: it reflects
// state, the Fix Stations page is where the error is corrected.
QQ.Item {
    id: badgeId

    property ErrorModel errorModel
    property list<int> errorTypeIds: []

    // The scoped, non-suppressed warning messages. warningMessagesForTypeIds is
    // an invokable, not a property, so it carries no change signal of its own —
    // the bare read of warningMessages below is what subscribes this binding to
    // warningMessagesChanged (fired on every add/remove/suppress/edit), making
    // the scoped query re-run live. It is load-bearing, not dead code: without
    // it the badge never clears on suppression.
    readonly property list<string> messages: {
        if (!badgeId.errorModel) {
            return []
        }
        badgeId.errorModel.errors.warningMessages
        return badgeId.errorModel.errors.warningMessagesForTypeIds(badgeId.errorTypeIds)
    }

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
