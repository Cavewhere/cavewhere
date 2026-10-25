import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

RowLayout {
    id: controlBarId

    // Optional split-button menu: when set, a chevron appears next to
    // the add button and clicking it pops this menu. The add button
    // itself always fires add() directly.
    property QC.Menu menu: null

    property alias addButtonText: splitButtonId.text
    property alias menuToolTip: splitButtonId.menuToolTip

    property bool addEnabled: true
    //! Shown on hover over the add button in either state; empty shows none.
    property string addToolTip: ""

    signal add();

    // The wrapper carries the hover and the tooltip because a disabled button
    // reports no hover of its own.
    QQ.Item {
        implicitWidth: splitButtonId.implicitWidth
        implicitHeight: splitButtonId.implicitHeight
        Layout.alignment: Qt.AlignLeft

        QQ.HoverHandler {
            id: addHoverId
        }

        QC.ToolTip {
            objectName: "addToolTip"
            text: controlBarId.addToolTip
            delay: Theme.toolTipDelay
            visible: addHoverId.hovered && controlBarId.addToolTip !== ""
        }

        SplitButton {
            id: splitButtonId

            buttonObjectName: "addButton"
            iconSource: "qrc:/twbs-icons/icons/plus.svg"
            menu: controlBarId.menu
            enabled: controlBarId.addEnabled

            onClicked: {
                controlBarId.add();
            }
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
