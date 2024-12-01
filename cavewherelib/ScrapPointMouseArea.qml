/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: stationItem

    required property ScrapItem scrapItem

    signal pointSelected()
    signal pointMoved(point noteCoord) // Emits in note coordinates
    signal doubleClicked()

    QQ.TapHandler {
        onTapped: {
            pointSelected()
        }

        onDoubleTapped: stationItem.doubleClicked()
    }

    QQ.DragHandler {
        id: stationDragHandler
        dragThreshold: 3

        onActiveChanged: {
            if (active) {
                // Drag has started
                pointSelected()
            }
        }

        onCentroidChanged: {
            if (active) {
                var parentCoord = mapToItem(parentView, centroid.position.x, centroid.position.y)
                pointMoved(scrapItem.toNoteCoordinates(parentCoord))
            }
        }
    }
}
