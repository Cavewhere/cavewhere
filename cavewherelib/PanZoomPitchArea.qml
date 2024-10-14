import QtQuick 2.0 as QQ
import cavewherelib

QQ.PinchArea {
    id: pintchInteraction
    property BasePanZoomInteraction basePanZoom
    onPinchUpdated: (pinch) => {
        var deltaScale = pinch.scale - pinch.previousScale
        var delta = Math.round(deltaScale * 100);

        if(delta != 0) {
            basePanZoom.zoom(delta, pinch.startCenter);
        }
    }
}
