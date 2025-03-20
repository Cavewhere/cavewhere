import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as Controls

RowLayout {
    id: controlBarId

    property alias addButtonText: addButtonId.text

    signal add();

    Controls.Button {
        id: addButtonId
        objectName: "addButton"

        icon.source: "qrc:/twbs-icons/icons/plus.svg"

        Layout.alignment: Qt.AlignLeft

        onClicked: {
            controlBarId.add();
        }
    }

    QQ.Item { Layout.fillWidth: true }

    //TODO: Add me back in, and implement!
//    Controls.TextField {
//        id: searchBox

//        placeholderText: "Filter..."
//        inputMethodHints: Qt.ImhNoPredictiveText

//        Layout.alignment: Qt.AlignRight

//        implicitWidth: 250
//    }
}
