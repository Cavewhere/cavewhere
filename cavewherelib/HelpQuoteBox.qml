import QtQuick
import cavewherelib

QuoteBox {
    property alias text: textId.text

    color: "#ffa23f"
    triangleOffset: 0.0
    Text {
        id: textId
        font.pixelSize: 24
        text: "NO TEXT"
    }
}
