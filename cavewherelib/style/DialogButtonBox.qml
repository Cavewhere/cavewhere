import QtQuick as QQ
import QtQuick.Templates as T
// Qualified: cavewherelib has a legacy Button of its own, and an explicit
// import outranks this module's own files, so a bare import would hand the
// delegate that Rectangle instead of the style's Button.
import cavewherelib as CW

T.DialogButtonBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    contentWidth: (contentItem as QQ.ListView)?.contentWidth

    spacing: CW.Theme.dialogButtonSpacing
    padding: CW.Theme.dialogPadding
    alignment: Qt.AlignRight

    delegate: Button { }

    contentItem: QQ.ListView {
        implicitWidth: contentWidth
        model: control.contentModel
        spacing: control.spacing
        orientation: QQ.ListView.Horizontal
        boundsBehavior: QQ.Flickable.StopAtBounds
        snapMode: QQ.ListView.SnapToItem
    }

    background: QQ.Item {
        implicitHeight: CW.Theme.controlHeight
    }
}
