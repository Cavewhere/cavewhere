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
        visible: mapPageId.visible
    }

    ChoosePaperSizeInteraction {
        id: paperSizeInteractionId
        parent: mapPageId.view
        visible: false; //view !== null
        paperMarginGroupBox: mapOptionsId.paperMarginGroupBox

        onWidthChanged: mapOptionsId.paperComboBox.updatePaperRectangleFromModel()
        onHeightChanged: mapOptionsId.paperComboBox.updatePaperRectangleFromModel()
    }

    CaptureManager {
        id: screenCaptureManagerId
        view: mapPageId.view
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
                manager: screenCaptureManagerId
            }

        }

        QC.ScrollView {
            QQ.Item {
                x: 3
                y: 3
                implicitWidth: mapOptionsId.implicitWidth + 3
                implicitHeight: mapOptionsId.implicitHeight + 3
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

    onViewChanged: {
        console.log("View changed:" + view)
    }
}
