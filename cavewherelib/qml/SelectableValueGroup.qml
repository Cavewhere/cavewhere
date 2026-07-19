/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ

pragma Singleton

// Coordinates SelectableValue instances so at most one shows a text selection at
// a time. Whichever field most recently gained focus or a selection owns the
// highlight; claiming it clears the previous owner. A context menu opening is not
// a SelectableValue, so it never claims — the selection survives a right-click,
// which is what lets Copy act on it. (SelectableValue keeps persistentSelection
// true, so a focus-out no longer clears the selection on its own; this does.)
QQ.QtObject {
    id: group

    property QQ.TextInput current: null

    function claim(item) {
        if (group.current === item) {
            return
        }
        let previous = group.current
        group.current = item
        if (previous !== null) {
            previous.deselect()
        }
    }

    function release(item) {
        if (group.current === item) {
            group.current = null
        }
    }
}
