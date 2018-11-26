import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.4
import Cavewhere 1.0

Item {
    id: itemId

    implicitWidth: columnLayoutId.implicitWidth

    property TurnTableInteraction turnTableInteraction

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            id: columnLayoutId

//            property double maxWidth: Math.max(azimuthSettingsId.implicitWidth,
//                                               Math.max(verticalAngleSettingsId.implicitWidth,
//                                                        projectionSettingsId.implicitWidth))

//            x: 5
//            width: childrenRect.width + x * 2
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 5

            Button {
                text: "Reset View"
                onClicked: {
                    turnTableInteraction.resetView()
                }
            }

            GroupBox {
                title: "Azimuth"
                Layout.fillWidth: true
                CameraAzimuthSettings {
                    id: azimuthSettingsId
                    turnTableInteraction: itemId.turnTableInteraction
                    Layout.fillWidth: true

                }
            }

            GroupBox {
                title: "Vertical Angle"
                Layout.fillWidth: true

                CameraVerticalAngleSettings {
                    id: verticalAngleSettingsId
                    turnTableInteraction: itemId.turnTableInteraction
                    anchors.left: parent.left
                    anchors.right: parent.right

                }
            }

            GroupBox {
                title: "Projection"
                Layout.fillWidth: true



                CameraProjectionSettings {
                    id: projectionSettingsId
                    camera: itemId.turnTableInteraction.camera
                    Layout.fillWidth: true
                }
            }
        }
    }
}
