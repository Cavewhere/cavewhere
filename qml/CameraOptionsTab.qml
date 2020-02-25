import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.4 as QC
import Cavewhere 1.0

QQ.Item {
    id: itemId

    implicitWidth: columnLayoutId.width

    property GLTerrainRenderer viewer

    QC.ScrollView {
        anchors.fill: parent

        ColumnLayout {
            id: columnLayoutId

            property double maxWidth: Math.max(azimuthSettingsId.width,
                                               Math.max(verticalAngleSettingsId.width,
                                                        projectionSettingsId.width))

            x: 5
            width: childrenRect.width + x * 2
            anchors.top: parent.top
            anchors.margins: 5

            QC.GroupBox {
                title: "Azimuth"
                QQ.Item {
                    width: columnLayoutId.maxWidth
                    height: azimuthSettingsId.height
                    CameraAzimuthSettings {
                        id: azimuthSettingsId
                        turnTableInteraction: itemId.viewer.turnTableInteraction
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }

            QC.GroupBox {
                title: "Vertical Angle"

                QQ.Item {
                    width: columnLayoutId.maxWidth
                    height: verticalAngleSettingsId.height
                    CameraVerticalAngleSettings {
                        id: verticalAngleSettingsId
                        turnTableInteraction: itemId.viewer.turnTableInteraction
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }

            QC.GroupBox {
                title: "Projection"

                QQ.Item {
                    width: columnLayoutId.maxWidth
                    height: projectionSettingsId.height
                    CameraProjectionSettings {
                        id: projectionSettingsId
                        viewer: itemId.viewer
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }
        }
    }
}
