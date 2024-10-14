/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ

QQ.Image {
    id: image

    property string highlightedImageSource
    property string imageSource

    signal clicked()

    source: mouseAreaPrevious.containsMouse ?  highlightedImageSource : imageSource

    opacity: 0.75
    scale: 1.0

    QQ.MouseArea {
        id: mouseAreaPrevious
        anchors.fill: parent

        hoverEnabled: true

        onClicked: {
            image.clicked()
        }
    }

    QQ.Behavior on scale {
        QQ.NumberAnimation { duration: 50 }
    }

    states: [
        QQ.State {
            name: "hoverState"
            when: mouseAreaPrevious.containsMouse

            QQ.PropertyChanges {
                image {
                    scale: 1.1
                }
            }
        }
    ]
}
