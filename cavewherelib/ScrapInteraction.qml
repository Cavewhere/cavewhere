/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

PanZoomInteraction {
    id: panZoom
    dragAcceptedButtons: Qt.RightButton

    property alias note: scrapInteraction.note
    property alias outlinePointView: scrapInteraction.outlinePointView
    required property ScrapView scrapView
    property real zoom: 1.0

    //private data
    property scrapInteractionPoint _lastPoint
    property int _lastPointIndex: -1

    function startNewScrap() {
        scrapInteraction.startNewScrap();
    }

    BaseScrapInteraction {
        id: scrapInteraction
        scrap: panZoom.scrapView.selectedScrapItem !== null ? panZoom.scrapView.selectedScrapItem.scrap : null
    }

    QQ.TapHandler {
        acceptedButtons: Qt.LeftButton
        // gesturePolicy: QQ.TapHandler.DragWithinBounds
        enabled: panZoom.enabled
        parent: panZoom.target
        target: panZoom.target
        onPressedChanged: {
            if(pressed) {
                if(scrapInteraction.scrap === null) {
                    scrapInteraction.addScrap()
                }

                //Scrap is completed, we just need to create a new scrap
                let index;
                if(!panZoom._lastPoint.isSnapped && scrapInteraction.scrap.isClosed()) {
                    scrapInteraction.addScrap()
                    index = 0; //Insert the point at the begining of the new scrap
                } else {
                    index = panZoom._lastPoint.insertIndex
                }

                scrapInteraction.scrap.insertPoint(index, panZoom._lastPoint.noteCoordsPoint)

                panZoom.outlinePointView.selectedItemIndex = index
                panZoom._lastPointIndex = index
            }
        }
    }

    QQ.HoverHandler {
        enabled: panZoom.enabled
        parent: panZoom.target
        target: panZoom.target
        onPointChanged: {
            // if(panZoom._lastPointIndex !== -1) {
            //     let noteCoords = panZoom.scrapView.toNoteCoordinates(point.position)
            //     scrapInteraction.scrap.setPoint(panZoom._lastPointIndex, noteCoords)
            // }

            //Is the last point snapped to something
            panZoom._lastPoint = scrapInteraction.snapToScrapLine(point.position)
            if(panZoom._lastPoint.isSnapped) {
                snapPoint.setPosition(panZoom._lastPoint.imagePoint)
                snapPoint.visible = true;
            } else {
                snapPoint.visible = false;
            }
        }
    }

    HelpBox {
        id: scrapHelpBox
        text: "Trace a cave section by <b>clicking</b> points around it."
    }

    QQ.Rectangle {
        id: snapPoint
        parent: panZoom.target
        visible: false
        color: "red"
        opacity: 0.75
        radius: 5 / panZoom.zoom
        width: 10 / panZoom.zoom
        height: width

        function setPosition(point) {
            x = point.x - width / 2.0
            y = point.y - height / 2.0
        }
    }
}
