// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

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

    Keys.onDeletePressed: {
        scrap.removePoint(pointIndex);
    }

    Keys.onPressed: {
        if(event.key === 0x01000003) { //Backspace key = Qt::Key_Backspace
            scrap.removePoint(pointIndex);
        }
    }

    Rectangle {
        id: pointGeometry
        width: 9
        height: 9

        anchors.centerIn: parent

        border.width: 1

        radius: width / 2.0
        color: scrapPointItem.selected ? "red" : "green" //Qt.rgba(0.25, 1.0, 0.25, 1.0) : Qt.rgba(0.0, 1.0, 0.0, 1.0)

        ScrapPointMouseArea {
            anchors.fill: parent
            anchors.bottomMargin: -3
            anchors.rightMargin: -3

            onPointSelected: {
                select()

                if(pointIndex === 0)
                {
                    //Close the scrap
                    scrap.close()
                }
            }

            onPointMoved: scrap.setPoint(pointIndex, Qt.point(noteCoord.x, noteCoord.y))
        }
    }
}
