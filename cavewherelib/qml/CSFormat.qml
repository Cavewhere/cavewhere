/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

pragma Singleton

// The single source of truth for turning a coordinate-system string into the
// text a human reads. Both CS surfaces route through here — the compact inline
// picker (CSComboBox's Custom-mode label) and the project's 3-line GroupBox
// (name + EPSG) — so a change to how a CS is presented (separator, datum, the
// nameless-Custom fallback, the Local wording) happens in exactly one place.
// Backed by CoordinateSystem.nameFor (PROJ proj_get_name).
QQ.QtObject {
    id: format

    // The word shown where a CS is absent (empty value == Local).
    function localLabel() {
        return qsTr("Local")
    }

    // True when PROJ can name this CS, i.e. displayName adds information beyond
    // the raw code. False for Local/empty and for a code PROJ can't resolve.
    function hasName(cs) {
        return cs !== "" && CoordinateSystem.nameFor(cs).length > 0
    }

    // The friendly name, e.g. "WGS 84 / UTM zone 13N". Falls back to the raw
    // code when PROJ can't name it, and to "" for Local/empty.
    function displayName(cs) {
        if (cs === "") {
            return ""
        }
        const name = CoordinateSystem.nameFor(cs)
        return name.length > 0 ? name : cs
    }

    // One-line description for the compact inline picker: "Local" for empty,
    // else "code — name" (or just the code when PROJ can't name it).
    function inlineDescription(cs) {
        if (cs === "") {
            return format.localLabel()
        }
        const name = CoordinateSystem.nameFor(cs)
        return name.length > 0 ? cs + " — " + name : cs
    }
}
