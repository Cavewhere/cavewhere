import QtQuick as QQ

QQ.Item {
    id: handle

    width: imageId.width
    height: imageId.height

    property url imageSource
    property url selectedImageSource
    property alias imageRotation: imageId.rotation

    signal dragStarted()
    signal dragDelta(QQ.vector2d delta)
//    signal mousePositionChanged(var position)

    QQ.Image {
        id: imageId
        visible: !hoverHandler.hovered
        source: handle.imageSource
    }

    QQ.Image {
        id: selectImageId
        rotation: imageId.rotation
        visible: hoverHandler.hovered
        source: handle.selectedImageSource
    }

    QQ.HoverHandler {
        id: hoverHandler
    }



    QQ.DragHandler {
        id: dragHandler

        // property QQ.vector2d oldTranslation

        dragThreshold: 1
        target: null

        // grabPermissions: QQ.PointerHandler.CanTakeOverFromAnything | PointerHandler.TakeOverForbidden


        // onGrabChanged: (transition, point) => {
        //                    console.log(QQ.PointerDevice.GrabPassive + "transition:" + transition + " ponit:" + point)
        //                }

        onActiveTranslationChanged: {
            // let delta = Qt.vector2d(oldTranslation.x - activeTranslation.x, oldTranslation.y - activeTranslation.y)
            // console.log("Delta:" + delta)
            // handle.dragDelta(delta)
            // oldTranslation = activeTranslation
            handle.dragDelta(activeTranslation)
        }

        onActiveChanged: {
            if(active) {
                dragStarted()
            }
        }

        // onCentroidChanged: {
        //     handle.dragDelta(centroid.position)
        // }

    }


    Text {
        color: dragHandler.active ? "darkgreen" : "black"
        text: dragHandler.activeTranslation.x.toFixed(1) + "," + dragHandler.activeTranslation.y.toFixed(1)
        x: dragHandler.activeTranslation.x - width / 2
        y: dragHandler.activeTranslation.y - height
    }
}
