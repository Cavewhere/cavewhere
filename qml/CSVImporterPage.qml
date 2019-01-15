import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.0
import QtQuick.Dialogs 1.2
import Cavewhere 1.0

ScrollViewPage {

    CSVImporterManager {
        id: csvManagerId
        property int maxPreviewLines: 20
        previewLines: maxPreviewLines
    }

    FileDialog {
        id: openCSVFileDialogId

        title: "Import CSV file"
        selectExisting: true
        nameFilters: [ "Comma Seperated (*.csv *.txt)", "All files (*)" ]
        folder: rootData.lastDirectory

        onAccepted: {
            rootData.lastDirectory = fileUrl
            csvManagerId.filename = fileUrl
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

        Item {
            height: 5
        }

        ColumnLayout {
            //            visible: csvManagerId.filename.length

            GroupBox {

                label: RowLayout {
                    Image {
                        source: "qrc:/icons/svg/dragAndDrop.svg"
                        sourceSize: Qt.size(50, 50)
                    }

                    Text {
                        text: "<b>Drag and drop</b> columns to add and remove them"
                    }
                }

                ColumnLayout {
                    GroupBox {
                        title: "Available Columns"

                        ColumnNameView {
                            model: csvManagerId.availableColumnsModel

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

                    GroupBox {
                        title: "Used Columns"

                        ColumnNameView {
                            model: csvManagerId.columnsModel
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

                SpinBox {
                    value: csvManagerId.skipHeaderLines
                    onValueChanged: {
                        csvManagerId.skipHeaderLines = value
                    }
                }

                Text {
                    text: "Seperator"
                }

                TextField {
                    text: csvManagerId.seperator
                    onEditingFinished: {
                        csvManagerId.seperator = text
                    }
                }

                Text {
                    text: "Length Unit"
                }

                UnitInput {
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
            }

            Item {
                height: 10
            }

            GroupBox {
                title: "CSV Text"
                ResizeableScrollView {
                    implicitWidth: 600
                    implicitHeight: 150
                    TextArea {
                        id: csvTextAreaId
                        anchors.fill: parent
                        text: csvManagerId.previewText
                        font.family: "Courier"
                    }
                }
            }

            GroupBox {
                title: "Preview"

                Item {
                    implicitWidth: previewScrollViewId.implicitWidth
                    implicitHeight: previewScrollViewId.implicitHeight

                    ResizeableScrollView {
                        id: previewScrollViewId
                        implicitWidth: 600
                        implicitHeight: 150
                        TableView {
                            model: csvManagerId.lineModel
                            anchors.fill: parent
                            delegate: Rectangle {
                                border.width: 1
                                border.color: "lightgrey"
                                implicitWidth: textId.width + 6
                                implicitHeight: textId.height + 6

                                Text {
                                    id: textId
                                    anchors.centerIn: parent
                                    text: displayRole == undefined ? "" : displayRole
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
                            State {
                                when: csvManagerId.previewLines > csvManagerId.lineCount
                                PropertyChanges {
                                    target: showAllButtonId
                                    text: "Less"
                                    visible: true
                                    onClicked: {
                                        csvManagerId.previewLines = csvManagerId.maxPreviewLines
                                    }
                                }

                            },
                            State {
                                when: csvManagerId.previewLines < csvManagerId.lineCount
                                PropertyChanges {
                                    target: showAllButtonId
                                    text: "More"
                                    visible: true
                                    onClicked: {
                                        csvManagerId.previewLines = Number.MAX_VALUE
                                    }
                                }
                            },
                            State {
                                when: csvManagerId.previewLines == csvManagerId.lineCount
                                PropertyChanges {
                                    target: showAllButtonId
                                    visible: false
                                }
                            }
                        ]
                    }
                }
            }

            Button {
                text: "Import"
                onClicked: {

                }
            }
        }
    }
}
