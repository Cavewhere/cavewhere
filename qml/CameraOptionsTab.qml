import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.1
import Cavewhere 1.0

Item {
    id: itemId

    implicitWidth: columnLayoutId.width

    property GLTerrainRenderer viewer

    ScrollView {
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

            GroupBox {
                title: "Azimuth"
                Item {
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

            GroupBox {
                title: "Vertical Angle"

                Item {
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

            GroupBox {
                title: "Projection"

                Item {
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
