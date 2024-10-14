pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import QtQuick.Dialogs as Dialogs
import cavewherelib
import "Utils.js" as Utils

QQ.Item {
    id: exportViewTabId

    implicitWidth: quickSceneView.implicitWidth + sidebarColumnId.implicitWidth

    property GLTerrainRenderer view
    property alias hideExternalTools: selectionTool.visible

    function updatePaperRectangle(paperWidth, paperHeight) {
        var i = paperSizeInteraction

        i.paperRectangle.paperWidth = i.paperRectangle.landScape ? paperHeight : paperWidth
        i.paperRectangle.paperHeight = i.paperRectangle.landScape ? paperWidth : paperHeight
        //        paperMarginGroupBoxId.unit = paperunits


        //Stretch the paper to the max width or height of the screen
        var paperAspect = paperWidth / paperHeight;
        if(i.paperRectangle.landScape) {
            paperAspect = 1.0 / paperAspect;
        }

        var viewerAspect = view.width / view.height;
        var aspect = paperAspect / viewerAspect
        if(aspect < 1.0) {
            i.paperRectangle.width = view.width * aspect
            i.paperRectangle.height = view.height
        } else {
            var aspect = viewerAspect / paperAspect
            i.paperRectangle.width = view.width
            i.paperRectangle.height = view.height * aspect
        }
    }

    ErrorDialog {
        id: errorDialog
        model: screenCaptureManagerId.errorModel
    }

    SelectExportAreaTool {
        id: selectionTool
        parent: exportViewTabId.view
        view: exportViewTabId.view
        manager: screenCaptureManagerId
        visible: exportViewTabId.visible
    }

    ChoosePaperSizeInteraction {
        id: paperSizeInteraction
        parent: exportViewTabId.view
        visible: false; //view !== null
        paperMarginGroupBox: paperMarginGroupBoxId

        onWidthChanged: paperComboBoxId.updatePaperRectangleFromModel()
        onHeightChanged: paperComboBoxId.updatePaperRectangleFromModel()
    }

    CaptureManager {
        id: screenCaptureManagerId
        view: exportViewTabId.view
        viewport: Qt.rect(paperSizeInteraction.captureRectangle.x,
                          paperSizeInteraction.captureRectangle.y,
                          paperSizeInteraction.captureRectangle.width,
                          paperSizeInteraction.captureRectangle.height)
        paperSize: Qt.size(paperSizeInteraction.paperRectangle.paperWidth,
                           paperSizeInteraction.paperRectangle.paperHeight)
        //        screenPaperSize: Qt.size(paperSizeInteraction.paperRectangle.width,
        //                                 paperSizeInteraction.paperRectangle.height)
        onFinishedCapture: Qt.openUrlExternally(filename)
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
                screenCaptureManagerId.removeCaptureViewport(layerRightClickMenu.capture);
            }
        }
    }

    RowLayout {
        id: rowLayoutId

        anchors.fill: parent

        QuickSceneView {
            id: quickSceneView
            implicitWidth: 300
            Layout.fillHeight: true
            Layout.fillWidth: true
            scene: screenCaptureManagerId.scene

            CaptureItemManiputalor {
                anchors.fill: parent;
                manager: screenCaptureManagerId
            }
        }

        ColumnLayout {
            id: sidebarColumnId

            RowLayout {
                Layout.fillWidth: true

                QC.GroupBox {
                    title: "File type"

                    QC.ComboBox {
                        id: fileTypeExportComboBox
                        model: screenCaptureManagerId.fileTypes
                    }
                }

                Button {
                    text: "Export"
                    enabled: screenCaptureManagerId.memoryLimit > screenCaptureManagerId.memoryRequired || screenCaptureManagerId.memoryLimit < 0.0
                    onClicked: {
                        exportDialogId.open();
                        //                    screenCaptureManagerId.filename = "file://Users/vpicaver/Documents/Projects/cavewhere/testcase/test.png"
                        //                    screenCaptureManagerId.capture()
                    }
                }
            }

            ColumnLayout {
                RowLayout {
                    Text {
                        id: memoryRequiredId

                        function formatMemory(memoryMB) {
                            var useGB = function() {
                                return memoryMB / 1024.0 > 1.0;
                            }

                            var requireMemory = function() {
                                var toUnit = useGB() ? 1024.0 : 1.0;
                                return memoryMB / toUnit;
                            }

                            var unit = useGB() ? "GB" : "MB";

                            return requireMemory().toFixed(2) + unit;
                        }

                        text: "Memory Required: " + formatMemory(screenCaptureManagerId.memoryRequired)
                    }

                    InformationButton {
                        showItemOnClick: memoryHelpAreaId
                    }
                }

                HelpArea {
                    id: memoryHelpAreaId
                    Layout.fillWidth: true;
                    text: "The amout of RAM that CaveWhere requires to save image. Using more memory than what's on computer my cause your computer to hang! CaveWhere may temporarily use equal or double the amount of disk space required by the memory required";
                }

                Text {
                    visible: screenCaptureManagerId.memoryLimit > 0.0
                    text: "Memory Limit: " + memoryRequiredId.formatMemory(screenCaptureManagerId.memoryLimit);
                }

            }

            BreakLine {  }

            Button {
                text: "Options"
                onClicked: {
                    optionsScrollViewId.visible = !optionsScrollViewId.visible
                }

            }

            QQ.Item {
                implicitWidth: 1
                visible: !optionsScrollViewId.visible
                Layout.fillHeight: true //!optionsScrollViewId.visible
            }

            QC.ScrollView {
                id: optionsScrollViewId
                visible: false

                Layout.fillHeight: true

                implicitWidth: columnLayoutId.width + 15

                ColumnLayout {
                    id: columnLayoutId



                    QC.GroupBox {
                        id: paperSizeId
                        title: "Paper Size"

                        ColumnLayout {

                            QC.ComboBox {
                                id: paperComboBoxId
                                model: paperSizeModel
                                textRole: "name"

                                //                    property alias paperRectangle: paperSizeInteraction.paperRectangle

                                function updatePaperRectangleFromModel() {
                                    var item = paperSizeModel.get(currentIndex);

                                    if(item) {
                                        exportViewTabId.updatePaperRectangle(item.width, item.height)
                                    }
                                }

                                function updateDefaultMargins() {
                                    var item = paperSizeModel.get(currentIndex);
                                    paperMarginGroupBoxId.setDefaultLeft(item.defaultLeftMargin)
                                    paperMarginGroupBoxId.setDefaultRight(item.defaultRightMargin)
                                    paperMarginGroupBoxId.setDefaultTop(item.defaultTopMargin)
                                    paperMarginGroupBoxId.setDefaultBottom(item.defaultBottomMargin)
                                }

                                onCurrentIndexChanged: {
                                    updatePaperRectangleFromModel()
                                    updateDefaultMargins()
                                }

                                QQ.Component.onCompleted: {
                                    updatePaperRectangleFromModel()
                                    updateDefaultMargins()
                                }
                            }

                            RowLayout {
                                Text {
                                    text: "Width"
                                }

                                ClickTextInput {
                                    id: paperSizeWidthInputId
                                    text: screenCaptureManagerId.paperSize.width
                                    readOnly: paperComboBoxId.currentIndex != 3
                                    onFinishedEditting: (newText) => {
                                        paperSizeModel.setProperty(paperComboBoxId.currentIndex, "width", newText)
                                        paperComboBoxId.updatePaperRectangleFromModel();
                                    }
                                }

                                Text {
                                    text: "in"
                                    font.italic: true
                                }

                                Text {
                                    text: "Height"
                                }

                                ClickTextInput {
                                    text: screenCaptureManagerId.paperSize.height
                                    readOnly: paperSizeWidthInputId.readOnly
                                    onFinishedEditting: (newText) => {
                                        paperSizeModel.setProperty(paperComboBoxId.currentIndex, "height", newText)
                                        paperComboBoxId.updatePaperRectangleFromModel();
                                    }
                                }

                                Text {
                                    text: "in"
                                    font.italic: true
                                }
                            }

                            RowLayout {
                                Text {
                                    text: "Resolution"
                                }

                                QC.SpinBox {
                                    id: resolutionSpinBoxId
                                    value: screenCaptureManagerId.resolution
                                    stepSize: 100
                                    from: 100
                                    to: 600

                                    onValueChanged: {
                                        screenCaptureManagerId.resolution = value
                                    }

                                    QQ.Connections {
                                        target: screenCaptureManagerId
                                        function onResolutionChanged() {
                                            resolutionSpinBoxId.value = screenCaptureManagerId.resolution
                                        }
                                    }
                                }

                                Text {
                                    text: "DPI"
                                    font.italic: true
                                }
                            }
                        }
                    }

                    PaperMarginGroupBox {
                        id: paperMarginGroupBoxId

                        onLeftMarginChanged: screenCaptureManagerId.leftMargin = leftMargin
                        onRightMarginChanged: screenCaptureManagerId.rightMargin = rightMargin
                        onTopMarginChanged: screenCaptureManagerId.topMargin = topMargin
                        onBottomMarginChanged: screenCaptureManagerId.bottomMargin = bottomMargin
                        unit: "in"
                    }

                    QC.GroupBox {
                        id: orientationId
                        title: "Orientation"

                        RowLayout {
                            id: portraitLandscrapSwitch
                            Text {
                                text: "Portrait"
                            }

                            QC.Switch {
                                id: orientationSwitchId
                                onCheckedChanged: {
                                    paperSizeInteraction.paperRectangle.landScape = checked
                                    paperComboBoxId.updatePaperRectangleFromModel()
                                }
                            }

                            Text {
                                text: "Landscrape"
                            }
                        }
                    }

                    QC.GroupBox {
                        id: layersId
                        title: "Layers"

                        QC.ScrollView {
                            QQ.ListView {
                                id: layerListViewId

                                model: screenCaptureManagerId

                                anchors.left: parent.left
                                anchors.right: parent.right

                                function updateLayerPropertyWidget() {
                                    if(currentIndex !== -1) {
                                        var modelIndex = screenCaptureManagerId.index(currentIndex);
                                        var layerObject = screenCaptureManagerId.data(modelIndex, CaptureManager.LayerObjectRole);
                                        layerProperties.layerObject = layerObject
                                    } else {
                                        layerProperties.layerObject = null;
                                    }
                                }

                                delegate: Text {
                                    id: textDelegateId
                                    property string layerNameRole
                                    property int index

                                    anchors.left: parent.left
                                    anchors.right: parent.right
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

                                onCurrentIndexChanged: updateLayerPropertyWidget()

                                QQ.Connections {
                                    target: screenCaptureManagerId
                                    function onRowsInserted() { layerListViewId.updateLayerPropertyWidget() }
                                    function onRowsRemoved() { layerListViewId.updateLayerPropertyWidget() }
                                }
                            }
                        }
                    }

                    QC.GroupBox {
                        id: layerGroupsId
                        title: "Layer Groups"

                        QC.ScrollView {
                            QQ.ListView {
                                id: groupListViewId
                                model: screenCaptureManagerId.groupModel

                                delegate: QQ.Rectangle {
                                    id: delegateId
                                    property string captureNameRole

                                    width: 100
                                    height: 100
                                    //                                color: "red"

                                    // QQ.VisualDataModel {
                                    //     id: visualModel
                                    //     model: screenCaptureManagerId.groupModel
                                    //     rootIndex: screenCaptureManagerId.groupModel.index(index)

                                    //     delegate: Text {
                                    //         text: captureNameRole
                                    //     }
                                    // }

                                    QQ.ListView {
                                        // model: visualModel
                                        model: screenCaptureManagerId.groupModel
                                        anchors.fill: parent
                                        delegate: Text {
                                            text: delegateId.captureNameRole
                                        }


                                    }
                                }
                            }
                        }
                    }


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
                                        title: "Properies of " + layerObject.name
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
            }
        }
    }

    Dialogs.FileDialog {
        id: exportDialogId
        title: "Export to " + fileTypeExportComboBox.currentText
        // selectExisting: false
        fileMode: Dialogs.FileDialog.SaveFile
        currentFolder: RootData.lastDirectory
        defaultSuffix: screenCaptureManagerId.fileTypeToExtention(screenCaptureManagerId.fileType)
        onAccepted: {
            var type = screenCaptureManagerId.typeNameToFileType(fileTypeExportComboBox.currentText);

            RootData.lastDirectory = fileUrl
            screenCaptureManagerId.filename = fileUrl
            screenCaptureManagerId.fileType = type
            screenCaptureManagerId.capture()
        }
    }

    QQ.ListModel {
        id: paperSizeModel

        QQ.ListElement {
            name: "Letter"
            width: 8.5
            height: 11
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }

        QQ.ListElement {
            name: "Legal"
            width: 8.5
            height: 14
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }

        QQ.ListElement {
            name: "A4"
            width: 8.26772
            height: 11.6929
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }

        QQ.ListElement {
            name: "Custom Size"
            width: 8.5
            height: 11
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }
    }
}
