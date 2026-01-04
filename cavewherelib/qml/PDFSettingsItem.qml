import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: itemId
    property PDFSettings pdfSettings: RootData.settings.pdfSettings

    QC.GroupBox {
        title: "PDF / SVG Import Settings"

        ColumnLayout {
            SupportedLabel {
                supported: itemId.pdfSettings.isSupportImport
                text: "PDFs"
            }

            RowLayout {

                InformationButton {
                    showItemOnClick: resolutionHelpId
                }

                Text {
                    text: "PDF / SVG rasterization resolution (in pixels per inch)"
                }

                QC.SpinBox {
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
                text: "The rasterization resolution for all PDF / SVG in pixels per inch. By default the import resolution is 300ppi. 300ppi is good balance to size vs quality. Using 72ppi (the lowest resolution for pdf) and 92ppi for svg, will import 1 to 1. PDF / SVG rasterization supported upto 600 ppi or 256mb, but will need a large amount of memory."
            }
        }
    }
}
