/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts

QQ.Item {
    id: cameraSettingsId

    implicitWidth: columnLayoutId.width
    implicitHeight: columnLayoutId.height

    property RegionViewer viewer
//    property alias orthoProjection: orthoProjectionId
//    property alias perspectiveProjection: perspectiveProjectionId

    ColumnLayout {
        id: columnLayoutId
        RowLayout {
            InformationButton {
                showItemOnClick: projectionSliderHelpAreaId
            }

            Text {
                text: "Orthognal"
            }

            ToggleSlider {
                id: sliderId
                objectName: "slider"
                leftText: "     "
                rightText: "     "

                onIsLeftChanged: {
                    if(cameraSettingsId.viewer !== null) {
                        cameraSettingsId.viewer.orthoProjection.enabled = isLeft
                    }
                }

                onIsRightChanged: {
                    if(cameraSettingsId.viewer !== null) {
                        cameraSettingsId.viewer.perspectiveProjection.enabled = isRight
                    }
                }
            }

            Text {
                text: "Perspective"
            }
        }

        HelpArea {
            id: projectionSliderHelpAreaId
            Layout.fillWidth: true
            text: "CaveWhere supports two types of projections:<ul>" +
"<li>Orthogonal - 3D objects always appear the same size no matter how close" +
"or far away they are to the view. When printing or drawing a cave map, this is the" +
"projection you want to use." +
"<li>Perspective – 3D objects appear smaller when they are further away and" +
" bigger when they are closer to the camera."
        }
    }

    Matrix4x4Animation {
        id: matrix4x4AnimationId
        startValue: cameraSettingsId.viewer.orthoProjection.matrix
        duration: 1000
        endValue: cameraSettingsId.viewer.perspectiveProjection.matrix
        currentTime: sliderId.sliderPos * 1000

        property real position: sliderId.sliderPos

        onPositionChanged: {
            if(position != 0.0 && position != 1.0) {
                cameraSettingsId.viewer.camera.setCustomProjection(currentValue)
            }
        }
    }
}
