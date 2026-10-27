/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// One icon button that opens every row of a tree while any row is closed,
// and closes them all once every row is open.
QC.ToolButton {
    id: expandCollapseButtonId

    // True while some row with children is closed; the button then expands.
    property bool hasCollapsedRows: false

    signal expandAllRequested()
    signal collapseAllRequested()

    text: expandCollapseButtonId.hasCollapsedRows ? qsTr("Expand all") : qsTr("Collapse all")
    display: QC.AbstractButton.IconOnly
    icon.source: expandCollapseButtonId.hasCollapsedRows
                 ? "qrc:/twbs-icons/icons/arrows-expand.svg"
                 : "qrc:/twbs-icons/icons/arrows-collapse.svg"
    icon.color: Theme.text

    QC.ToolTip.text: expandCollapseButtonId.text
    QC.ToolTip.delay: Theme.toolTipDelay
    QC.ToolTip.visible: expandCollapseButtonId.hovered

    onClicked: {
        if (expandCollapseButtonId.hasCollapsedRows) {
            expandCollapseButtonId.expandAllRequested()
        } else {
            expandCollapseButtonId.collapseAllRequested()
        }
    }
}
