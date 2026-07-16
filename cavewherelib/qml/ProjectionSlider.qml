/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts
import QtQuick.Controls as QC
QQ.Item {
    id: cameraSettingsId

    implicitWidth: columnLayoutId.width
    implicitHeight: columnLayoutId.height

    property ProjectionTransition projectionTransition

    ColumnLayout {
        id: columnLayoutId
        RowLayout {
            InformationButton {
                showItemOnClick: projectionSliderHelpAreaId
            }

            QC.Label {
                text: "Orthognal"
            }

            ToggleSlider {
                id: sliderId
                objectName: "slider"
                leftText: "     "
                rightText: "     "

                // The slider is pure input: it scrubs the controller's
                // transition. The controller owns the reconcile, the matrix
                // blend, and settling onto a typed projection at the endpoints.
                onSliderPosChanged: {
                    if(cameraSettingsId.projectionTransition !== null) {
                        cameraSettingsId.projectionTransition.progress = sliderPos
                    }
                }
            }

            QC.Label {
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
}
