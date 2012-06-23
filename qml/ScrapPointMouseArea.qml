// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

/**
This is used for the scrap outline point and the note point to move the point around
  */
MouseArea {
    id: stationMouseArea

    property variant lastPoint;
    property bool ignoreLength;

    signal pointSelected()
    signal pointMoved(var noteCoord)

    onClicked: pointSelected()

    onReleased: ({ })

    onPressed: {
        lastPoint = Qt.point(mouse.x, mouse.y);
        ignoreLength = false;
    }

    onPositionChanged: {
        //Make sure the mouse has move at least three pixel from where it's started
        var length = Math.sqrt(Math.pow(lastPoint.x - mouse.x, 2) + Math.pow(lastPoint.y - mouse.y, 2));
        if(length > 3 || ignoreLength) {
            ignoreLength = true
            var parentCoord = mapToItem(parentView, mouse.x, mouse.y);
            var transformer = parentView.transformUpdater;
            var noteCoord = transformer.mapFromViewportToModel(Qt.point(parentCoord.x, parentCoord.y));
            pointMoved(noteCoord);
        }
    }
}
