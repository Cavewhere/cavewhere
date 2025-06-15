import QtQuick as QQ
import cavewherelib

QQ.TapHandler {
    id: tapHandler

    required property ScrapView scrapView
    required property BaseNotePointInteraction notePointInteraction

    acceptedButtons: Qt.LeftButton
    onSingleTapped: (eventPoint, button) => {
                        let notePoint = tapHandler.scrapView.toNoteCoordinates(eventPoint.position)
                        tapHandler.notePointInteraction.addPoint(notePoint)
                    }

}
