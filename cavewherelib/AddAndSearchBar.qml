import QtQuick as QQ
import QtQuick.Layouts

RowLayout {
    id: controlBarId

    property alias addButtonText: addButtonId.text

    signal add();

    Button {
        id: addButtonId

        iconSource: "qrc:/icons/plus.png"

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