import QtQuick 2.0
import QtQuick.Controls 1.2 as Controls
import QtQuick.Layouts 1.1

RowLayout {

    Button {
        iconSource: "qrc:/icons/leftCircleArrow-32x32.png"
        enabled: rootData.pageSelectionModel.hasBackward
        onClicked: {
            rootData.pageSelectionModel.back();
        }
    }

    Button {
        iconSource: "qrc:/icons/rightCircleArrow-32x32.png"
        enabled: rootData.pageSelectionModel.hasForward
        onClicked: {
            rootData.pageSelectionModel.forward();
        }
    }

    Controls.TextField {
        Layout.fillWidth: true

        text: rootData.pageSelectionModel.currentPageAddress
        onEditingFinished: rootData.pageSelectionModel.currentPageAddress = text
    }
}

