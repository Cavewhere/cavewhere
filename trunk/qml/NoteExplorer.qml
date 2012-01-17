// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import Cavewhere 1.0

ImageExplorer {

    property alias noteModel: view.model

    ListView {
        id: view

        anchors.fill: parent

        snapMode: ListView.SnapOneItem
        orientation: ListView.Horizontal
        spacing: 10
        interactive: false

        highlightMoveDuration: 200

        delegate: ImageExplorer {
            width: view.width
            height: view.height
            image: model.image
            rotation: model.noteObject.rotate

            hasPrevious: view.currentIndex - 1 >= 0
            hasNext: view.currentIndex + 1 < view.count

            onNextImage: {
                view.currentIndex = Math.min(view.count - 1, view.currentIndex + 1)
            }

            onPreviousImage: {
                view.currentIndex = Math.max(0, view.currentIndex - 1)
            }
        }
    }
}
