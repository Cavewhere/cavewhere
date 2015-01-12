import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2 as Controls

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

    Item { Layout.fillWidth: true }

    //TODO: Add me back in, and implement!
//    Controls.TextField {
//        id: searchBox

//        placeholderText: "Filter..."
//        inputMethodHints: Qt.ImhNoPredictiveText

//        Layout.alignment: Qt.AlignRight

//        implicitWidth: 250
//    }
}
