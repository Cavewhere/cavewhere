import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2 as Controls

RowLayout {

//    Layout.fillWidth: true

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

    Controls.TextField {
        id: searchBox

        placeholderText: "Search..."
        inputMethodHints: Qt.ImhNoPredictiveText

        Layout.alignment: Qt.AlignRight

        implicitWidth: 250
    }
}
