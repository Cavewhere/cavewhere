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

    ChoosePaperSizeInteraction {
        id: paperSizeInteraction
        parent: view
        visible: view !== null

        onWidthChanged: paperComboBoxId.updatePaperRectangle()
        onHeightChanged: paperComboBoxId.updatePaperRectangle()
    }

    ScreenCaptureManager {
        id: screenCaptureManagerId
        view: exportViewTabId.view
        viewport: Qt.rect(paperSizeInteraction.paperRectangle.x,
                          paperSizeInteraction.paperRectangle.y,
                          paperSizeInteraction.paperRectangle.width,
                          paperSizeInteraction.paperRectangle.height)
        paperSize: Qt.size(paperSizeInteraction.paperRectangle.paperWidth,
                           paperSizeInteraction.paperRectangle.paperHeight)
        onFinishedCapture: Qt.openUrlExternally(filename)
    }

    ColumnLayout {
        id: columnLayoutId

        GroupBox {
            title: "Paper Size"

            ColumnLayout {

                ComboBox {
                    id: paperComboBoxId
                    model: paperSizeModel
                    textRole: "name"

                    property alias paperRectangle: paperSizeInteraction.paperRectangle

                    function updatePaperRectangle() {
                        var item = paperSizeModel.get(currentIndex);

                        if(item) {
                            paperRectangle.paperWidth = paperRectangle.landScape ? item.height : item.width
                            paperRectangle.paperHeight = paperRectangle.landScape ? item.width : item.height
                            paperMarginGroupBoxId.unit = item.units

                            var paperAspect = item.width / item.height;
                            if(paperRectangle.landScape) {
                                paperAspect = 1.0 / paperAspect;
                            }

                            var viewerAspect = viewer.width / viewer.height;
                            var aspect = paperAspect / viewerAspect
                            if(aspect < 1.0) {
                                paperRectangle.width = viewer.width * aspect
                                paperRectangle.height = viewer.height
                            } else {
                                var aspect = viewerAspect / paperAspect
                                paperRectangle.width = viewer.width
                                paperRectangle.height = viewer.height * aspect
                            }
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
                        updatePaperRectangle()
                        updateDefaultMargins()
                    }

                    Component.onCompleted: {
                        updatePaperRectangle()
                        updateDefaultMargins()
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
                    }
                }
            }
        }

        PaperMarginGroupBox {
            id: paperMarginGroupBoxId
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
                        paperComboBoxId.paperRectangle.landScape = checked
                        paperComboBoxId.updatePaperRectangle()
                    }
                }

                Text {
                    text: "Landscrape"
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
//                screenCaptureManagerId.capture()
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
            width: 210
            height: 297
            units: "mm"
            defaultLeftMargin: 10
            defaultRightMargin: 10
            defaultTopMargin: 10
            defaultBottomMargin: 10
        }

        ListElement {
            name: "Custom Size"
            width: 0
            height: 0
            units: "in"
            defaultLeftMargin: 1.0
            defaultRightMargin: 1.0
            defaultTopMargin: 1.0
            defaultBottomMargin: 1.0
        }

        ListElement {
            name: "Screen Size"
            width: 300 //viewer.width / 100
            height: 400 //viewer.height / 100
            units: "in"
        }
    }
}
