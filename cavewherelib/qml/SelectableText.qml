/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The wrapping twin of SelectableValue: a flat, copyable, read-only value that
// folds onto more lines when its row is too narrow. A TextField paints a single
// line, so this one is a TextArea, whose text control likewise ignores a right
// press, leaving the selection for the copy menu to act on.
QC.TextArea {
    id: root

    background: null
    readOnly: true
    selectByMouse: true
    persistentSelection: true
    // Wrap, not WordWrap: a station name is one token with nowhere to break,
    // and WordWrap paints it past the frame instead of folding it.
    wrapMode: QQ.Text.Wrap
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    color: Theme.text
    selectedTextColor: Theme.textInverse
    selectionColor: Theme.highlight
    font.family: Theme.fontFamily

    // The style sizes a TextArea by its wrapped content, so a row that narrowed
    // it once would keep it narrow. The unwrapped line width is the size the
    // value asks for, as a wrapping Label's is.
    implicitWidth: Math.ceil(unwrappedMetricsId.advanceWidth) + root.leftPadding + root.rightPadding

    onActiveFocusChanged: {
        if (root.activeFocus) {
            SelectableValueGroup.claim(root)
        }
    }

    onSelectedTextChanged: {
        if (root.selectedText.length > 0) {
            SelectableValueGroup.claim(root)
        }
    }

    QQ.Component.onDestruction: SelectableValueGroup.release(root)

    QQ.TextMetrics {
        id: unwrappedMetricsId
        font: root.font
        text: root.text
    }

    QQ.Connections {
        target: SelectableValueGroup

        function onCurrentChanged() {
            if (SelectableValueGroup.current !== root) {
                root.deselect()
            }
        }
    }

    QC.ContextMenu.menu: SelectableValueMenu {
        canCopy: root.selectedText.length > 0
        canSelectAll: root.length > 0

        onCopyRequested: root.copy()
        onSelectAllRequested: root.selectAll()
    }
}
