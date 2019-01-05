import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.0
import Cavewhere 1.0

Item {

    anchors.fill: parent

    ColumnLayout {

        RowLayout {
            Text {
                id: filenameId
                text: "No file loaded"
            }

            Button {
                text: "Open"
            }
        }

        GroupBox {
            title: "Available Columns"

        }

        GroupBox {
            title: "Used Columns"
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
