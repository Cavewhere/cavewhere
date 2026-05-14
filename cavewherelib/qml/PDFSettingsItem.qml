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

                QC.Label {
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
                text: "The rasterization resolution for all PDF / SVG in pixels per inch. By default the import resolution is 96ppi, a good balance of size vs quality. Using 72ppi (the lowest resolution for pdf) and 92ppi for svg will import 1 to 1. PDF / SVG rasterization is supported up to 600 ppi or 256mb, but high resolutions require a large amount of memory."
            }
        }
    }

    RestoreDefaultsButton {
        settings: itemId.pdfSettings
    }
}
