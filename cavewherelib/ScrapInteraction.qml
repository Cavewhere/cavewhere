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
        target: panZoom.target
        onPressedChanged: {
            console.log("pressed:" + pressed)
            if(pressed) {
                if(scrapInteraction.scrap === null) {
                    console.log("Adding scrap!")
                    scrapInteraction.addScrap()
                }

                //Scrap is completed, we just need to create a new scrap
                let index;
                if(!panZoom._lastPoint.isSnapped && scrapInteraction.scrap.isClosed()) {
                    console.log("Adding scrap again!");
                    scrapInteraction.addScrap()
                    index = 0; //Insert the point at the begining of the new scrap
                } else {
                    console.log("LastPoint:" + panZoom._lastPoint.insertIndex)
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
        target: panZoom.target
        onPointChanged: {
            if(panZoom._lastPointIndex !== -1) {
                let noteCoords = panZoom.scrapView.toNoteCoordinates(point.position)
                scrapInteraction.scrap.setPoint(panZoom._lastPointIndex, noteCoords)
            }

            //Is the last point snapped to something
            panZoom._lastPoint = scrapInteraction.snapToScrapLine(point.position)
            console.log("LastPoint move:" + panZoom._lastPoint.insertIndex + " " + panZoom._lastPoint.noteCoordsPoint + panZoom._lastPoint.imagePoint)
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
        visible: false
        color: "red"
        opacity: 0.75
        radius: 5
        width: 10
        height: 10

        function setPosition(point) {
            x = point.x - width / 2.0
            y = point.y - height / 2.0
        }
    }

}

// BaseScrapInteraction {
//     id: interaction

//     property BasePanZoomInteraction basePanZoomInteraction

//     PanZoomPitchArea {
//         anchors.fill: parent
//         basePanZoom: interaction.basePanZoomInteraction

//         QQ.MouseArea {
//             anchors.fill: parent
//             acceptedButtons: Qt.LeftButton | Qt.RightButton
//             hoverEnabled: true

//             property var lastPoint; //This is a Array of the last point
//             property var lastPointIndex; //The last point item that was added



//             onReleased: {
//                 lastPointIndex = -1
//             }

//             onPositionChanged: (mouse) => {
//                 if(pressedButtons === Qt.RightButton) {
//                     interaction.basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
//                 }

//                 if(pressedButtons === Qt.LeftButton &&
//                         lastPointIndex !== -1) {
//                     var noteCoords = interaction.imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y))
//                     interaction.scrap.setPoint(lastPointIndex, noteCoords)
//                 }

//                 //Is the last point snapped to something
//                 lastPoint = interaction.snapToScrapLine(Qt.point(mouse.x, mouse.y))
//                 if(lastPoint["IsSnapped"]) {
//                     var point = lastPoint["QtViewportPoint"]
//                     snapPoint.setPosition(point)
//                     snapPoint.visible = true;
//                 } else {
//                     snapPoint.visible = false;
//                 }
//             }

//             onEntered: {
//                 interaction.forceActiveFocus()
//             }

//             onWheel: interaction.basePanZoomInteraction.zoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))
//         }
//     }

//     HelpBox {
//         id: scrapHelpBox
//         text: "Trace a cave section by <b>clicking</b> points around it."
//     }

//     QQ.Rectangle {
//         id: snapPoint
//         visible: false
//         color: "red"
//         opacity: 0.75
//         radius: 5
//         width: 10
//         height: 10

//         function setPosition(point) {
//             x = point.x - width / 2.0
//             y = point.y - height / 2.0
//         }
//     }

//     QQ.Keys.onSpacePressed: {
//         interaction.startNewScrap()
//     }
// }
