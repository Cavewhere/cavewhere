import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib
import QtQuick.Controls as QC
ColumnLayout {
    id: itemId

    property RegionViewer viewer;

    anchors.fill: parent

    ColumnLayout {

        spacing: 5

        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true


        ProjectionSlider {
            id: projectionSlider
            objectName: "projectionSlider"
            viewer: itemId.viewer
        }

        RowLayout {
            id: fieldOfViewRowLayout
            visible: itemId.viewer.perspectiveProjection.enabled
            LabelWithHelp {
                text: "Field of View"
                helpArea: fieldOfViewId
            }

            ClickTextInput {
                text: Number(itemId.viewer.perspectiveProjection.fieldOfView).toFixed(1)

                onFinishedEditting: (newText) => {
                                        fieldOfViewAnimationId.to = newText
                                        fieldOfViewAnimationId.restart()
                                    }
            }

            Text {
                text: "°"
            }

            QQ.NumberAnimation {
                id: fieldOfViewAnimationId
                target: itemId.viewer.perspectiveProjection;
                property: "fieldOfView";
                duration: 200;
                easing.type: QQ.Easing.InOutQuad }

        }

        HelpArea {
            id: fieldOfViewId
            Layout.fillWidth: true
            text: "The FOV (field of view) is the visible angle of the view." +
                  "The FOV is valid between 0.0° to 180.0°. A low FOV will make the view zoom in, while a" +
                  "high FOV (near 180) will give a fish eye effect.  A good number for FOV is 55°"
        }
    }
}
