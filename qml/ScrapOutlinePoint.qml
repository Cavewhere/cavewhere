// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

Positioner3D {

    property Scrap scrap;
    property ScrapControlPointView controlPointView;
    property int scrapPointIndex;

    /**
      Returns true if the point is in point's geometry and false if it isn't.
      Point needs to be in the local coordinate system of the point.  Where 0,0,0 is the
      center of the point.
      */
    function contains(point) {
        var length = Math.sqrt(point.x * point.x + point.y + point.y);
        return length <= pointGeometry.radius;
    }

    Rectangle {
        id: pointGeometry
        width: 9
        height: 9

        anchors.centerIn: parent

        border.width: 1

        color: "green"
        radius: width / 2.0



    }



}
