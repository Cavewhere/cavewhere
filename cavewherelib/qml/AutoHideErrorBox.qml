import QtQuick as QQ
import cavewherelib

ErrorHelpBox {
    id: root

    property int hideDelay: 4000

    visible: false

    function show() {
        visible = true
        hideTimer.restart()
    }

    QQ.Timer {
        id: hideTimer
        interval: root.hideDelay
        repeat: false
        onTriggered: root.visible = false
    }
}
