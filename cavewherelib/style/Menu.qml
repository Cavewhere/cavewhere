import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 0
    leftInset: -Theme.popupShadowMargin
    topInset: -Theme.popupShadowMargin
    rightInset: -Theme.popupShadowMargin
    bottomInset: -Theme.popupShadowMargin
    verticalPadding: Theme.popupPadding
    horizontalPadding: Theme.menuHorizontalPadding
    overlap: Theme.menuOverlap

    delegate: MenuItem { }

    contentItem: QQ.ListView {
        implicitHeight: contentHeight
        // Size the menu to its widest item so short menus stay compact.
        implicitWidth: {
            let widest = 0
            for (let i = 0; i < count; ++i) {
                const item = itemAtIndex(i)
                if (item) {
                    widest = Math.max(widest, item.implicitWidth)
                }
            }
            return widest
        }
        model: control.contentModel
        interactive: QQ.Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex

        T.ScrollIndicator.vertical: ScrollIndicator { }
    }

    background: StylePopupPanel {
        implicitWidth: Theme.menuMinimumWidth + 2 * Theme.popupShadowMargin
        implicitHeight: Theme.controlHeight + 2 * Theme.popupShadowMargin
    }

    T.Overlay.modal: QQ.Rectangle {
        color: Theme.overlayScrim
    }
}
