pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import QtQuick.Dialogs as Dialogs
import cavewherelib

ColumnLayout {
    id: mapOptionsId
    objectName: "mapOptions"

    required property GLTerrainRenderer view
    required property CaptureManager screenCaptureManager
    required property ChoosePaperSizeInteraction paperSizeInteraction
    property alias paperMarginGroupBox: paperMarginGroupBoxId
    property alias paperComboBox: paperComboBoxId

    function updatePaperRectangle(paperWidth, paperHeight) {
        let i = paperSizeInteraction

        let swapDim = i.paperRectangle.landScape
            && !paperSizeModel.get(paperComboBoxId.currentIndex).sizeEdittable

        i.paperRectangle.paperWidth = swapDim ? paperHeight : paperWidth
        i.paperRectangle.paperHeight = swapDim ? paperWidth : paperHeight

        //Stretch the paper to the max width or height of the screen
        let paperAspect = paperWidth / paperHeight;
        if(i.paperRectangle.landScape) {
            paperAspect = 1.0 / paperAspect;
        }

        let viewerAspect = view.width / view.height;
        let aspect = paperAspect / viewerAspect
        if(aspect < 1.0) {
            i.paperRectangle.width = view.width * aspect
            i.paperRectangle.height = view.height
        } else {
            let aspect = viewerAspect / paperAspect
            i.paperRectangle.width = view.width
            i.paperRectangle.height = view.height * aspect
        }
    }

    Dialogs.FileDialog {
        id: exportDialogId
        title: "Export to " + fileTypeExportComboBox.currentText
        // selectExisting: false
        fileMode: Dialogs.FileDialog.SaveFile
        currentFolder: RootData.lastDirectory
        defaultSuffix: mapOptionsId.screenCaptureManager.fileTypeToExtention(mapOptionsId.screenCaptureManager.fileType)
        onAccepted: {
            var type = mapOptionsId.screenCaptureManager.typeNameToFileType(fileTypeExportComboBox.currentText);

            RootData.lastDirectory = selectedFile
            mapOptionsId.screenCaptureManager.filename = selectedFile
            mapOptionsId.screenCaptureManager.fileType = type
            mapOptionsId.screenCaptureManager.capture()
        }
    }

    QQ.ListModel {
        id: paperSizeModel

        QQ.ListElement {
            name: "Letter"
            width: "8.5"
            height: "11"
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
            sizeEdittable: false
        }

        QQ.ListElement {
            name: "Legal"
            width: "8.5"
            height: "14"
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
            sizeEdittable: false
        }

        QQ.ListElement {
            name: "A4"
            width: "8.26772"
            height: "11.6929"
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
            sizeEdittable: false
        }

        QQ.ListElement {
            name: "Custom Size"
            width: "8.5"
            height: "11"
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
            sizeEdittable: true
        }
    }



    RowLayout {
        Layout.fillWidth: true

        QC.GroupBox {
            title: "File type"

            QC.ComboBox {
                id: fileTypeExportComboBox
                model: mapOptionsId.screenCaptureManager.fileTypes
            }
        }

        QC.Button {
            text: "Export"
            enabled: mapOptionsId.screenCaptureManager.memoryLimit > mapOptionsId.screenCaptureManager.memoryRequired || mapOptionsId.screenCaptureManager.memoryLimit < 0.0
            onClicked: {
                exportDialogId.open();
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

                text: "Memory Required: " + formatMemory(mapOptionsId.screenCaptureManager.memoryRequired)
            }

            InformationButton {
                objectName: "memoryHelpArea"
                showItemOnClick: memoryHelpAreaId
            }
        }

        HelpArea {
            id: memoryHelpAreaId
            Layout.fillWidth: true;
            text: "The amout of RAM that CaveWhere requires to save image. Using more memory than what's on computer my cause your computer to hang! CaveWhere may temporarily use equal or double the amount of disk space required by the memory required";
        }

        Text {
            visible: mapOptionsId.screenCaptureManager.memoryLimit > 0.0
            text: "Memory Limit: " + memoryRequiredId.formatMemory(mapOptionsId.screenCaptureManager.memoryLimit);
        }

    }

    BreakLine {  }

    QC.Button {
        objectName: "opitonsButton"
        text: "Options"
        onClicked: {
            optionsScrollViewId.visible = !optionsScrollViewId.visible
        }

    }

    QQ.Item {
        implicitWidth: 1
        visible: !optionsScrollViewId.visible
        Layout.fillHeight: true
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
                        objectName: "paperComboBox"
                        model: paperSizeModel
                        textRole: "name"

                        function updatePaperRectangleFromModel() {
                            let item = paperSizeModel.get(currentIndex);

                            if(item) {
                                mapOptionsId.updatePaperRectangle(item.width, item.height)
                            }
                        }

                        function updateDefaultMargins() {
                            let item = paperSizeModel.get(currentIndex);
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
                            objectName: "paperWidthInput"
                            text: mapOptionsId.screenCaptureManager.paperSize.width
                            readOnly: !paperSizeModel.get(paperComboBoxId.currentIndex).sizeEdittable
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
                            objectName: "paperHeightInput"
                            text: mapOptionsId.screenCaptureManager.paperSize.height
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
                            value: mapOptionsId.screenCaptureManager.resolution
                            stepSize: 100
                            from: 100
                            to: 600

                            onValueChanged: {
                                mapOptionsId.screenCaptureManager.resolution = value
                            }

                            QQ.Connections {
                                target: mapOptionsId.screenCaptureManager
                                function onResolutionChanged() {
                                    resolutionSpinBoxId.value = mapOptionsId.screenCaptureManager.resolution
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

                onLeftMarginChanged: {
                    mapOptionsId.screenCaptureManager.leftMargin = leftMargin
                }
                onRightMarginChanged: mapOptionsId.screenCaptureManager.rightMargin = rightMargin
                onTopMarginChanged: mapOptionsId.screenCaptureManager.topMargin = topMargin
                onBottomMarginChanged: mapOptionsId.screenCaptureManager.bottomMargin = bottomMargin
                unit: "in"
            }

            QC.GroupBox {
                id: orientationId
                title: "Orientation"
                visible: !paperSizeModel.get(paperComboBoxId.currentIndex).sizeEdittable

                RowLayout {
                    id: portraitLandscrapSwitch
                    Text {
                        text: "Portrait"
                    }

                    QC.Switch {
                        id: orientationSwitchId
                        objectName: "orientaitonSwitch"
                        onCheckedChanged: {
                            mapOptionsId.paperSizeInteraction.paperRectangle.landScape = checked
                            paperComboBoxId.updatePaperRectangleFromModel()
                        }
                    }

                    Text {
                        text: "Landscape"
                    }
                }
            }
        }
    }
}
