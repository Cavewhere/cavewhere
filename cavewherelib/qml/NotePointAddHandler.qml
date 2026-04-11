import QtQuick as QQ
import cavewherelib

// Wraps TapHandler in Item to avoid intermittent "Cannot find member data"
// failures — TapHandler lacks the `data` default property QML expects.
QQ.Item {
    id: root

    required property ScrapView scrapView
    required property BaseNotePointInteraction notePointInteraction
    property QQ.Item tapTarget

    visible: false
    width: 0
    height: 0

    QQ.TapHandler {
        target: root.tapTarget
        parent: root.parent
        enabled: root.enabled
        acceptedButtons: Qt.LeftButton
        onSingleTapped: (eventPoint, button) => {
                            let notePoint = root.scrapView.toNoteCoordinates(eventPoint.position)
                            root.notePointInteraction.addPoint(notePoint)
                        }
    }
}
