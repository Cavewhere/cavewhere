import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.Popup {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    leftInset: -Theme.popupShadowMargin
    topInset: -Theme.popupShadowMargin
    rightInset: -Theme.popupShadowMargin
    bottomInset: -Theme.popupShadowMargin
    padding: Theme.dialogPadding

    background: StylePopupPanel { }

    T.Overlay.modal: QQ.Rectangle {
        color: Theme.overlayScrim
    }
}
