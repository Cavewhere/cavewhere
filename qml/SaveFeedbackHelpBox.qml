import QtQuick 2.0 as QQ
import QtQml 2.12
import Cavewhere 1.0

HelpBox {
    id: savedHelpBoxId
    anchors.centerIn: parent
    text: "Save successful"
    visible: false;
    animateHide: true

    color: "#a0faa0"

    Timer {
        id: hideTimerId
        interval: 1500
        onTriggered: {
            savedHelpBoxId.visible = false;
        }
    }

    QQ.Connections {
        target: rootData.project
        onFileSaved: {
            savedHelpBoxId.visible = true;
            hideTimerId.start()
        }
    }
}
