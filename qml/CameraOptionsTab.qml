import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.1
import Cavewhere 1.0

Item {
    id: itemId

    property RegionViewer viewer

    ScrollView {
        anchors.fill: parent

        ColumnLayout {
            id: columnLayoutId
            x: 5
            width: itemId.width - x * 2
            anchors.top: parent.top
            anchors.margins: 5

            GroupBox {
                title: "Azimuth"
                Item {
                    width: columnLayoutId.width - 15
                    height: childrenRect.height
                    CameraAzimuthSettings {
                        viewer: itemId.viewer
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }

            GroupBox {
                title: "Vertical Angle"

                Item {
                    width: columnLayoutId.width - 15
                    height: childrenRect.height
                    CameraVerticalAngleSettings {
                        viewer: itemId.viewer
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }

            GroupBox {
                title: "Projection"

                Item {
                    width: columnLayoutId.width - 15
                    height: childrenRect.height
                    CameraProjectionSettings {
                        viewer: itemId.viewer
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }
        }
    }
}
