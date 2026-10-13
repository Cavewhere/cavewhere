import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.TabBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    spacing: 0

    contentItem: QQ.ListView {
        model: control.contentModel
        currentIndex: control.currentIndex

        spacing: control.spacing
        orientation: QQ.ListView.Horizontal
        boundsBehavior: QQ.Flickable.StopAtBounds
        flickableDirection: QQ.Flickable.AutoFlickIfNeeded
        snapMode: QQ.ListView.SnapToItem

        highlightMoveDuration: 0
        highlightRangeMode: QQ.ListView.ApplyRange
        preferredHighlightBegin: Theme.controlHeight
        preferredHighlightEnd: width - Theme.controlHeight
    }

    // The checked tab fills down over the bottom line, so it reads as joined
    // to the page below.
    background: QQ.Rectangle {
        color: Theme.tabStrip

        QQ.Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
        }
    }
}
