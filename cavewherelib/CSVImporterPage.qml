pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQuick.Dialogs as QD
import cavewherelib

ScrollViewPage {
    id: pageId
    property int defaultMinWidth: 600

    CSVImporterManager {
        id: csvManagerId
        property int maxPreviewLines: 20
        previewLines: maxPreviewLines
    }

    QD.FileDialog {
        id: openCSVFileDialogId

        title: "Import CSV file"
        nameFilters: [ "Comma Seperated (*.csv *.txt)", "All files (*)" ]
        currentFolder: RootData.lastDirectory

        onAccepted: {
            RootData.lastDirectory = selectedFile
            csvManagerId.filename = selectedFile
        }
    }

    ColumnLayout {

        RowLayout {
            Button {
                text: "Open"
                onClicked: openCSVFileDialogId.open()
            }

            Text {
                id: filenameId
                text: csvManagerId.filename.length == 0 ? "No file loaded" : csvManagerId.filename
            }
        }

        BreakLine {
            visible: csvManagerId.filename.length
        }

        ColumnLayout {
            visible: csvManagerId.filename.length

            QC.GroupBox {

                label: RowLayout {
                    QQ.Image {
                        source: "qrc:/icons/svg/dragAndDrop.svg"
                        sourceSize: Qt.size(50, 50)
                    }

                    Text {
                        text: "<b>Drag and drop</b> columns to add and remove them"
                    }
                }

                ColumnLayout {
                    QC.GroupBox {
                        title: "Available Columns"

                        ColumnNameView {
                            id: availableColumnViewId
                            model: csvManagerId.availableColumnsModel
                            Layout.minimumWidth: pageId.defaultMinWidth

                            //Custom drop function for model manipulation that prevents the skip column
                            //from being removed
                            moveFunction: function(model, oldModel, index, indexOffset, oldIndex) {
                                var value = oldModel.get(oldIndex);

                                //Remove columns that aren't skip column
                                oldModel.remove(oldIndex);

                                if(value.columnId !== csvManagerId.skipColumnId || oldModel === model) {
                                    index = Math.max(0, Math.min(index + indexOffset, model.count))
                                    model.insert(index, value);
                                }
                            }
                        }
                    }

                    QC.GroupBox {
                        title: "Used Columns"

                        ColumnNameView {
                            model: csvManagerId.columnsModel
                            Layout.minimumWidth: availableColumnViewId.Layout.minimumWidth
                            moveFunction: function(model, oldModel, index, indexOffset, oldIndex) {
                                var value = oldModel.get(oldIndex);
                                if(value.columnId !== csvManagerId.skipColumnId || oldModel === model) {
                                    //Only add columns that aren't skip columns since multiple columns can be skipped
                                    oldModel.remove(oldIndex);
                                }
                                index = Math.max(0, Math.min(index + indexOffset, model.count))
                                model.insert(index, value);
                            }
                        }
                    }
                }
            }

            GridLayout {
                columns: 2
                Text {
                    text: "Skip header lines"
                }

                QC.SpinBox {
                    value: csvManagerId.skipHeaderLines
                    onValueChanged: {
                        csvManagerId.skipHeaderLines = value
                    }
                }

                Text {
                    text: "Seperator"
                }

                QC.TextField {
                    text: csvManagerId.seperator
                    onEditingFinished: {
                        csvManagerId.seperator = text
                    }
                }

                Text {
                    text: "Length Unit"
                }

                UnitCombo {
                    id: lengthUnits

                    Length {
                        id: lengthId
                    }

                    unitModel: lengthId.unitNames
                    unit: csvManagerId.distanceUnit
                    onNewUnit: {
                        csvManagerId.distanceUnit = unit
                    }
                }

                Text {
                    text: "Empty lines create new Trips"
                }

                CheckBox {
                    id: emptyLineCheckBoxId
                    checked: csvManagerId.newTripOnEmptyLines
                    onCheckedChanged: {
                        csvManagerId.newTripOnEmptyLines = checked
                    }
                }

                Text {
                    text: "Associate LRUDs with"
                }

                QC.ComboBox {
                    implicitWidth: 200
                    model: ["From station", "To station"]
                    currentIndex: csvManagerId.useFromStationForLRUD ? 0 : 1
                    onCurrentIndexChanged: {
                        if(currentIndex == 0) {
                            csvManagerId.useFromStationForLRUD = true
                        } else {
                            csvManagerId.useFromStationForLRUD = false
                        }
                    }
                }
            }

            QQ.Item {
                implicitHeight: 10
            }

            QC.GroupBox {
                title: "CSV Text"
                ResizeableScrollView {
                    implicitWidth: pageId.defaultMinWidth
                    implicitHeight: 150

                    RowLayout {
                        spacing: 0

                        QQ.ListView {
                            implicitWidth: 75
                            implicitHeight: csvTextAreaId.implicitHeight
                            model: csvTextAreaId.lineCount
                            delegate: Text {
                                property int index;

                                anchors.horizontalCenter: parent.horizontalCenter
                                font.family: csvTextAreaId.font.family
                                font.pixelSize: csvTextAreaId.font.pixelSize
                                text: index + 1
                                color: "grey"
                            }

                            QQ.Rectangle {
                                anchors.fill: parent
                                color: "#e8e8e8"
                                z: -1
                            }
                        }

                        QC.TextArea {
                            id: csvTextAreaId
                            text: csvManagerId.previewText
                            font.family: "Courier"
                            leftPadding: 0
                            topPadding: 0
                        }
                    }
                }
            }

            QC.GroupBox {
                title: "Preview"

                QQ.Item {
                    implicitWidth: previewScrollViewId.width
                    implicitHeight: previewScrollViewId.height

                    ResizeableScrollView {
                        id: previewScrollViewId
                        implicitWidth: pageId.defaultMinWidth
                        implicitHeight: 150
                        QQ.TableView {
                            model: csvManagerId.lineModel
                            anchors.fill: parent
                            delegate: QQ.Rectangle {
                                id: delegateId
                                required property string displayRole

                                border.width: 1
                                border.color: "lightgrey"
                                implicitWidth: textId.width + 6
                                implicitHeight: textId.height + 6

                                Text {
                                    id: textId
                                    anchors.centerIn: parent
                                    text: delegateId.displayRole == undefined ? "" : delegateId.displayRole
                                }
                            }
                        }
                    }

                    Button {
                        id: showAllButtonId
                        anchors.right: previewScrollViewId.right
                        anchors.rightMargin: previewScrollViewId.resizeHandleSize.width + 5
                        anchors.bottom: previewScrollViewId.bottom

                        states: [
                            QQ.State {
                                when: csvManagerId.previewLines == CSVImporterManager.AllLines
                                QQ.PropertyChanges {
                                    showAllButtonId {
                                        text: "Less"
                                        visible: true
                                        onClicked: {
                                            csvManagerId.previewLines = csvManagerId.maxPreviewLines
                                        }
                                    }
                                }

                            },
                            QQ.State {
                                when: csvManagerId.previewLines !== CSVImporterManager.AllLines
                                QQ.PropertyChanges {
                                    showAllButtonId {
                                        text: "More"
                                        visible: true
                                        onClicked: {
                                            csvManagerId.previewLines = CSVImporterManager.AllLines
                                        }
                                    }
                                }
                            }
                        ]
                    }
                }
            }

            QC.GroupBox {
                title: "Status"
                ColumnLayout {
                    RowLayout {
                        visible: csvManagerId.errorModel.errors.count == 0
                        QQ.Image {
                            source: "qrc:icons/good.png"
                        }
                        Text {
                            text: "Success"
                        }
                    }

                    ColumnLayout {
                        visible: csvManagerId.errorModel.errors.count > 0
                        RowLayout {
                            QQ.Image {
                                source: "qrc:icons/svg/stopSignError.svg"
                            }
                            Text {
                                text: csvManagerId.errorModel.fatalCount + " errors"
                            }

                            QQ.Item { implicitWidth: 1} //spacer

                            QQ.Image {
                                source: "qrc:icons/svg/warning.svg"
                            }
                            Text {
                                text: csvManagerId.errorModel.warningCount + " warnings"
                            }
                        }

                        ResizeableScrollView {
                            implicitWidth: pageId.defaultMinWidth
                            implicitHeight: 150
                            ErrorListView {
                                model: csvManagerId.errorModel.errors
                            }
                        }
                    }
                }
            }

            BreakLine {}

            QC.Button {
                text: "Import"
                enabled: csvManagerId.errorModel.fatalCount == 0
                onClicked: {
                    //Add the caves
                    RootData.region.addCaves(csvManagerId.caves);

                    //Go back to main page
                    RootData.pageSelectionModel.back()
                }

                font.bold: true
            }
        }
    }
}
