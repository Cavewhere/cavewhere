/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ

pragma Singleton

// Coordinates SelectableValue and SelectableText instances so at most one shows a
// text selection at a time. Whichever field most recently gained focus or a
// selection owns the highlight; every other field deselects itself when the owner
// changes. A context menu opening is not a field, so it never claims — the
// selection survives a right-click, which is what lets Copy act on it. (The fields
// keep persistentSelection true, so a focus-out no longer clears the selection on
// its own; this does.)
QQ.QtObject {
    id: group

    // A TextInput or a TextEdit; each field watches this and clears its own
    // selection, since the two share deselect() but no type to call it through.
    property QQ.Item current: null

    function claim(item) {
        group.current = item
    }

    function release(item) {
        if (group.current === item) {
            group.current = null
        }
    }
}
