import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1

RowLayout {

    property alias addButtonText: addButtonId.text

    signal add();

    Button {
        id: addButtonId

        iconSource: "qrc:/icons/plus.png"

        Layout.alignment: Qt.AlignLeft

        onClicked: {
            add();
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
