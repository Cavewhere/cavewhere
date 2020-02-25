import QtQuick 2.0 as QQ
import Cavewhere 1.0

QQ.PinchArea {
    id: pintchInteraction
    property BasePanZoomInteraction basePanZoom
    onPinchUpdated: {
        var deltaScale = pinch.scale - pinch.previousScale
        var delta = Math.round(deltaScale * 100);

        if(delta != 0) {
            basePanZoom.zoom(delta, pinch.startCenter);
        }
    }
}
