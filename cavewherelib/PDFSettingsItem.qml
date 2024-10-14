import QtQuick 2.0
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12
import cavewherelib

ColumnLayout {
    id: itemId
    property PDFSettings pdfSettings: RootData.settings.pdfSettings

    QC.GroupBox {
        title: "PDF Import Settings"

        ColumnLayout {
            SupportedLabel {
                supported: itemId.pdfSettings.isSupportImport
                text: "Importing PDFs"
            }

            RowLayout {

                InformationButton {
                    showItemOnClick: resolutionHelpId
                }

                Text {
                    text: "PDF import resolution (in ppi)"
                }

                QC.SpinBox {
                    enabled: itemId.pdfSettings.isSupportImport
                    from: 72
                    to: 600
                    value: itemId.pdfSettings.resolutionImport
                    editable: true
                    onValueChanged: {
                        itemId.pdfSettings.resolutionImport = value;
                    }
                }
            }

            HelpArea {
                id: resolutionHelpId
                Layout.fillWidth: true
                text: "The import resolution for all future PDF imports in ppi. By default the import resolution is 300ppi. 300ppi is good balance to size vs quality. Using 72ppi (the lowest resolution), will import 1 to 1. PDF imports support upto 600 ppi, but will cause large file sizes and memory consumption. "
            }
        }
    }
}
