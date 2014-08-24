import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.1
import Cavewhere 1.0
import QtQuick.Dialogs 1.0 as Dialogs

Item {
    id: exportViewTabId

    implicitHeight: columnLayoutId.height
    implicitWidth: columnLayoutId.width

    property GLTerrainRenderer view

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

    SelectExportAreaTool {
        parent: exportViewTabId.view
        view: exportViewTabId.view
        manager: screenCaptureManagerId
    }

    ChoosePaperSizeInteraction {
        id: paperSizeInteraction
        parent: view
        visible: false; //view !== null

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

    RowLayout {

        ColumnLayout {
            id: columnLayoutId

            GroupBox {
                title: "Paper Size"

                ColumnLayout {

                    ComboBox {
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

                        Component.onCompleted: {
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
                            onFinishedEditting: {
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
                            onFinishedEditting: {
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

                        SpinBox {
                            id: resolutionSpinBoxId
                            value: screenCaptureManagerId.resolution
                            stepSize: 100
                            maximumValue: 600

                            onValueChanged: {
                                screenCaptureManagerId.resolution = value
                            }

                            Connections {
                                target: screenCaptureManagerId
                                onResolutionChanged: resolutionSpinBoxId.value = screenCaptureManagerId.resolution
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
            }

            GroupBox {
                title: "Orientation"

                RowLayout {
                    id: portraitLandscrapSwitch
                    Text {
                        text: "Portrait"
                    }

                    Switch {
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

            GroupBox {
                id: projectionScaleGroupBoxId
                title: "Scale"

                Scale {
                    id: projectionScaleId

                    property double oldScale: 0.0
                    property bool scaleChangeEnabled: false

                    //On paper
                    scaleNumerator.value: 1
                    scaleNumerator.unit: Units.LengthUnitless
                    scaleDenominator.unit: Units.LengthUnitless

                    onScaleChanged: {
                        //Update the ortho projection matrix
                        console.log("1. oldScale:" + oldScale + " " + scale)

                        if(!isFinite(scale)) {
                            return;
                        }

                        if(oldScale == 0.0) {
                            oldScale = scale;
                            return;
                        }

                        if(scaleChangeEnabled) {

                            console.log("2. oldScale:" + oldScale + " " + scale + "\n")

                            var scaleFactor = oldScale / scale

                            var newZoomScale = view.camera.zoomScale * scaleFactor;
                            console.log("scalefactor:" + scaleFactor + "newZomeScale:" + newZoomScale)
                            view.camera.zoomScale = newZoomScale

                            constantScaleCheckboxId.checked = true
                        }

                        oldScale = scale
                    }

                }

                function updateScaleDenominator() {
                    var inchToMeter = 0.0254;
                    if(constantScaleCheckboxId.checked) {
                        //Update the paper size such that the scale stays the same
                        var paperSize = (screenCaptureManagerId.screenPaperSize.width * projectionScaleId.scale) /
                                (inchToMeter * view.camera.pixelsPerMeter);

                        var paperAspect = screenCaptureManagerId.paperSize.height / screenCaptureManagerId.paperSize.width;
                        var paperHeight = paperSize * paperAspect;
                        var paperWidth = paperSize;

                        paperSizeInteraction.paperRectangle.paperWidth = paperWidth
                        paperSizeInteraction.paperRectangle.paperHeight = paperHeight

                        //                    screenCaptureManagerId.paperSize = Qt.size(paperWidth, paperHeight)
                        paperSizeModel.set(3, {"width":paperWidth})
                        paperSizeModel.set(3, {"height":paperHeight})

                        console.log("Paper size:" + paperSize)

                    } else {
                        //Update the scale
                        var logicalDotPerMeter = screenCaptureManagerId.screenPaperSize.width /
                                (screenCaptureManagerId.paperSize.width * inchToMeter)
                        projectionScaleId.scaleChangeEnabled = false;
                        projectionScaleId.scale = view.camera.pixelsPerMeter / logicalDotPerMeter;
                        projectionScaleId.scaleChangeEnabled = true;
                    }
                }

                Connections {
                    target: screenCaptureManagerId
                    onScreenPaperSizeChanged: projectionScaleGroupBoxId.updateScaleDenominator()
                    onPaperSizeChanged: projectionScaleGroupBoxId.updateScaleDenominator()
                }

                Connections {
                    target: view.camera
                    onPixelsPerMeterChanged: projectionScaleGroupBoxId.updateScaleDenominator()
                }

                Column {
                    PaperScaleInput {
                        usingInteraction: false
                        scaleObject: projectionScaleId
                    }

                    CheckBox {
                        id: constantScaleCheckboxId
                        text: "Constant Scale"
                        checked: false
                        onCheckedChanged: {
                            paperComboBoxId.currentIndex = 3
                        }
                    }
                }

            }

            GroupBox {
                title: "File type"

                ComboBox {
                    id: fileTypeExportComboBox
                    model: ["PDF", "SVG", "PNG"]
                }
            }

            Button {
                Layout.alignment: Qt.AlignRight
                text: "Export"

                onClicked: {
                    exportDialogId.open();
                    //                screenCaptureManagerId.filename = "file://Users/vpicaver/Documents/Projects/cavewhere/testcase/loopCloser/test.png"
                    //                screenCaptureManagerId.capture()
                }
            }
        }
    }

    Dialogs.FileDialog {
        id: exportDialogId
        title: "Export to " + fileTypeExportComboBox.currentText
        selectExisting: false
        onAccepted: {
            screenCaptureManagerId.filename = fileUrl
            screenCaptureManagerId.capture();
        }
    }

    ListModel {
        id: paperSizeModel

        ListElement {
            name: "Letter"
            width: 8.5
            height: 11
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }

        ListElement {
            name: "Legal"
            width: 8.5
            height: 14
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }

        ListElement {
            name: "A4"
            width: 8.26772
            height: 11.6929
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }

        ListElement {
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
