/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma Singleton

import QtQuick as QQ
import cavewherelib

// The unit system (Metric / Imperial) in effect for display right now: the open
// project's, or the app-level default when no project is loaded. Every display
// surface resolves its unit through this one binding, so the project→app-default
// fallback lives in a single place instead of being copy-pasted per call site.
QQ.QtObject {
    id: projectUnits

    readonly property int unitSystem: RootData.region
        ? RootData.region.unitSystem
        : RootData.settings.unitSettings.unitSystem
}
