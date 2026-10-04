import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.TextArea {
    id: control

    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding,
                            implicitBackgroundWidth + leftInset + rightInset,
                            placeholder.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(contentHeight + topPadding + bottomPadding,
                             implicitBackgroundHeight + topInset + bottomInset,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    // The sides follow padding, as in Basic, so a caller's padding: 0 still
    // reads as plain text.
    padding: Theme.textAreaVerticalPadding
    leftPadding: padding + Theme.fieldHorizontalPadding - Theme.textAreaVerticalPadding
    rightPadding: padding + Theme.fieldHorizontalPadding - Theme.textAreaVerticalPadding

    color: control.palette.active.text
    selectionColor: Theme.highlight
    selectedTextColor: control.palette.active.text
    placeholderTextColor: Theme.fieldPlaceholder
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
        hovered: control.hovered
        focused: control.activeFocus
    }
}
