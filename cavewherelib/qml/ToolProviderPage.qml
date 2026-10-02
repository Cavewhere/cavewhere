/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// Base for a page that contributes tools to the main sidebar's tool rail. The
// rail reaches the active page through RootData.pageView.currentPageItem (a
// QQuickItem*) and reads these two properties; inheriting this component makes
// that contract explicit and typed rather than each page re-declaring the two
// properties by convention. A page with no tools inherits the empty defaults
// and simply contributes nothing.
QQ.Item {
    id: root

    // The tools this page publishes, in the rail's grouping and order.
    property list<ToolItem> tools

    // The modal tool-state machine the rail toggles tools through; null for a
    // page whose tools (if any) don't route through an InteractionManager.
    property InteractionManager interactionManager: null
}
