/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

/**
  What floats above one window's content: the shared text editor, the space
  popups reparent into so their own parent can't clip them, and the strip
  app-scope banners register into.

  Every window declares one of these and needs to do nothing else with it.
  Anything inside that window reaches it through the WindowOverlay attached
  property, which resolves per window:

      WindowOverlay.overlay as AppOverlay

  Banners work the way popups do: a feature declares its own banner wherever
  its wiring lives and hands it to addBanner(). This decides only where
  banners sit and in what order, and knows nothing about any of them.
 */
WindowOverlay {
    id: appOverlay

    readonly property alias shadowEditor: shadowEditorHostId

    // Where a registered banner ends up, for anything that needs to see the
    // strip itself. The way in is addBanner().
    readonly property alias bannerArea: bannerAreaId

    anchors.fill: parent

    // Takes a feature's banner into this window's banner strip. Banners stack
    // in registration order, the first registered on top, and each one spans
    // the strip. Registering the same banner twice changes nothing.
    function addBanner(banner: QQ.Item) {
        if (banner === null || banner.parent === bannerAreaId) {
            return
        }

        banner.parent = bannerAreaId
        banner.width = Qt.binding(() => bannerAreaId.width)
    }

    // Floats at the top of the window, over the page rather than inside it,
    // and takes up no room while every banner is quiet.
    QQ.Column {
        id: bannerAreaId

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.pageMargin
        spacing: Theme.tightSpacing
    }

    // Last, so an open editor covers whatever else the overlay is showing.
    ShadowEditorHost {
        id: shadowEditorHostId
    }
}
