/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma Singleton

import QtQuick as QQ
import cavewherelib

// The active page's tool contract, resolved once for every consumer. The
// current page is a QQuickItem*; casting it to ToolProviderPage narrows it to
// the typed tools + interactionManager contract (null for a page that doesn't
// provide tools, e.g. Data/Map). The sidebar tool rail and the tool-property
// flyout both read from here, so the currentPageItem lookup and its guard live
// in one place instead of being duplicated and duck-typed at each site.
QQ.QtObject {
    id: activeTools

    readonly property ToolProviderPage provider:
        RootData.pageView ? (RootData.pageView.currentPageItem as ToolProviderPage) : null

    readonly property list<ToolItem> tools: provider ? provider.tools : []

    readonly property InteractionManager interactionManager:
        provider ? provider.interactionManager : null
}
