import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
QuoteBox {
    id: helpQuoteBoxId

    property alias text: textId.text

    // Without a cap the label lays out on one line, so a long hint runs
    // past the window edge instead of growing the box downwards.
    property real maximumTextWidth: Theme.helpBoxMaxWidth

    color: Theme.warning
    triangleOffset: 0.0
    QC.Label {
        id: textId
        objectName: "helpText"
        width: Math.min(implicitWidth,
                        helpQuoteBoxId.maximumTextWidth,
                        helpQuoteBoxId.maximumContentWidth)
        font.pixelSize: Theme.fontSizeBody
        wrapMode: QC.Label.WordWrap
        // text is public API here, so a caller interpolating a cave or
        // file name must not be able to smuggle in markup.
        textFormat: QC.Label.PlainText
        text: "NO TEXT"
    }
}
