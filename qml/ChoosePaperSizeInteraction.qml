import QtQuick 2.0
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.0
import Cavewhere 1.0

Item {
    property var viewer;

    //    parent: viewer

    anchors.fill: parent

    onWidthChanged: paperComboBoxId.updatePaperRectangle()
    onHeightChanged: paperComboBoxId.updatePaperRectangle()

    Rectangle {
        id: leftMargin
        width: paperMarginGroupBoxId.leftMargin / paperRectangleId.paperWidth * paperRectangleId.width
        anchors.top: paperRectangleId.top
        anchors.bottom: paperRectangleId.bottom
        color: paperRectangleId.marginColor
    }

    Rectangle {
        id: rightMargin
        width: paperMarginGroupBoxId.rightMargin / paperRectangleId.paperWidth * paperRectangleId.width
        anchors.top: paperRectangleId.top
        anchors.bottom: paperRectangleId.bottom
        anchors.right: paperRectangleId.right
        color: paperRectangleId.marginColor
    }

    Rectangle {
        id: topMargin
        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        height: paperMarginGroupBoxId.topMargin / paperRectangleId.paperHeight * paperRectangleId.height
        color: paperRectangleId.marginColor
    }

    Rectangle {
        id: bottomMargin
        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        anchors.bottom: paperRectangleId.bottom
        height: paperMarginGroupBoxId.bottomMargin / paperRectangleId.paperHeight * paperRectangleId.height
        color: paperRectangleId.marginColor
    }

    Rectangle {
        id: paperRectangleId

        property real paperWidth: 0
        property real paperHeight: 0

        property bool landScape: orientationSwitchId.checked
        property color marginColor: "#BBE8E8E8"

        border.width: 5
        color: "#00000000"

        onLandScapeChanged: paperComboBoxId.updatePaperRectangle()

    }

    Rectangle {
        id: contentRectangleId

        anchors.left: leftMargin.right
        anchors.right: rightMargin.left
        anchors.top: topMargin.bottom
        anchors.bottom: bottomMargin.top

        color: "#00000000"

        border.width: 1
    }

    ColumnLayout {
        anchors.centerIn: parent

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
