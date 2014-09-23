import QtQuick 2.0
import Cavewhere 1.0

Rectangle {
    id: interactionId

    property CaptureItem captureItem: null;
    property double captureScale: 1.0
    property point captureOffset

    width: 0
    height: 0
    x: 0
    y: 0

    border.width: 1
    border.color: "green"
    color: "#00000000"

    states: [
        State {
            when: captureItem !== null

            PropertyChanges {
                target: interactionId
                width: captureItem.paperSizeOfItem.width * captureScale
                height: captureItem.paperSizeOfItem.height * captureScale;
                x: (captureItem.positionOnPaper.x - captureOffset.x) * captureScale;
                y: (captureItem.positionOnPaper.y - captureOffset.y) * captureScale
            }
        }
    ]
}
