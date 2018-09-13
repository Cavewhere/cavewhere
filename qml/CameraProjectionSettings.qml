import QtQuick 2.1
import QtQuick.Layouts 1.1
import Cavewhere 1.0

ColumnLayout {
    id: itemId

    property RegionViewer viewer;
    spacing: 5

    implicitWidth: Math.max(projectionSlider.width, fieldOfViewRowLayout.width)

    ProjectionSlider {
        id: projectionSlider
//        anchors.left: parent.left
//        anchors.right: parent.right
        viewer: itemId.viewer
    }

    RowLayout {
        id: fieldOfViewRowLayout
        visible: viewer.perspectiveProjection.enabled
        LabelWithHelp {
            text: "Field of View"
            helpArea: fieldOfViewId
        }

        ClickTextInput {
            text: Number(viewer.perspectiveProjection.fieldOfView).toFixed(1)

            onFinishedEditting: {
                fieldOfViewAnimationId.to = newText
                fieldOfViewAnimationId.restart()
            }
        }

        Text {
            text: "°"
        }

        NumberAnimation {
            id: fieldOfViewAnimationId
            target: viewer.perspectiveProjection;
            property: "fieldOfView";
            duration: 200;
            easing.type: Easing.InOutQuad }

    }

    HelpArea {
        id: fieldOfViewId
        text: "The FOV (field of view) is the visible angle of the view.
The FOV is valid between 0.0° to 180.0°. A low FOV will make the view zoom in, while a
high FOV (near 180) will give a fish eye effect.  A good number for FOV is 55°"
    }
}
