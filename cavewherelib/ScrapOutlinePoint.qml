/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick as QQ
import cavewherelib

ScrapPointItem {
    id: scrapPointItem

    /**
      Returns true if the point is in point's geometry and false if it isn't.
      Point needs to be in the local coordinate system of the point.  Where 0,0,0 is the
      center of the point.
      */
    function contains(point) {
        var length = Math.sqrt(point.x * point.x + point.y + point.y);
        return length <= pointGeometry.radius;
    }

    QQ.Keys.onDeletePressed: {
        scrap.removePoint(pointIndex);
    }

    QQ.Keys.onPressed: (event) => {
                           if(event.key === Qt.Key_Backspace) {
                               scrap.removePoint(pointIndex);
                           }
                       }

    QQ.Rectangle {
        id: pointGeometry
        width: 10
        height: width

        anchors.centerIn: parent

        border.width: 1

        radius: width / 2.0
        color: scrapPointItem.selected ? "red" : "green" //Qt.rgba(0.25, 1.0, 0.25, 1.0) : Qt.rgba(0.0, 1.0, 0.0, 1.0)

        ScrapPointMouseArea {
            anchors.fill: parent
            anchors.bottomMargin: -3
            anchors.rightMargin: -3

            scrapItem: scrapPointItem.scrapItem
            parentView: scrapPointItem.parentView

            onPointSelected: {
                scrapPointItem.select()

                if(scrapPointItem.pointIndex === 0) {
                    //Close the scrap
                    scrapPointItem.scrap.close()
                }
            }

            onPointMoved: (noteCoord) => scrapPointItem.scrap.setPoint(scrapPointItem.pointIndex, noteCoord)
        }
    }
}
