import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.ToolTip {
    id: control

    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: -implicitHeight - Theme.toolTipGap

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: Theme.toolTipMargin
    leftInset: -Theme.popupShadowMargin
    topInset: -Theme.popupShadowMargin
    rightInset: -Theme.popupShadowMargin
    bottomInset: -Theme.popupShadowMargin
    verticalPadding: Theme.toolTipVerticalPadding
    horizontalPadding: Theme.toolTipHorizontalPadding

    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    contentItem: QQ.Text {
        text: control.text
        font: control.font
        wrapMode: QQ.Text.Wrap
        color: Theme.toolTipText
    }

    background: StylePopupPanel {
        color: Theme.toolTipSurface
        borderColor: Theme.toolTipSurface
    }
}
