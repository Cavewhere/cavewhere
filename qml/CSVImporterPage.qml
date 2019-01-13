import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.0
import QtQuick.Dialogs 1.2
import Cavewhere 1.0

Item {
    anchors.fill: parent

    CSVImporterManager {
        id: csvManagerId

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
            Text {
                id: filenameId
                text: csvManagerId.filename.length == 0 ? "No file loaded" : csvManagerId.filename
            }

            Button {
                text: "Open"
                onClicked: openCSVFileDialogId.open()
            }
        }

        GroupBox {
            title: "Available Columns"

            ColumnNameView {
                model: csvManagerId.availableColumnsModel
            }
        }

        GroupBox {
            title: "Used Columns"

            ColumnNameView {
                model: csvManagerId.columnsModel
            }
        }

        RowLayout {
            RowLayout {
                Text {
                    text: "Skip header lines"
                }

                SpinBox {

                }

                }

            RowLayout {
                Text {
                    text: "Seperator"
                }

                TextField {

                }
            }


        }

        RowLayout {
            TapeCalibrationEditor {
                width: 300
            }

            CheckBox {
                text: "New trip on empty line"
            }
        }

        RowLayout {
            TextEdit {

            }

            Rectangle {
                width: 100
                height: 100
                color: "red"

                Text {
                    text:"Preview"
                }
            }
        }

        Button {
            Text {
                text: "Import"
            }
        }
    }
}
