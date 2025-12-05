import QtQuick as QQ
import QtQml
import cavewherelib

HelpBox {
    id: savedHelpBoxId
    anchors.centerIn: parent
    text: "Save successful"
    visible: false;
    animateHide: true

    color: Theme.success

    Timer {
        id: hideTimerId
        interval: 1500
        onTriggered: {
            savedHelpBoxId.visible = false;
        }
    }

    QQ.Connections {
        target: RootData.project
        function onFileSaved() {
            savedHelpBoxId.visible = true;
            hideTimerId.start()
        }
    }
}
