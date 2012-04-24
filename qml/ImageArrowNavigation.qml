// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Image {
    id: image

    property string highlightedImageSource
    property string imageSource

    signal clicked()

    source: mouseAreaPrevious.containsMouse ?  highlightedImageSource : imageSource

    opacity: 0.75
    scale: 1.0

    MouseArea {
        id: mouseAreaPrevious
        anchors.fill: parent

        hoverEnabled: true

        onClicked: {
            image.clicked()
        }
    }

    Behavior on scale {
        NumberAnimation { duration: 50 }
    }

    states: [
        State {
            name: "hoverState"
            when: mouseAreaPrevious.containsMouse

            PropertyChanges {
                target: image
                scale: 1.1
            }
        }
    ]
}
