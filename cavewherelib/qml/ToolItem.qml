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

    // Heading shown on the tool-property flyout while this tool is armed. The
    // rail button stays terse (`text`), so this defaults to it but can be a
    // fuller phrase (e.g. "Point Cloud Clip") for the flyout's wider header.
    property string flyoutTitle: toolId.text

    // Optional options UI shown in the sidebar tool-property flyout while this
    // tool is armed. Null means the tool has no editable options, so arming it
    // opens no flyout. The Component is instantiated in the flyout's Loader but
    // resolves ids in its declaration context (the page), so it can reference
    // the interaction objects declared beside the tool model.
    property QQ.Component propertyContent: null

    // Stable objectName handed to the rail button so tests (and screenshots) can
    // find it. Kept off this data holder's own objectName to avoid a duplicate
    // in the object tree.
    property string buttonObjectName

    // Grouping heading ("Measure & Pick", "Point Cloud", …). Tools sharing a
    // group render together under one label in the rail.
    property string group

    // Stable, untranslated key the rail groups on. `group` is the visible
    // (translatable) heading; boundary detection keys off this so a translation
    // never merges or splits groups.
    property string groupId
}
