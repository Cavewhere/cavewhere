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
    signal finishedMoving(point position)
    signal doubleClicked()
    signal dragActiveChanged(bool active)

    function position() {
        return stationItem.mapToItem(stationItem.parentView,
        stationDragHandler.centroid.position.x,
        stationDragHandler.centroid.position.y)
    }

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
            } else {
                stationItem.finishedMoving(position())
            }

            stationItem.dragActiveChanged(active)
        }

        onCentroidChanged: {
            if (active) {
                stationItem.positionChanged(position())
            }
        }
    }
}
