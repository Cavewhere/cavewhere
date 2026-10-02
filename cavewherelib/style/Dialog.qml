import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    leftInset: -Theme.popupShadowMargin
    topInset: -Theme.popupShadowMargin
    rightInset: -Theme.popupShadowMargin
    bottomInset: -Theme.popupShadowMargin
    padding: Theme.dialogPadding

    background: StylePopupPanel {
        radius: Theme.panelRadius
    }

    header: Label {
        text: control.title
        visible: control.title.length > 0 && parent?.parent === T.Overlay.overlay
        elide: QQ.Text.ElideRight
        font.bold: true
        padding: Theme.dialogPadding
        bottomPadding: 0
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    T.Overlay.modal: QQ.Rectangle {
        color: Theme.overlayScrim
    }
}
