/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

Item {
    id: cameraSettingsId

    implicitWidth: columnLayoutId.width
    implicitHeight: columnLayoutId.height

    property Camera camera

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
                leftText: "     "
                rightText: "     "

                onIsLeftChanged: {
                    if(camera !== null) {
                        camera.projectionType = Camera.Orthoginal
                    }
                }

                onIsRightChanged: {
                    if(camera !== null) {
                        camera.projectionType = Camera.Perspective
                    }
                }
            }

            Text {
                text: "Perspective"
            }
        }

        HelpArea {
            id: projectionSliderHelpAreaId
            text: "Cavewhere supports two types of projections:<ul>
<li>Orthogonal - 3D objects always appear the same size no matter how close
or far away they are to the view. When printing or drawing a cave map, this is the
projection you want to use.
<li>Perspective – 3D objects appear smaller when they are further away and
 bigger when they are closer to the camera."
        }
    }

    Matrix4x4Animation {
        id: matrix4x4AnimationId
        startValue: camera.orthProjectionMatrix
        endValue: camera.perspectiveProjectionMatrix
        duration: 1000
        currentTime: sliderId.sliderPos * 1000

        property real position: sliderId.sliderPos

        onPositionChanged: {
            if(position != 0.0 && position != 1.0) {
                camera.setCustomProjection(currentValue)
            }
        }
    }
}
