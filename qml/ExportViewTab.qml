import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.1

Item {

    implicitHeight: columnLayoutId.height
    implicitWidth: columnLayoutId.width

    ColumnLayout {
        id: columnLayoutId

        GroupBox {
            title: "Paper Size"

            ColumnLayout {

                ComboBox {
                    id: paperComboBoxId
                    model: paperSizeModel
                    textRole: "name"

                    function updatePaperRectangle() {
                        var item = paperSizeModel.get(currentIndex);

                        if(item) {
                            paperRectangleId.paperWidth = item.width
                            paperRectangleId.paperHeight = item.height
                            paperMarginGroupBoxId.unit = item.units

                            var paperAspect = item.width / item.height;
                            if(paperRectangleId.landScape) {
                                paperAspect = 1.0 / paperAspect;
                            }

                            var viewerAspect = viewer.width / viewer.height;
                            var aspect = paperAspect / viewerAspect
                            if(aspect < 1.0) {
                                paperRectangleId.width = viewer.width * aspect
                                paperRectangleId.height = viewer.height
                            } else {
                                var aspect = viewerAspect / paperAspect
                                paperRectangleId.width = viewer.width
                                paperRectangleId.height = viewer.height * aspect
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
                        value: 300
                        stepSize: 100
                        maximumValue: 600
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
                }

                Text {
                    text: "Landscrape"
                }
            }
        }

        GroupBox {
            title: "File type"

            ComboBox {
                model: ["svg"]
            }
        }

        Button {
            Layout.alignment: Qt.AlignRight
            text: "Export"
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

        //        ListElement {
        //            name: "Legal"
        //            width: 8.5
        //            height: 14
        //            units: "in"
        //        }

        //        ListElement {
        //            name: "A4"
        //            width: 210
        //            height: 297
        //            units: "mm"
        //        }

        //        ListElement {
        //            name: "Custom Size"
        //            width: 0
        //            height: 0
        //            units: "in"
        //        }

        //        ListElement {
        //            name: "Screen Size"
        //            width: 300 //viewer.width / 100
        //            height: 400 //viewer.height / 100
        //            units: "in"
        //        }
    }
}
