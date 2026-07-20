import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC

RowLayout {
    id: controlBarId

    // Optional split-button menu: when set, a chevron appears next to
    // the add button and clicking it pops this menu. The add button
    // itself always fires add() directly.
    property QC.Menu menu: null

    property alias addButtonText: splitButtonId.text
    property alias menuToolTip: splitButtonId.menuToolTip

    signal add();

    SplitButton {
        id: splitButtonId

        buttonObjectName: "addButton"
        iconSource: "qrc:/twbs-icons/icons/plus.svg"
        menu: controlBarId.menu

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
