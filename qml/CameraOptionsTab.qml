import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.4
import Cavewhere 1.0

QQ.Item {
    id: itemId

    implicitWidth: columnLayoutId.implicitWidth

    property TurnTableInteraction turnTableInteraction

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            id: columnLayoutId

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

                QQ.Item {
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



                    QQ.Item {
                        CameraProjectionSettings {
                            id: projectionSettingsId
                            camera: itemId.turnTableInteraction.camera
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }
}
