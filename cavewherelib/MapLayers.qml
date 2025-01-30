pragma ComponentBehavior: Bound

import QtQuick
import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

ColumnLayout {
    id: mapLayersId

    required property CaptureManager screenCaptureManager
    required property CaptureItemManiputalor captureItemManiputlor
    required property GLTerrainRenderer view
    property alias addLayerButton: addLayerButton

    SelectExportAreaTool {
        id: selectionTool
        parent: mapLayersId.view
        view: mapLayersId.view
        manager: mapLayersId.screenCaptureManager
        visible: false
    }

    QC.Button {
        id: addLayerButton
        objectName: "addLayerButton"
        text: " Add Layer"
        icon.source: "qrc:/twbs-icons/icons/layers.svg"
        onClicked: {
            selectionTool.activate()
            RootData.pageSelectionModel.gotoPageByName(null, "View");
        }
    }

    QC.GroupBox {
        id: layersId
        title: "Layers"

        QC.ScrollView {
            implicitWidth: 200
            implicitHeight: 100

            QQ.ListView {
                id: layerListViewId
                objectName: "layerListView"

                implicitWidth: 200
                implicitHeight: 100


                model: mapLayersId.screenCaptureManager

                anchors.left: parent.left
                anchors.right: parent.right

                function layerObject(i) {
                    let modelIndex = mapLayersId.screenCaptureManager.index(i);
                    let layerObject = mapLayersId.screenCaptureManager.data(modelIndex, CaptureManager.LayerObjectRole);
                    if(layerObject === undefined) {
                        return null;
                    }
                    return layerObject;
                }

                function updateLayerPropertyWidget() {
                    let layer = layerObject(currentIndex)
                    layerProperties.layerObject = layer;
                }

                delegate: Text {
                    id: textDelegateId
                    objectName: "layerDelegate" + index
                    required property string layerNameRole
                    required property int index

                    anchors.left: parent ? parent.left : undefined
                    anchors.right: parent ? parent.right : undefined
                    anchors.leftMargin: 5

                    text: layerNameRole

                    QQ.MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton

                        onClicked: (mouse) => {
                                       layerListViewId.currentIndex = textDelegateId.index

                                       if(mouse.button == Qt.RightButton) {
                                           layerRightClickMenu.capture = layerProperties.layerObject
                                           layerRightClickMenu.popup()
                                       }
                                   }
                    }
                }

                highlight: QQ.Rectangle {
                    color: "#8AC6FF"
                    radius: 3
                }

                onCurrentIndexChanged: {
                    let layer = layerObject(currentIndex)
                    layerProperties.layerObject = layer;
                    mapLayersId.captureItemManiputlor.select(layer);
                }

                QQ.Connections {
                    target: mapLayersId.captureItemManiputlor
                    function onSelectedItemChanged() {
                        let captureInteraction = mapLayersId.captureItemManiputlor.selectedItem as CaptureItemInteraction
                        console.log("Selected item changed:" + captureInteraction)
                        if(captureInteraction) {
                            layerListViewId.currentIndex = mapLayersId.screenCaptureManager.indexOf(captureInteraction.captureItem)
                        }
                    }
                }

                QQ.Connections {
                    target: mapLayersId.screenCaptureManager
                    function onRowsInserted() { layerListViewId.updateLayerPropertyWidget() }
                    function onRowsRemoved() { layerListViewId.updateLayerPropertyWidget() }
                }
            }
        }
    }

    QC.Menu {
        id: layerRightClickMenu

        property CaptureViewport capture: null

        QC.MenuItem {
            text: "Remove"
            onTriggered: {
                if(layerRightClickMenu.capture == null) {
                    console.warn("Capture is null")
                    return;
                }
                mapLayersId.screenCaptureManager.removeCaptureViewport(layerRightClickMenu.capture);
            }
        }
    }

    // QC.GroupBox {
    //     id: layerGroupsId
    //     title: "Layer Groups"

    //     QC.ScrollView {
    //         implicitWidth: paperMarginGroupBoxId.width
    //         implicitHeight: 100

    //         QQ.ListView {
    //             id: groupListViewId

    //             implicitWidth: paperMarginGroupBoxId.width
    //             implicitHeight: 100

    //             model: mapLayersId.screenCaptureManager.groupModel

    //             delegate: QQ.Rectangle {
    //                 id: delegateId
    //                 required property string captureNameRole

    //                 width: 100
    //                 height: 100
    //                 //                                color: "red"

    //                 // QQ.VisualDataModel {
    //                 //     id: visualModel
    //                 //     model: mapLayersId.screenCaptureManager.groupModel
    //                 //     rootIndex: mapLayersId.screenCaptureManager.groupModel.index(index)

    //                 //     delegate: Text {
    //                 //         text: captureNameRole
    //                 //     }
    //                 // }

    //                 QQ.ListView {
    //                     // model: visualModel
    //                     model: mapLayersId.screenCaptureManager.groupModel
    //                     anchors.fill: parent
    //                     delegate: Text {
    //                         text: delegateId.captureNameRole
    //                     }


    //                 }
    //             }
    //         }
    //     }
    // }


    QC.GroupBox {
        id: layerProperties

        property var layerObject: null

        title: "" //layerObject !== null ? "Properies of " + layerObject.name : ""
        visible: false //layerObject !== null

        ColumnLayout {

            PaperScaleInput {
                id: paperScaleInputId
                usingInteraction: false
                scaleObject: null
            }

            RowLayout {
                Text {
                    text: "Size"
                }

                ClickTextInput {
                    id: sizeWidthInputId
                    text: "" //layerProperties.layerObject !== null ? layerProperties.layerObject.paperSizeOfItem.width : ""
                }

                Text {
                    text: "x"
                }

                ClickTextInput {
                    id: sizeHeightInputId
                    text: "" //layerProperties.layerObject !== null ? layerProperties.layerObject.paperSizeOfItem.height : ""
                }

            }

            RowLayout {
                Text {
                    text: "Position"
                }

                Text {
                    text: "x:"
                }

                ClickTextInput {
                    id: posXInputId
                    text: ""
                }

                Text {
                    text: "y:"
                }

                ClickTextInput {
                    id: posYInputId
                    text: ""
                }
            }

            RowLayout {
                Text {
                    text: "Rotation"
                }

                ClickTextInput {
                    id: rotationInput
                    text: ""
                }
            }


        }

        states: [
            QQ.State {
                when: typeof(layerProperties.layerObject) !== "undefined" && layerProperties.layerObject !== null

                QQ.PropertyChanges {
                    layerProperties {
                        title: "Properies of " + layerProperties.layerObject.name
                        visible: true
                    }
                }

                QQ.PropertyChanges {
                    paperScaleInputId {
                        scaleObject: layerProperties.layerObject.scaleOrtho
                    }
                }

                QQ.PropertyChanges {
                    sizeWidthInputId {
                        text: Utils.fixed(layerProperties.layerObject.paperSizeOfItem.width, 3);
                        onFinishedEditting: {
                            layerProperties.layerObject.setPaperWidthOfItem(newText)
                        }
                    }
                }

                QQ.PropertyChanges {
                    sizeHeightInputId {
                        text: Utils.fixed(layerProperties.layerObject.paperSizeOfItem.height, 3)
                        onFinishedEditting: {
                            layerProperties.layerObject.setPaperHeightOfItem(newText)
                        }
                    }
                }

                QQ.PropertyChanges {
                    posXInputId {
                        text: Utils.fixed(layerProperties.layerObject.positionOnPaper.x, 3)
                        onFinishedEditting: {
                            var y = layerProperties.layerObject.positionOnPaper.y
                            layerProperties.layerObject.positionOnPaper = Qt.point(newText, y);
                        }
                    }
                }

                QQ.PropertyChanges {
                    posYInputId {
                        text: Utils.fixed(layerProperties.layerObject.positionOnPaper.y, 3)
                        onFinishedEditting: {
                            var x = layerProperties.layerObject.positionOnPaper.x
                            layerProperties.layerObject.positionOnPaper = Qt.point(x, newText);
                        }
                    }
                }

                QQ.PropertyChanges {
                    rotationInput {
                        text: Utils.fixed(layerProperties.layerObject.rotation, 2);
                        onFinishedEditting: {
                            layerProperties.layerObject.rotation = newText
                        }
                    }
                }
            }
        ]
    }
}
