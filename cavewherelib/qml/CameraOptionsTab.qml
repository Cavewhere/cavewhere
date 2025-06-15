import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: itemId
    objectName: "cameraOptions"

    implicitWidth: columnLayoutId.width + 10

    property GLTerrainRenderer viewer

    QC.ScrollView {
        anchors.fill: parent

        ColumnLayout {
            id: columnLayoutId

            x: 5
            // width: childrenRect.width + x * 2
            anchors.top: parent.top
            anchors.margins: 5

            QC.Button {
                objectName: "resetViewButton"
                Layout.alignment: Qt.AlignHCenter
                text: " Reset"
                icon.source: "qrc:/twbs-icons/icons/repeat.svg"
                onClicked: {
                    itemId.viewer.turnTableInteraction.resetView()
                }
            }

            QC.GroupBox {
                title: "Azimuth"
                Layout.fillWidth: true
                CameraAzimuthSettings {
                    id: azimuthSettingsId
                    turnTableInteraction: itemId.viewer.turnTableInteraction
                }
            }

            QC.GroupBox {
                title: "Vertical Angle"
                Layout.fillWidth: true

                CameraVerticalAngleSettings {
                    Layout.alignment: Qt.AlignHCenter
                    turnTableInteraction: itemId.viewer.turnTableInteraction
                }
            }

            QC.GroupBox {
                title: "Projection"
                Layout.fillWidth: true

                CameraProjectionSettings {
                    id: projectionSettingsId
                    viewer: itemId.viewer.renderer
                }
            }
        }
    }
}
