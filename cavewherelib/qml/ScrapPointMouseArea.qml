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
    required property AbstractPointManager parentView

    signal pointSelected()
    signal pointMoved(point noteCoord) // Emits in note coordinates
    signal doubleClicked()

    QQ.TapHandler {
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
        onTapped: {
            stationItem.pointSelected()
        }

        onDoubleTapped: stationItem.doubleClicked()
    }

    QQ.DragHandler {
        id: stationDragHandler
        dragThreshold: 3

        onActiveChanged: {
            if (active) {
                // Drag has started
                stationItem.pointSelected()
            }
        }

        onCentroidChanged: {
            if (active) {
                var parentCoord = stationItem.mapToItem(stationItem.parentView, centroid.position.x, centroid.position.y)
                stationItem.pointMoved(stationItem.scrapItem.toNoteCoordinates(parentCoord))
            }
        }
    }
}
