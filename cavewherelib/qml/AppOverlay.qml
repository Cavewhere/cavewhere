/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

/**
  What floats above one window's content: the shared text editor, and the space
  popups reparent into so their own parent can't clip them.

  Every window declares one of these and needs to do nothing else with it.
  Anything inside that window reaches it through the WindowOverlay attached
  property, which resolves per window:

      WindowOverlay.overlay as AppOverlay
 */
WindowOverlay {
    id: appOverlay

    readonly property alias shadowEditor: shadowEditorHostId

    anchors.fill: parent

    ShadowEditorHost {
        id: shadowEditorHostId
    }
}
