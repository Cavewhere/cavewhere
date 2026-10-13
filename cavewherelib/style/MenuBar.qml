import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

// Used on macOS only, where Qt may present the bar natively.
T.MenuBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    delegate: MenuBarItem { }

    contentItem: QQ.Row {
        spacing: control.spacing

        QQ.Repeater {
            model: control.contentModel
        }
    }

    background: QQ.Rectangle {
        implicitHeight: Theme.menuBarHeight
        color: Theme.chrome
    }
}
