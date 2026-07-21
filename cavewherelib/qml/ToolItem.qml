/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// One entry in a page's tool model. A page publishes a list<ToolItem>; the
// sidebar tool rail (and the phone tools drawer) render it. This is pure data —
// it names the modal interaction the tool arms plus its button chrome.
QQ.QtObject {
    id: toolId

    // The modal interaction this tool arms. The rail compares it against
    // InteractionManager.activeInteraction for the armed state and calls
    // activate()/deactivate() on it.
    property Interaction interaction

    // Button chrome. iconSource is the rail glyph; text/toolTip drive the
    // accessible name and the instant tooltip.
    property url iconSource
    property string text
    property string toolTip

    // Grouping heading ("Measure & Pick", "Point Cloud", …). Tools sharing a
    // group render together under one label in the rail.
    property string group
}
