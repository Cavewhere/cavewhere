import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC

RowLayout {
    id: controlBarId

    property alias addButtonText: addButtonId.text

    signal add();

    QC.Button {
        id: addButtonId
        objectName: "addButton"

        icon.source: "qrc:/twbs-icons/icons/plus.svg"
        icon.width: Theme.iconSizeButton
        icon.height: Theme.iconSizeButton

        Layout.alignment: Qt.AlignLeft

        onClicked: {
            controlBarId.add();
        }
    }

    QQ.Item { Layout.fillWidth: true }

    //TODO: Add me back in, and implement!
//    QC.TextField {
//        id: searchBox

//        placeholderText: "Filter..."
//        inputMethodHints: Qt.ImhNoPredictiveText

//        Layout.alignment: Qt.AlignRight

//        implicitWidth: 250
//    }
}
