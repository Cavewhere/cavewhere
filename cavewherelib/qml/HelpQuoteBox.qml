import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
QuoteBox {
    property alias text: textId.text

    color: Theme.warning
    triangleOffset: 0.0
    Text {
        id: textId
        font.pixelSize: 24
        text: "NO TEXT"
    }
}
