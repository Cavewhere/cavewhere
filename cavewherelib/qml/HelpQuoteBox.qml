import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
QuoteBox {
    property alias text: textId.text

    color: Theme.warning
    triangleOffset: 0.0
    QC.Label {
        id: textId
        font.pixelSize: Theme.fontSizeLarge
        text: "NO TEXT"
    }
}
