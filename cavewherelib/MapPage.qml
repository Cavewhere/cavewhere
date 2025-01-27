// pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import QtQuick.Dialogs as Dialogs
import cavewherelib
import "Utils.js" as Utils

QQ.Item {
    id: mapPageId
    objectName: "mapPage"

    // implicitWidth: quickSceneView.implicitWidth + sidebarColumnId.implicitWidth

    property alias hideExternalTools: selectionTool.visible
    property GLTerrainRenderer view

    ErrorDialog {
        id: errorDialog
        model: screenCaptureManagerId.errorModel
    }

    SelectExportAreaTool {
        id: selectionTool
        parent: mapPageId.view
        view: mapPageId.view
        manager: screenCaptureManagerId
        visible: false
    }

    ChoosePaperSizeInteraction {
        id: paperSizeInteractionId
        parent: mapPageId.view
        visible: false;
        paperMarginGroupBox: mapOptionsId.paperMarginGroupBox

        onWidthChanged: mapOptionsId.paperComboBox.updatePaperRectangleFromModel()
        onHeightChanged: mapOptionsId.paperComboBox.updatePaperRectangleFromModel()
    }

    CaptureManager {
        id: screenCaptureManagerId
        objectName: "screenCaptureManager"
        view: mapPageId.view.renderer
        viewport: Qt.rect(paperSizeInteractionId.captureRectangle.x,
                          paperSizeInteractionId.captureRectangle.y,
                          paperSizeInteractionId.captureRectangle.width,
                          paperSizeInteractionId.captureRectangle.height)
        paperSize: Qt.size(paperSizeInteractionId.paperRectangle.paperWidth,
                           paperSizeInteractionId.paperRectangle.paperHeight)
        //        screenPaperSize: Qt.size(paperSizeInteraction.paperRectangle.width,
        //                                 paperSizeInteraction.paperRectangle.height)
        onFinishedCapture: Qt.openUrlExternally(filename)
    }

    QC.SplitView {
        id: rowLayoutId

        anchors.fill: parent

        QuickSceneView {
            id: quickSceneView
            QC.SplitView.preferredWidth: rowLayoutId.width * 0.7
            // implicitWidth: 300
            // Layout.fillHeight: true
            // Layout.fillWidth: true
            scene: screenCaptureManagerId.scene

            CaptureItemManiputalor {
                anchors.fill: parent;
                view: quickSceneView
                manager: screenCaptureManagerId
            }
        }

        QC.ScrollView {
            padding: 5

            ColumnLayout {

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

                BreakLine {  }

                MapOptions {
                    id: mapOptionsId
                    view: mapPageId.view
                    screenCaptureManager: screenCaptureManagerId
                    paperSizeInteraction: paperSizeInteractionId
                }
            }
        }
    }

    // QQ.Rectangle {
    //     anchors.fill: parent
    //     RenderingView {
    //         id: viewId
    //         anchors.fill: parent
    //         // visible: false;
    //         scene: RootData.regionSceneManager.scene
    //     }
    // }

    HelpQuoteBox {
        pointAtObject: addLayerButton
        pointAtObjectPosition: Qt.point(addLayerButton.width / 2.0, addLayerButton.height)
        triangleOffset: 0.0
        visible: screenCaptureManagerId.numberOfCaptures === 0
        text: "Add a new layer to create a map"
    }
}
