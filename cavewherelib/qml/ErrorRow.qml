import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// One error the way a cell's popup lists it: the kind's icon, a checkbox that
// ignores a warning, and the message, struck through once ignored. The owner
// writes the change back to its own model.
RowLayout {
    id: errorRowId

    required property int errorType
    required property string message
    required property bool suppressed

    signal ignoreToggled(bool ignored)
    signal messageTapped()

    function errorImageSource() {
        switch (errorRowId.errorType) {
        case CwError.Fatal:
            return "qrc:icons/svg/stopSignError.svg"
        case CwError.Warning:
            return "qrc:icons/svg/warning.svg"
        default:
            return ""
        }
    }

    QQ.Image {
        source: errorRowId.errorImageSource()
        sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
    }

    CheckBox {
        objectName: "suppress"

        checked: errorRowId.suppressed
        visible: errorRowId.errorType === CwError.Warning

        onToggled: errorRowId.ignoreToggled(checked)
    }

    QC.Label {
        objectName: "errorText"

        Layout.fillWidth: true

        // Error messages quote user-typed names, so they are never markup —
        // AutoText would guess otherwise for a station named A<b>B
        textFormat: QC.Label.PlainText
        text: errorRowId.message
        font.strikeout: errorRowId.suppressed
        elide: QC.Label.ElideRight

        QQ.TapHandler {
            onTapped: errorRowId.messageTapped()
        }
    }
}
