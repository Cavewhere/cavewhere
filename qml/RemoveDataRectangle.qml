import QtQuick 2.0

Rectangle {
    id: rect
    color: "red"
    opacity: .5

    onVisibleChanged: {
        if(visible) {
            opacityAnimation.restart()
        }
    }

    NumberAnimation {
        id: opacityAnimation
        target: rect
        property: "opacity"
        from: 0.0
        to: 0.5
    }
}
