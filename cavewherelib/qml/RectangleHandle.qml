import QtQuick as QQ

QQ.Item {
    id: handle

    width: imageId.width
    height: imageId.height

    property url imageSource
    // property url selectedImageSource
    property alias imageRotation: imageId.rotation
    property alias imageSourceSize: imageId.sourceSize

    signal dragStarted(QQ.point startPoint)
    signal dragDelta(QQ.vector2d delta)


    QQ.Image {
        id: imageId
        visible: !hoverHandler.hovered
        source: handle.imageSource
    }

    QQ.Image {
        id: selectImageId
        rotation: imageId.rotation
        visible: hoverHandler.hovered
        source: handle.imageSource
        sourceSize: imageId.sourceSize

        layer.enabled: true
        layer.effect: QQ.ShaderEffect {
            property QQ.color pixelColor: Qt.darker("#8dcdff") //Selection color
            fragmentShader: "qrc:/shaders/toColor.frag.qsb"
        }
    }

    QQ.HoverHandler {
        id: hoverHandler
    }

    QQ.DragHandler {
        id: dragHandler

        dragThreshold: 1
        target: null

        onActiveTranslationChanged: {
            handle.dragDelta(activeTranslation)
        }

        onActiveChanged: {
            if(active) {
                let startPoint = handle.mapToItem(null,
                                              handle.width / 2.0,
                                              handle.height / 2.0);
                handle.dragStarted(startPoint)
            }
        }
    }

    // Text {
    //     color: dragHandler.active ? "darkgreen" : "black"
    //     text: dragHandler.activeTranslation.x.toFixed(1) + "," + dragHandler.activeTranslation.y.toFixed(1)
    //     x: dragHandler.activeTranslation.x - width / 2
    //     y: dragHandler.activeTranslation.y - height
    // }
}
