import QtQuick 2.0
import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12
import Cavewhere 1.0

ColumnLayout {
    property PDFSettings pdfSettings: rootData.settings.pdfSettings

    QC.GroupBox {
        title: "PDF Import Settings"

        ColumnLayout {
            SupportedLabel {
                supported: pdfSetting.isSupportImport
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
                    enabled: pdfSetting.isSupportImport
                    from: 72
                    to: 600
                    value: pdfSettings.resolutionImport
                    editable: true
                    onValueChanged: {
                        pdfSettings.resolutionImport = value;
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
