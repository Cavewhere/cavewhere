import QtQuick 2.0
import Cavewhere 1.0

Item {
    id: cameraSettingsId

    property RegionViewer viewer
    property alias orthoProjection: orthoProjectionId
    property alias perspectiveProjection: perspectiveProjectionId

    ToggleSlider {
        id: sliderId
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 5
        anchors.topMargin: 5
        leftText: "Perspective"
        rightText: "Orthogonal"
    }

    OrthogonalProjection {
        id: orthoProjectionId
        enabled: sliderId.isLeft //On the left
        viewer: cameraSettingsId.viewer
    }

    PerspectiveProjection {
        id: perspectiveProjectionId
        enabled: sliderId.isRight //!sliderId.isLeft //On the right
        viewer: cameraSettingsId.viewer
    }


    Matrix4x4Animation {
        id: matrix4x4AnimationId
        startValue: orthoProjectionId.matrix
        endValue: perspectiveProjectionId.matrix
        duration: 1000
        currentTime: sliderId.sliderPos * 1000

        property real position: sliderId.sliderPos

        onPositionChanged: {
            viewer.camera.setCustomProjection(currentValue)
        }
    }
}
