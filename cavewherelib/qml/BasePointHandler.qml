/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: stationItem

    required property QQ.Item parentView

    signal pointSelected()
    signal positionChanged(point position)
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
                let parentCoord = stationItem.mapToItem(stationItem.parentView, centroid.position.x, centroid.position.y)
                positionChanged(parentCoord)
                // stationItem.pointMoved(stationItem.scrapItem.toNoteCoordinates(parentCoord))
            }
        }
    }
}
