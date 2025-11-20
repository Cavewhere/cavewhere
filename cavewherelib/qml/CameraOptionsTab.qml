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

            GridLayout {
                id: gltfLayoutId

                function updateGLTF() {
                    RootData.regionSceneManager.gltf.setRotation(
                                Number(xAxis.text),
                                Number(yAxis.text),
                                Number(zAxis.text),
                                angleSliderId.value
                                );
                }

                columns: 3
                QC.Label {
                    text: "x"
                }
                QC.Label {
                    text: "y"
                }
                QC.Label {
                    text: "z"
                }
                QC.TextField {
                    id: xAxis
                    text: "1.0"
                    onTextEdited: {
                        gltfLayoutId.updateGLTF()
                    }
                }
                QC.TextField {
                    id: yAxis
                    text: "0.0"
                    onTextEdited: {
                        gltfLayoutId.updateGLTF()
                    }
                }
                QC.TextField {
                    id: zAxis
                    text: "0.0"
                    onTextEdited: {
                        gltfLayoutId.updateGLTF()
                    }
                }
            }

            RowLayout {
                QC.Label {
                    text: "Angle"
                }
                QC.Slider {
                    id: angleSliderId
                    Layout.fillWidth: true
                    from: -180.0
                    to: 180.0
                    value: 0.0
                    onValueChanged:  {
                        gltfLayoutId.updateGLTF()
                    }
                }
                QC.Label {
                    text: angleSliderId.value.toFixed(2)
                }
            }

            GridLayout {
                id: gltfTranslationLayoutId

                function updateGLTF() {
                    RootData.regionSceneManager.gltf.setTranslation(
                                Number(xAxisTranslate.text),
                                Number(yAxisTranslate.text),
                                Number(zAxisTranslate.text),
                                );
                }

                columns: 3
                QC.Label {
                    text: "x"
                }
                QC.Label {
                    text: "y"
                }
                QC.Label {
                    text: "z"
                }
                QC.TextField {
                    id: xAxisTranslate
                    text: "0.0"
                    onTextEdited: {
                        gltfTranslationLayoutId.updateGLTF()
                    }
                }
                QC.TextField {
                    id: yAxisTranslate
                    text: "0.0"
                    onTextEdited: {
                        gltfTranslationLayoutId.updateGLTF()
                    }
                }
                QC.TextField {
                    id: zAxisTranslate
                    text: "0.0"
                    onTextEdited: {
                        gltfTranslationLayoutId.updateGLTF()
                    }
                }
            }
        }
    }
}
