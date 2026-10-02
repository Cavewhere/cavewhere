import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    topPadding: Theme.controlVerticalPadding
    bottomPadding: Theme.controlVerticalPadding
    leftPadding: Theme.fieldHorizontalPadding
    rightPadding: Theme.fieldHorizontalPadding

    color: control.palette.text
    selectionColor: Theme.highlight
    selectedTextColor: control.palette.text
    placeholderTextColor: Theme.fieldPlaceholder
    verticalAlignment: QQ.TextInput.AlignVCenter
    opacity: control.enabled ? 1 : Theme.disabledOpacity

    T.ContextMenu.menu: StyleTextEditingMenu {
        editor: control
    }

    PlaceholderText {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: QQ.Text.ElideRight
        renderType: control.renderType
    }

    background: StyleFieldPanel {
        implicitWidth: Theme.fieldWidth
        implicitHeight: Theme.controlHeight
        hovered: control.hovered
        focused: control.activeFocus
    }
}
