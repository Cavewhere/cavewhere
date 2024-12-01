/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick

Item {
    id: stationItem

    signal pointSelected()
    signal pointMoved(point noteCoord) // Emits in image coordinates
    signal doubleClicked()

    TapHandler {
        onTapped: {
            pointSelected()
        }

        onDoubleTapped: stationItem.doubleClicked()
    }

    DragHandler {
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
                pointMoved(parentCoord)
            }
        }
    }
}
