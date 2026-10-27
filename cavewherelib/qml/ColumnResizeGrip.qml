/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// A strip on one edge of a column's header cell whose sideways drag asks for
// a new column width through resizeRequested(). The host owns the width: it
// clamps the request, stores it and hands the result back as columnWidth.
QQ.Item {
    id: gripId

    //The column's width as the host lays it out, read when a drag starts.
    required property int columnWidth

    //True for a grip on the column's left edge, where a leftward drag widens
    //the column; false for one on the right edge, where a rightward drag does.
    property bool growsLeftward: false

    width: Theme.columnResizeGripWidth

    signal resizeRequested(int newWidth)

    QQ.HoverHandler {
        cursorShape: Qt.SplitHCursor
    }

    QQ.DragHandler {
        id: dragHandlerId

        property int startWidth: 0

        target: null
        yAxis.enabled: false

        onActiveChanged: {
            if (active) {
                dragHandlerId.startWidth = gripId.columnWidth
            }
        }
        // Measured from the press, so the drag threshold moves the edge too.
        onCentroidChanged: {
            if (active) {
                const dx = centroid.scenePosition.x - centroid.scenePressPosition.x
                gripId.resizeRequested(Math.round(gripId.growsLeftward
                                                  ? dragHandlerId.startWidth - dx
                                                  : dragHandlerId.startWidth + dx))
            }
        }
    }
}
