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

        delegate: ImageExplorer {
            width: view.width
            height: view.height
            image: model.image
            rotation: model.noteObject.rotate
        }
    }
}
